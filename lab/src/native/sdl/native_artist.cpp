//
// native_artist.cpp
// 
// Encapsulate basic SDL drawing.
// TODO:
//  Everything.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    void artist::draw_line(color c, pt p1, pt p2) const {
        ::SDL_SetRenderDrawColor(canvas_, c.r, c.g, c.b, c.a);
        ::SDL_RenderDrawLine(canvas_,p1.x, p1.y, p2.x, p2.y);
    }

    void artist::draw_rect(color c, rct r) const {
        SDL_Rect rdst={ r.x, r.y, r.w, r.h};
        ::SDL_SetRenderDrawColor(canvas_, c.r, c.g, c.b, c.a);
        ::SDL_RenderDrawRect(canvas_,&rdst);
    }

    void artist::fill_rect(color c, rct r) const {
        SDL_Rect rdst={ r.x, r.y, r.w, r.h};
        ::SDL_SetRenderDrawColor(canvas_, c.r, c.g, c.b, c.a);
        ::SDL_RenderFillRect(canvas_,&rdst);
    }

    void artist::draw_raster(const raster& rst, pt p) const {
        
        // Create a surface.
        SDL_Surface *surface=SDL_CreateRGBSurfaceFrom(
            rst.raw(),
            rst.width(),
            rst.height(),
            32,
            4*rst.width(),
            0,0,0,0
        );

        // Get surface to texture.
        SDL_Texture *texture = SDL_CreateTextureFromSurface(canvas_, surface);

        // Draw on window.
        SDL_Rect rsrc={ 0, 0, rst.width(), rst.height() };
        SDL_Rect rdst={ p.x, p.y, rst.width(), rst.height()};
        SDL_RenderCopy( 
            canvas_, 
            texture, 
            &rsrc, 
            &rdst
        );

        // And free surface and texture.
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
//{{END.DEF}}
}