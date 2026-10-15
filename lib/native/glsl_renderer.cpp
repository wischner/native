//
// Executes original GLSL multipass image packages through private OpenGL.
// Programs, targets, previous-frame textures and context belong to the peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "glsl_api.h"
#include "glsl_shader.h"
#include <algorithm>
#include <cstring>
#include <regex>
#include <stdexcept>

namespace
{
    using namespace native::detail;
    constexpr unsigned texture_2d = 0x0de1, rgba8 = 0x8058,
                       rgba_format = 0x1908, unsigned_byte = 0x1401,
                       float_type = 0x1406, array_buffer = 0x8892,
                       uniform_buffer = 0x8a11, framebuffer = 0x8d40,
                       color_attachment = 0x8ce0;
    constexpr unsigned invalid_index = 0xffffffff;
    struct target
    {
        unsigned texture = 0, width = 0, height = 0;
    };
    struct program
    {
        unsigned id = 0, buffer = 0;
        int block_size = 0;
    };
    // Change only GLSL interface/version syntax for OpenGL 3.3 portability.
    // Effect expressions, constants, control flow and texture operations stay
    // intact.
    std::string compatible(const std::string &source) {
        auto text =
            std::regex_replace(source, std::regex("#version[ \\t]+440"),
                               "#version 330\n#define QSHADER_VIEW_COUNT 1");
        text = std::regex_replace(
            text, std::regex("layout\\(location[ \\t]*=[^)]*\\)[ \\t]*"), "");
        text = std::regex_replace(
            text,
            std::regex("layout\\(std140,[ \\t]*binding[ \\t]*=[^)]*\\)"),
            "layout(std140)");
        return std::regex_replace(
            text, std::regex("layout\\(binding[ \\t]*=[^)]*\\)[ \\t]*"), "");
    }
    class renderer final : public glsl_renderer
    {
    public:
        explicit renderer(const shader_package_data &package)
            : context(make_glsl_context()), passes(package.glsl_passes) {
            context->enter();
            try {
                gl = std::make_unique<glsl_api>(*context);
                const char *description =
                    reinterpret_cast<const char *>(gl->get_string(0x1f01));
                const std::string device = description ? description : "";
                hardware = device.find("llvmpipe") == std::string::npos &&
                           device.find("softpipe") == std::string::npos &&
                           device.find("Software") == std::string::npos;
                gl->get_integerv(0x0d33, &maximum_texture);
                programs.reserve(passes.size());
                for (const auto &pass : passes)
                    programs.push_back(compile(pass));
                gl->gen_framebuffers(1, &fbo);
                gl->gen_vertex_arrays(1, &vao);
                gl->gen_buffers(1, &vertices);
                gl->bind_vertex_array(vao);
                gl->bind_buffer(array_buffer, vertices);
                gl->enable_vertex_attrib_array(0);
                gl->enable_vertex_attrib_array(1);
                gl->vertex_attrib_pointer(0, 4, float_type, 0,
                                          6 * sizeof(float), nullptr);
                gl->vertex_attrib_pointer(
                    1, 2, float_type, 0, 6 * sizeof(float),
                    reinterpret_cast<void *>(4 * sizeof(float)));
                current.resize(passes.size());
                history.resize(passes.size());
                textures.resize(package.glsl_textures.size());
                for (unsigned i = 0; i < textures.size(); ++i) {
                    const auto &asset = package.glsl_textures[i];
                    validate_extent(asset.width, asset.height);
                    allocate(textures[i], asset.width, asset.height,
                             asset.pixels.data());
                }
            } catch (...) {
                cleanup();
                context->leave();
                throw;
            }
            // Never clean up GL resources in the caller's context if
            // restoring that context fails. Teardown releases ownership.
            context->leave();
        }
        ~renderer() override {
            try {
                context->enter();
                cleanup();
                context->leave();
            } catch (...) {
            }
        }
        bool accelerated() const override {
            return hardware;
        }
        std::uint64_t storage_bytes() const override {
            std::uint64_t bytes =
                std::uint64_t(source.width) * source.height * 4;
            // GPU pixels plus the immutable package's CPU pixels; the
            // renderer never keeps a second CPU copy of packaged textures.
            for (const auto &texture : textures)
                bytes += std::uint64_t(texture.width) * texture.height * 8;
            for (const auto *list : {&current, &history})
                for (const auto &t : *list)
                    bytes += std::uint64_t(t.width) * t.height * 4;
            return bytes;
        }
        std::unique_ptr<native::img>
        render(const native::img &image,
               const std::map<std::string, native::shader_value, std::less<>>
                   &parameters,
               unsigned vw, unsigned vh, double source_time, double elapsed,
               double delta, bool reset,
               std::uint64_t retained_bytes = 0) override {
            validate_extent(vw, vh);
            validate_extent(image.w(), image.h());
            // Count retained and candidate targets, packaged pixels, CPU
            // readback/old output, and worst-case zero-initialization upload.
            std::uint64_t bytes = retained_bytes + storage_bytes() +
                                  std::uint64_t(image.w()) * image.h() * 8;
            std::uint64_t largest = 0;
            std::vector<std::pair<unsigned, unsigned>> extents;
            for (const auto &p : passes) {
                auto extent = [&](unsigned n) {
                    const unsigned x =
                        (n + p.reduction - 1) / p.reduction + 2 * p.padding;
                    return (x + p.alignment - 1) / p.alignment * p.alignment;
                };
                unsigned w = extent(p.viewport ? vw : image.w()),
                         h = extent(p.viewport ? vh : image.h());
                validate_extent(w, h);
                const auto allocation = std::uint64_t(w) * h * 4;
                largest = std::max(largest, allocation);
                bytes += allocation * (p.history ? 2 : 1);
                extents.emplace_back(w, h);
            }
            bytes += largest + std::uint64_t(extents.back().first) *
                                   extents.back().second * 8;
            if (bytes > 256u * 1024u * 1024u)
                throw std::invalid_argument("GLSL targets exceed 256 MiB.");
            context->enter();
            std::unique_ptr<native::img> output;
            std::vector<target> fresh_history(passes.size());
            try {
                allocate(source, image.w(), image.h(), image.pixels());
                // Every retained edge exists before the first pass samples
                // it, including edges pointing to later passes. Old history
                // remains untouched until the complete tick and readback
                // succeed.
                for (unsigned i = 0; i < passes.size(); ++i) {
                    const auto [w, h] = extents[i];
                    allocate(current[i], w, h, nullptr);
                    if (passes[i].history &&
                        (reset || !history[i].texture ||
                         history[i].width != w || history[i].height != h))
                        allocate(fresh_history[i], w, h, nullptr);
                }
                for (unsigned i = 0; i < passes.size(); ++i) {
                    const auto [w, h] = extents[i];
                    const auto &p = passes[i];
                    gl->bind_framebuffer(framebuffer, fbo);
                    gl->framebuffer_texture_2d(framebuffer, color_attachment,
                                               texture_2d, current[i].texture,
                                               0);
                    if (gl->check_framebuffer_status(framebuffer) != 0x8cd5)
                        throw std::runtime_error(
                            "GLSL framebuffer is incomplete.");
                    gl->viewport(0, 0, w, h);
                    auto &program = programs[i];
                    gl->use_program(program.id);
                    update_uniforms(program, p, parameters, image.w(),
                                    image.h(), vw, vh, w, h, source_time,
                                    elapsed, delta);
                    for (unsigned unit = 0; unit < p.samplers.size();
                         ++unit) {
                        const auto &sampler = p.samplers[unit];
                        unsigned texture = source.texture;
                        if (sampler.input >= 100)
                            texture = textures[sampler.input - 100].texture;
                        else if (sampler.input >= 0)
                            texture = current[sampler.input].texture;
                        else if (sampler.input < -1) {
                            const auto index =
                                -std::int64_t(sampler.input) - 2;
                            texture = fresh_history[index].texture
                                          ? fresh_history[index].texture
                                          : history[index].texture;
                        }
                        gl->active_texture(0x84c0 + unit);
                        gl->bind_texture(texture_2d, texture);
                        const int wrap = sampler.wrap == 1   ? 0x2901
                                         : sampler.wrap == 2 ? 0x812d
                                                             : 0x812f;
                        gl->tex_parameteri(texture_2d, 0x2802, wrap);
                        gl->tex_parameteri(texture_2d, 0x2803, wrap);
                        gl->uniform1i(gl->get_uniform_location(
                                          program.id, sampler.name.c_str()),
                                      unit);
                    }
                    auto r = p.source_rect;
                    if (p.uv_mapping == 1) {
                        const float bw =
                            float(p.viewport ? vw : image.w()) / p.reduction;
                        const float bh =
                            float(p.viewport ? vh : image.h()) / p.reduction;
                        r = {-float(p.padding) / bw, -float(p.padding) / bh,
                             1 + 2 * p.padding / bw, 1 + 2 * p.padding / bh};
                    } else if (p.uv_mapping == 2) {
                        const float px = r[0], py = r[1];
                        r = {px / (w + 2 * px), py / (h + 2 * py),
                             w / (w + 2 * px), h / (h + 2 * py)};
                    }
                    const float quad[] = {
                        -1, -1, 0, 1, r[0],        r[1],
                        1,  -1, 0, 1, r[0] + r[2], r[1],
                        -1, 1,  0, 1, r[0],        r[1] + r[3],
                        1,  1,  0, 1, r[0] + r[2], r[1] + r[3]};
                    gl->bind_vertex_array(vao);
                    gl->bind_buffer(array_buffer, vertices);
                    gl->buffer_data(array_buffer, sizeof(quad), quad, 0x88e0);
                    gl->draw_arrays(0x0005, 0, 4);
                    if (gl->get_error() != 0)
                        throw std::runtime_error(
                            "Original GLSL draw failed.");
                }
                const auto &last = current.back();
                output =
                    std::make_unique<native::img>(last.width, last.height);
                gl->read_pixels(0, 0, last.width, last.height, rgba_format,
                                unsigned_byte, output->pixels());
                if (gl->get_error() != 0)
                    throw std::runtime_error("GLSL image readback failed.");
                for (unsigned i = 0; i < passes.size(); ++i)
                    if (passes[i].history) {
                        if (fresh_history[i].texture)
                            std::swap(history[i], fresh_history[i]);
                        std::swap(current[i], history[i]);
                    }
                release_targets(fresh_history);
            } catch (...) {
                release_targets(fresh_history);
                context->leave();
                throw;
            }
            context->leave();
            return output;
        }

