//
// native_artist.cpp
// 
// Encapsulate X11 drawing.
// TODO:
//  Use Cairo.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 17.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    void artist::draw_line(color c, pt p1, pt p2) const {
    }

    void artist::draw_rect(color c, rct r) const {   
    }

    void artist::fill_rect(color c, rct r) const {   
        // 1000 mile walk to create a simple RGB color.
        Colormap cmap=DefaultColormap(canvas_.d,DefaultScreen(canvas_.d));    
        XColor xc;
        xc.red=c.r * 0xff; 
        xc.green=c.g * 0xff; 
        xc.blue=c.b * 0xff;
        xc.flags = DoRed | DoGreen | DoBlue;
        XAllocColor(canvas_.d, cmap, &xc);
        // Set pen.
        XSetForeground(canvas_.d, canvas_.gc, xc.pixel);
        // And fill rect.
        XFillRectangle( canvas_.d, canvas_.w, canvas_.gc, r.x, r.y, r.w, r.h );
    }

    void artist::draw_raster(const raster& rst, pt p) const {
        // Get the visual.
        Visual *visual=DefaultVisual(canvas_.d, DefaultScreen(canvas_.d));
        // Create the iage.
        XImage* img=XCreateImage(
            canvas_.d, 
            visual, 
            24, 
            ZPixmap, 
            0, 
            (char*)rst.raw(),
            rst.width(),
            rst.height(),
            32,
            0);
        // Draw it!
        XPutImage(
            canvas_.d, 
            canvas_.w, 
            canvas_.gc, 
            img, 
            0, 0, p.x, p.y, 
            rst.width(), rst.height());
        // We don't want our raster object wildly released by XDestroyImage.
        img->data=NULL;
        // Destroy the image.
        XDestroyImage(img);
    }
//{{END.DEF}}
}