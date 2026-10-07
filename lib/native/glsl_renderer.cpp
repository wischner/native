//
// Executes original GLSL multipass image packages through private OpenGL.
// Programs, targets, previous-frame textures and context belong to the peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "glsl_shader.h"
#include "glsl_api.h"
#include <algorithm>
#include <cstring>
#include <regex>
#include <stdexcept>

namespace
{
    using namespace native::detail;
    constexpr unsigned texture_2d=0x0de1, rgba8=0x8058, rgba_format=0x1908,
        unsigned_byte=0x1401, float_type=0x1406, array_buffer=0x8892,
        uniform_buffer=0x8a11, framebuffer=0x8d40, color_attachment=0x8ce0;
    constexpr unsigned invalid_index=0xffffffff;
    struct target {unsigned texture=0,width=0,height=0;};
    struct program {
        unsigned id=0,buffer=0;
        int block_size=0;
    };
    // Change only GLSL interface/version syntax for OpenGL 3.3 portability.
    // Effect expressions, constants, control flow and texture operations stay intact.
    std::string compatible(const std::string &source) {
        auto text=std::regex_replace(source,std::regex("#version[ \\t]+440"),"#version 330\n#define QSHADER_VIEW_COUNT 1");
        text=std::regex_replace(text,std::regex("layout\\(location[ \\t]*=[^)]*\\)[ \\t]*"),"");
        text=std::regex_replace(text,std::regex("layout\\(std140,[ \\t]*binding[ \\t]*=[^)]*\\)"),"layout(std140)");
        return std::regex_replace(text,std::regex("layout\\(binding[ \\t]*=[^)]*\\)[ \\t]*"),"");
    }
    class renderer final : public glsl_renderer {
    public:
        explicit renderer(const shader_package_data &package)
            :context(make_glsl_context()),passes(package.glsl_passes),assets(package.glsl_textures) {
            context->enter();
            try {
                gl=std::make_unique<glsl_api>(*context);
                const char *description=reinterpret_cast<const char*>(gl->GetString(0x1f01));
                const std::string device=description?description:"";
                hardware=device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos&&device.find("Software")==std::string::npos;
                gl->GetIntegerv(0x0d33,&maximum_texture);
                for(const auto &pass:passes)programs.push_back(compile(pass));
                gl->GenFramebuffers(1,&fbo);gl->GenVertexArrays(1,&vao);
                gl->GenBuffers(1,&vertices);gl->BindVertexArray(vao);
                gl->BindBuffer(array_buffer,vertices);
                gl->EnableVertexAttribArray(0);gl->EnableVertexAttribArray(1);
                gl->VertexAttribPointer(0,4,float_type,0,6*sizeof(float),nullptr);
                gl->VertexAttribPointer(1,2,float_type,0,6*sizeof(float),reinterpret_cast<void*>(4*sizeof(float)));
                current.resize(passes.size());history.resize(passes.size());
                for(const auto &asset:assets) {
                    target t;allocate(t,asset.width,asset.height,asset.pixels.data());textures.push_back(t);
                }
                context->leave();
            }catch(...){cleanup();context->leave();throw;}
        }
        ~renderer() override {
            try{context->enter();cleanup();context->leave();}catch(...){}
        }
        bool accelerated() const override{return hardware;}
        std::unique_ptr<native::img> render(const native::img &image,
            const std::map<std::string,native::shader_value,std::less<>> &parameters,
            unsigned vw,unsigned vh,double source_time,double elapsed,double delta,bool reset) override {
            if(!vw||!vh||vw>4096||vh>4096)throw std::invalid_argument("GLSL viewport exceeds 4096 pixels.");
            std::uint64_t bytes=std::uint64_t(image.w())*image.h()*4;
            std::vector<std::pair<unsigned,unsigned>> extents;
            for(const auto &p:passes) {
                auto extent=[&](unsigned n) {const unsigned x=(n+p.reduction-1)/p.reduction+2*p.padding;return (x+p.alignment-1)/p.alignment*p.alignment;};
                unsigned w=extent(p.viewport?vw:image.w()),h=extent(p.viewport?vh:image.h());
                if(w>4096||h>4096||w>unsigned(maximum_texture)||h>unsigned(maximum_texture))throw std::invalid_argument("GLSL target exceeds the device limit.");
                bytes+=std::uint64_t(w)*h*4*(p.history?2:1);extents.emplace_back(w,h);
            }
            if(bytes>256u*1024u*1024u)throw std::invalid_argument("GLSL targets exceed 256 MiB.");
            context->enter();
            try {
                allocate(source,image.w(),image.h(),image.pixels());
                for(unsigned i=0;i<passes.size();++i) {
                    const auto [w,h]=extents[i];const auto &p=passes[i];
                    if(reset&&history[i].texture){gl->DeleteTextures(1,&history[i].texture);history[i]={};}
                    allocate(current[i],w,h,nullptr);
                    if(p.history)allocate(history[i],w,h,nullptr);
                    gl->BindFramebuffer(framebuffer,fbo);
                    gl->FramebufferTexture2D(framebuffer,color_attachment,texture_2d,current[i].texture,0);
                    if(gl->CheckFramebufferStatus(framebuffer)!=0x8cd5)throw std::runtime_error("GLSL framebuffer is incomplete.");
                    gl->Viewport(0,0,w,h);auto &program=programs[i];gl->UseProgram(program.id);
                    update_uniforms(program,p,parameters,image.w(),image.h(),vw,vh,w,h,source_time,elapsed,delta);
                    for(unsigned unit=0;unit<p.samplers.size();++unit) {
                        const auto &sampler=p.samplers[unit];unsigned texture=source.texture;
                        if(sampler.input>=100)texture=textures[sampler.input-100].texture;
                        else if(sampler.input>=0)texture=current[sampler.input].texture;
                        else if(sampler.input< -1)texture=history[-std::int64_t(sampler.input)-2].texture;
                        gl->ActiveTexture(0x84c0+unit);gl->BindTexture(texture_2d,texture);
                        const int wrap=sampler.wrap==1?0x2901:sampler.wrap==2?0x812d:0x812f;
                        gl->TexParameteri(texture_2d,0x2802,wrap);gl->TexParameteri(texture_2d,0x2803,wrap);
                        gl->Uniform1i(gl->GetUniformLocation(program.id,sampler.name.c_str()),unit);
                    }
                    auto r=p.source_rect;
                    if(p.uv_mapping==1) {
                        const float bw=float(p.viewport?vw:image.w())/p.reduction;
                        const float bh=float(p.viewport?vh:image.h())/p.reduction;
                        r={-float(p.padding)/bw,-float(p.padding)/bh,1+2*p.padding/bw,1+2*p.padding/bh};
                    } else if(p.uv_mapping==2) {
                        const float px=r[0],py=r[1];
                        r={px/(w+2*px),py/(h+2*py),w/(w+2*px),h/(h+2*py)};
                    }
                    const float quad[]={-1,-1,0,1,r[0],r[1], 1,-1,0,1,r[0]+r[2],r[1], -1,1,0,1,r[0],r[1]+r[3], 1,1,0,1,r[0]+r[2],r[1]+r[3]};
                    gl->BindVertexArray(vao);gl->BindBuffer(array_buffer,vertices);
                    gl->BufferData(array_buffer,sizeof(quad),quad,0x88e0);
                    gl->DrawArrays(0x0005,0,4);
                    if(gl->GetError()!=0)throw std::runtime_error("Original GLSL draw failed.");
                }
                const auto &last=current.back();
                auto output=std::make_unique<native::img>(last.width,last.height);
                gl->ReadPixels(0,0,last.width,last.height,rgba_format,unsigned_byte,output->pixels());
                if(gl->GetError()!=0)throw std::runtime_error("GLSL image readback failed.");
                for(unsigned i=0;i<passes.size();++i)if(passes[i].history)std::swap(current[i],history[i]);
                context->leave();return output;
            }catch(...){context->leave();throw;}
        }
    private:
        unsigned shader(unsigned type,const std::string &source) {
            unsigned id=gl->CreateShader(type);const auto text=compatible(source);const char *data=text.c_str();
            gl->ShaderSource(id,1,&data,nullptr);gl->CompileShader(id);int valid=0;gl->GetShaderiv(id,0x8b81,&valid);
            if(!valid){char log[8192]{};gl->GetShaderInfoLog(id,sizeof(log),nullptr,log);gl->DeleteShader(id);throw std::runtime_error(std::string("Original GLSL compilation: ")+log);}
            return id;
        }
        program compile(const glsl_pass &pass) {
            program p;unsigned vs=shader(0x8b31,pass.vertex),fs=0;
            try {
                fs=shader(0x8b30,pass.fragment);p.id=gl->CreateProgram();gl->AttachShader(p.id,vs);gl->AttachShader(p.id,fs);
                gl->BindAttribLocation(p.id,0,"qt_Vertex");gl->BindAttribLocation(p.id,1,"qt_MultiTexCoord0");gl->LinkProgram(p.id);
                int valid=0;gl->GetProgramiv(p.id,0x8b82,&valid);
                if(!valid){char log[8192]{};gl->GetProgramInfoLog(p.id,sizeof(log),nullptr,log);throw std::runtime_error(pass.name+" GLSL link: "+log);}
                unsigned block=invalid_index;
                for(const char *name:{"ubuf","buf","qt_buf"}){block=gl->GetUniformBlockIndex(p.id,name);if(block!=invalid_index)break;}
                if(block==invalid_index)throw std::runtime_error("GLSL pass requires one uniform block.");
                gl->GetActiveUniformBlockiv(p.id,block,0x8a40,&p.block_size);
                if(p.block_size<=0||p.block_size>4096)throw std::runtime_error("GLSL uniform block exceeds 4096 bytes.");
                gl->UniformBlockBinding(p.id,block,0);gl->GenBuffers(1,&p.buffer);
                gl->DeleteShader(vs);gl->DeleteShader(fs);return p;
            }catch(...){gl->DeleteShader(vs);if(fs)gl->DeleteShader(fs);if(p.id)gl->DeleteProgram(p.id);throw;}
        }
        int offset(unsigned id,const std::string &name) {
            for(const char *prefix:{"","ubuf.","buf.","qt_buf.","qt_ubuf."}) {
                const auto key=std::string(prefix)+name;const char *n=key.c_str();unsigned index=invalid_index;
                gl->GetUniformIndices(id,1,&n,&index);if(index==invalid_index)continue;
                int result=-1;gl->GetActiveUniformsiv(id,1,&index,0x8a3b,&result);return result;
            }
            return -1;
        }
        void update_uniforms(program &program,const glsl_pass &pass,
            const std::map<std::string,native::shader_value,std::less<>> &parameters,
            unsigned sw,unsigned sh,unsigned vw,unsigned vh,unsigned w,unsigned h,
            double source_time,double elapsed,double delta) {
            std::vector<unsigned char> bytes(program.block_size,0);
            auto put=[&](const std::string &name,const float *data,unsigned count) {
                int at=offset(program.id,name);if(at<0)return;
                if(std::size_t(at)+count*sizeof(float)>bytes.size())throw std::runtime_error("GLSL uniform binding exceeds its block.");
                std::memcpy(bytes.data()+at,data,count*sizeof(float));
            };
            const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float opacity=1;
            put("qt_Matrix",identity,16);put("qt_Opacity",&opacity,1);
            for(const auto &u:pass.uniforms) {
                auto v=u.value;
                if(u.binding=="elapsed")v.fill(float(elapsed));
                else if(u.binding=="delta")v.fill(float(delta));
                else if(u.binding=="previous_time")v.fill(float(elapsed-delta));
                else if(u.binding=="source_time")v.fill(float(source_time));
                else if(u.binding=="source_size")v={float(sw),float(sh),0,0};
                else if(u.binding=="viewport_size")v={float(vw),float(vh),0,0};
                else if(u.binding=="pass_texel")v.fill(u.value[0]/w+u.value[1]/h);
                else if(u.binding!="value")v=shader_components(parameters.at(u.binding));
                put(u.name,v.data(),u.components);
            }
            gl->BindBuffer(uniform_buffer,program.buffer);gl->BufferData(uniform_buffer,bytes.size(),bytes.data(),0x88e0);
            gl->BindBufferBase(uniform_buffer,0,program.buffer);
        }
        void allocate(target &t,unsigned w,unsigned h,const native::rgba *pixels) {
            if(t.texture&&t.width==w&&t.height==h&&!pixels)return;
            if(!t.texture)gl->GenTextures(1,&t.texture);
            gl->BindTexture(texture_2d,t.texture);
            gl->TexParameteri(texture_2d,0x2801,0x2601);gl->TexParameteri(texture_2d,0x2800,0x2601);
            std::vector<native::rgba> zeros;
            if(!pixels){zeros.resize(std::size_t(w)*h,native::rgba(0,0,0,0));pixels=zeros.data();}
            gl->TexImage2D(texture_2d,0,rgba8,w,h,0,rgba_format,unsigned_byte,pixels);t.width=w;t.height=h;
        }
        void cleanup() noexcept {
            if(!gl)return;
            for(auto &p:programs){if(p.buffer)gl->DeleteBuffers(1,&p.buffer);if(p.id)gl->DeleteProgram(p.id);}
            for(auto *list:{&current,&history,&textures})for(auto &t:*list)if(t.texture)gl->DeleteTextures(1,&t.texture);
            if(source.texture)gl->DeleteTextures(1,&source.texture);
            if(fbo)gl->DeleteFramebuffers(1,&fbo);if(vao)gl->DeleteVertexArrays(1,&vao);if(vertices)gl->DeleteBuffers(1,&vertices);
        }
        std::unique_ptr<glsl_context> context;std::unique_ptr<glsl_api> gl;
        std::vector<glsl_pass> passes;std::vector<glsl_texture> assets;
        std::vector<program> programs;std::vector<target> current,history,textures;
        target source;unsigned fbo=0,vao=0,vertices=0;int maximum_texture=0;bool hardware=false;
    };
}
namespace native::detail
{
    std::unique_ptr<glsl_renderer> make_glsl_renderer(const shader_package_data &package){return std::make_unique<renderer>(package);}
}