    private:
        // Validate source, viewport, packaged and computed target dimensions.
        void validate_extent(unsigned w, unsigned h) const {
            if (!w || !h || w > 4096 || h > 4096 ||
                w > unsigned(maximum_texture) ||
                h > unsigned(maximum_texture))
                throw std::invalid_argument(
                    "GLSL image exceeds the extent/device limit.");
        }
        // Release temporary history on either commit or failed execution.
        void release_targets(std::vector<target> &targets) noexcept {
            for (auto &t : targets) {
                if (t.texture)
                    gl->delete_textures(1, &t.texture);
                t = {};
            }
        }
        unsigned shader(unsigned type, const std::string &source) {
            const auto text = compatible(source);
            unsigned id = gl->create_shader(type);
            const char *data = text.c_str();
            gl->shader_source(id, 1, &data, nullptr);
            gl->compile_shader(id);
            int valid = 0;
            gl->get_shaderiv(id, 0x8b81, &valid);
            if (!valid) {
                char log[8192]{};
                gl->get_shader_info_log(id, sizeof(log), nullptr, log);
                gl->delete_shader(id);
                throw std::runtime_error(
                    std::string("Original GLSL compilation: ") + log);
            }
            return id;
        }
        program compile(const glsl_pass &pass) {
            program p;
            unsigned vs = shader(0x8b31, pass.vertex), fs = 0;
            try {
                fs = shader(0x8b30, pass.fragment);
                p.id = gl->create_program();
                gl->attach_shader(p.id, vs);
                gl->attach_shader(p.id, fs);
                gl->bind_attrib_location(p.id, 0, "qt_Vertex");
                gl->bind_attrib_location(p.id, 1, "qt_MultiTexCoord0");
                gl->link_program(p.id);
                int valid = 0;
                gl->get_programiv(p.id, 0x8b82, &valid);
                if (!valid) {
                    char log[8192]{};
                    gl->get_program_info_log(p.id, sizeof(log), nullptr, log);
                    throw std::runtime_error(pass.name +
                                             " GLSL link: " + log);
                }
                unsigned block = invalid_index;
                for (const char *name : {"ubuf", "buf", "qt_buf"}) {
                    block = gl->get_uniform_block_index(p.id, name);
                    if (block != invalid_index)
                        break;
                }
                if (block == invalid_index)
                    throw std::runtime_error(
                        "GLSL pass requires one uniform block.");
                gl->get_active_uniform_blockiv(p.id, block, 0x8a40,
                                               &p.block_size);
                if (p.block_size <= 0 || p.block_size > 4096)
                    throw std::runtime_error(
                        "GLSL uniform block exceeds 4096 bytes.");
                gl->uniform_block_binding(p.id, block, 0);
                gl->gen_buffers(1, &p.buffer);
                gl->delete_shader(vs);
                gl->delete_shader(fs);
                return p;
            } catch (...) {
                gl->delete_shader(vs);
                if (fs)
                    gl->delete_shader(fs);
                if (p.id)
                    gl->delete_program(p.id);
                throw;
            }
        }
        int offset(unsigned id, const std::string &name) {
            for (const char *prefix :
                 {"", "ubuf.", "buf.", "qt_buf.", "qt_ubuf."}) {
                const auto key = std::string(prefix) + name;
                const char *n = key.c_str();
                unsigned index = invalid_index;
                gl->get_uniform_indices(id, 1, &n, &index);
                if (index == invalid_index)
                    continue;
                int result = -1;
                gl->get_active_uniformsiv(id, 1, &index, 0x8a3b, &result);
                return result;
            }
            return -1;
        }
        void update_uniforms(program &program, const glsl_pass &pass,
                             const std::map<std::string, native::shader_value,
                                            std::less<>> &parameters,
                             unsigned sw, unsigned sh, unsigned vw,
                             unsigned vh, unsigned w, unsigned h,
                             double source_time, double elapsed,
                             double delta) {
            std::vector<unsigned char> bytes(program.block_size, 0);
            auto put = [&](const std::string &name, const float *data,
                           unsigned count) {
                int at = offset(program.id, name);
                if (at < 0)
                    return;
                if (std::size_t(at) + count * sizeof(float) > bytes.size())
                    throw std::runtime_error(
                        "GLSL uniform binding exceeds its block.");
                std::memcpy(bytes.data() + at, data, count * sizeof(float));
            };
            const float identity[] = {1, 0, 0, 0, 0, 1, 0, 0,
                                      0, 0, 1, 0, 0, 0, 0, 1};
            float opacity = 1;
            put("qt_Matrix", identity, 16);
            put("qt_Opacity", &opacity, 1);
            for (const auto &u : pass.uniforms) {
                auto v = u.value;
                if (u.binding == "elapsed")
                    v.fill(float(elapsed));
                else if (u.binding == "delta")
                    v.fill(float(delta));
                else if (u.binding == "previous_time")
                    v.fill(float(elapsed - delta));
                else if (u.binding == "source_time")
                    v.fill(float(source_time));
                else if (u.binding == "source_size")
                    v = {float(sw), float(sh), 0, 0};
                else if (u.binding == "viewport_size")
                    v = {float(vw), float(vh), 0, 0};
                else if (u.binding == "pass_texel")
                    v.fill(u.value[0] / w + u.value[1] / h);
                else if (u.binding != "value")
                    v = shader_components(parameters.at(u.binding));
                put(u.name, v.data(), u.components);
            }
            gl->bind_buffer(uniform_buffer, program.buffer);
            gl->buffer_data(uniform_buffer, bytes.size(), bytes.data(),
                            0x88e0);
            gl->bind_buffer_base(uniform_buffer, 0, program.buffer);
        }
        void allocate(target &t, unsigned w, unsigned h,
                      const native::rgba *pixels) {
            if (t.texture && t.width == w && t.height == h && !pixels)
                return;
            if (!t.texture)
                gl->gen_textures(1, &t.texture);
            gl->bind_texture(texture_2d, t.texture);
            gl->tex_parameteri(texture_2d, 0x2801, 0x2601);
            gl->tex_parameteri(texture_2d, 0x2800, 0x2601);
            std::vector<native::rgba> zeros;
            if (!pixels) {
                zeros.resize(std::size_t(w) * h, native::rgba(0, 0, 0, 0));
                pixels = zeros.data();
            }
            gl->tex_image_2d(texture_2d, 0, rgba8, w, h, 0, rgba_format,
                             unsigned_byte, pixels);
            if (gl->get_error() != 0) {
                t.width = t.height = 0;
                throw std::runtime_error("GLSL texture allocation failed.");
            }
            t.width = w;
            t.height = h;
        }
        void cleanup() noexcept {
            if (!gl)
                return;
            for (auto &p : programs) {
                if (p.buffer)
                    gl->delete_buffers(1, &p.buffer);
                if (p.id)
                    gl->delete_program(p.id);
            }
            for (auto *list : {&current, &history, &textures})
                for (auto &t : *list)
                    if (t.texture)
                        gl->delete_textures(1, &t.texture);
            if (source.texture)
                gl->delete_textures(1, &source.texture);
            if (fbo)
                gl->delete_framebuffers(1, &fbo);
            if (vao)
                gl->delete_vertex_arrays(1, &vao);
            if (vertices)
                gl->delete_buffers(1, &vertices);
        }
        std::unique_ptr<glsl_context> context;
        std::unique_ptr<glsl_api> gl;
        std::vector<glsl_pass> passes;
        std::vector<program> programs;
        std::vector<target> current, history, textures;
        target source;
        unsigned fbo = 0, vao = 0, vertices = 0;
        int maximum_texture = 0;
        bool hardware = false;
    };
} // namespace
namespace native::detail
{
    std::unique_ptr<glsl_renderer>
    make_glsl_renderer(const shader_package_data &package) {
        return std::make_unique<renderer>(package);
    }
} // namespace native::detail
