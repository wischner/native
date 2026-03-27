#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

namespace sdl {

static int text_width_est(const std::string &s)
{
    return static_cast<int>(s.size()) * 8 + 16;
}

static const int ITEM_H  = 20;
static const int POPUP_W = 180;

// ---------------------------------------------------------------------------
// Simple text rendering (uses SDL2_ttf when available)
// ---------------------------------------------------------------------------

static void draw_text_simple(SDL_Renderer *r, const std::string &text, int x, int y, SDL_Color col)
{
#ifdef HAVE_SDL2_TTF
    auto *fh = sdl::font_bindings.from_a(native::font_t::stock(native::font_role::system).id());
    if (fh && fh->ttf_font)
    {
        SDL_Surface *surf = TTF_RenderText_Solid(fh->ttf_font, text.c_str(), col);
        if (surf)
        {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
            if (tex)
            {
                SDL_Rect dst = {x, y, surf->w, surf->h};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }
#else
    // Without TTF, draw a simple pixel row as placeholder
    (void)r; (void)text; (void)x; (void)y; (void)col;
#endif
}

// ---------------------------------------------------------------------------
// Public render_menu — called from app.cpp render loop
// ---------------------------------------------------------------------------

void render_menu(sdl2menu *m, SDL_Renderer *r, int win_w, int /*win_h*/)
{
    if (!m || !r) return;

    // Draw bar background
    SDL_SetRenderDrawColor(r, 212, 208, 200, 255);
    SDL_Rect bar_rect = {0, 0, win_w, MENU_BAR_H};
    SDL_RenderFillRect(r, &bar_rect);

    // Top and bottom border lines
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawLine(r, 0, 0, win_w, 0);
    SDL_SetRenderDrawColor(r, 64, 64, 64, 255);
    SDL_RenderDrawLine(r, 0, MENU_BAR_H - 1, win_w, MENU_BAR_H - 1);

    // Draw menu titles
    SDL_Color black_col = {0, 0, 0, 255};
    SDL_Color white_col = {255, 255, 255, 255};
    for (int i = 0; i < static_cast<int>(m->tops.size()); ++i)
    {
        auto &top = m->tops[i];
        if (m->open_idx == i)
        {
            SDL_SetRenderDrawColor(r, 0, 0, 128, 255);
            SDL_Rect sel = {top.x0, 1, top.x1 - top.x0, MENU_BAR_H - 2};
            SDL_RenderFillRect(r, &sel);
            draw_text_simple(r, top.title, top.x0 + 8, (MENU_BAR_H - 14) / 2, white_col);
        }
        else
        {
            draw_text_simple(r, top.title, top.x0 + 8, (MENU_BAR_H - 14) / 2, black_col);
        }
    }

    // Draw open popup
    if (m->open_idx >= 0 && m->open_idx < static_cast<int>(m->tops.size()))
    {
        auto &top   = m->tops[m->open_idx];
        int popup_h = static_cast<int>(top.items.size()) * ITEM_H + 2;

        SDL_SetRenderDrawColor(r, 212, 208, 200, 255);
        SDL_Rect pop = {m->popup_x, m->popup_y, POPUP_W, popup_h};
        SDL_RenderFillRect(r, &pop);

        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderDrawRect(r, &pop);

        for (int i = 0; i < static_cast<int>(top.items.size()); ++i)
        {
            int iy = m->popup_y + 1 + i * ITEM_H;
            draw_text_simple(r, top.items[i].second, m->popup_x + 8, iy + (ITEM_H - 14) / 2, black_col);
        }
    }
}

// ---------------------------------------------------------------------------
// handle_menu_click — returns true if click was consumed by menu
// ---------------------------------------------------------------------------

bool handle_menu_click(sdl2menu *m, int x, int y, SDL_Renderer * /*r*/, int /*win_w*/)
{
    if (!m) return false;

    // Click in menu bar area
    if (y >= 0 && y < MENU_BAR_H)
    {
        for (int i = 0; i < static_cast<int>(m->tops.size()); ++i)
        {
            auto &top = m->tops[i];
            if (x >= top.x0 && x < top.x1)
            {
                if (m->open_idx == i)
                    m->open_idx = -1;
                else
                {
                    m->open_idx = i;
                    m->popup_x  = top.x0;
                    m->popup_y  = MENU_BAR_H;
                }
                return true;
            }
        }
        // Click in bar but not on a title — close any open menu
        m->open_idx = -1;
        return true;
    }

    // Click in open popup area
    if (m->open_idx >= 0)
    {
        auto &top   = m->tops[m->open_idx];
        int popup_h = static_cast<int>(top.items.size()) * ITEM_H + 2;

        if (x >= m->popup_x && x < m->popup_x + POPUP_W &&
            y >= m->popup_y && y < m->popup_y + popup_h)
        {
            int item_idx = (y - m->popup_y) / ITEM_H;
            if (item_idx >= 0 && item_idx < static_cast<int>(top.items.size()))
            {
                int item_id  = top.items[item_idx].first;
                m->open_idx  = -1;
                if (m->owner)
                    m->owner->on_menu.emit(item_id);
                return true;
            }
        }
        // Click outside popup — close it
        m->open_idx = -1;
        return false;
    }

    return false;
}

} // namespace sdl

// ---------------------------------------------------------------------------
// native::main_menu platform implementation for SDL2
// ---------------------------------------------------------------------------

namespace native {

main_menu::~main_menu()
{
    if (!_id) return;
    auto *m = sdl::menu_bindings.from_a(_id);
    if (m) delete m;
    sdl::menu_bindings.unregister_by_a(_id);
    _id = 0;
}

void main_menu::attach(app_wnd &owner)
{
    if (_id || _tops.empty()) return;
    _owner = &owner;

    auto *sm  = new sdl::sdl2menu();
    sm->owner = &owner;

    int x = 0;
    for (const auto &top : _tops)
    {
        sdl::sdl2menu::top_entry te;
        te.title = top.title;
        te.x0    = x;
        te.x1    = x + sdl::text_width_est(top.title);
        x        = te.x1;
        for (const auto &item : top.items)
            te.items.push_back({item.id, item.label});
        sm->tops.push_back(std::move(te));
    }

    _id = next_id();
    sdl::menu_bindings.register_pair(_id, sm);
}

} // namespace native
