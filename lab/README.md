![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]
 
 # Welcome to nice

by Tomaz Stih

Nice is a modern C++ micro framework for building desktop applications
that started as an excercise in modern C++.

> Nice is currently under heavy development; unstable, and poorly documented. 
> It will live up to expectations. Just not now. Thank you for your patience.

The philosophy of nice is to:
 * keep it simple; if it is not, refactor it until it is,
 * make it micro; an individual must to be able to fully understand it,
 * hide native API complexities and expose nice interfaces: hence the name
 * enable creating derived classes on top of existing classes
 * use best of C++20
 * use single header
 * support native C++ multithreading 
 * support modern layout managers
 * superfast, and supersmall; load instantly and use kilobytes, not megabytes!
 * single exe
 * multiplatform.

# Hello nice

Here's the Hello World application in nice:

~~~cpp
#include "nice.hpp"

void program()
{
    nice::app::run(app_wnd("Hello World!"));
}
~~~

And here's the Hello Paint application in nice:

~~~cpp
#include "nice.hpp"

class main_wnd : public nice::app_wnd {
public:
    main_wnd() : app_wnd("Hello paint!") {
        paint.connect(this, &main_wnd::on_paint);
    }
private:
    // Paint handler, draws rectangle.
    void on_paint(const nice::artist& a) {
        a.draw_rect({ 255,0,0 }, { 10,10,100,100 });
    }
};

void program()
{
    nice::app::run(main_wnd());
}
~~~

# Compiling

Open your terminal application and clone the git archive with

`git clone --recursive https://github.com/tstih/nice.git`

The `--recursive` flag will get the repository and all submodules
i.e. dependencies. Then `cd` to the repository folder and execute
build command (see following two chapters for the correct build 
command).

## Windows

Make sure your `vcvarsall.bat` (Visual Studio variables) are all set.

The build command is

~~~
nmake /F Makefile.nmake
~~~

## Linux

The build command is

~~~
make x11
~~~

or 

~~~
make sdl
~~~

After the compilation, samples are in the `build` folder, and the `nice.hpp`
single header library is in the `include` folder.

# Status on 20-Jun 2021

 - [x] Supported platforms: SDL, MS Windows and X11/XLib
 - [x] Basic window 
 - [x] Basic window messages
 - [x] Main window and the application class
 - [x] Proof of concept: drawing inside main window
 - [x] Raw ARGB raster images 
 - [ ] Playing wave file (60%).

Sample projects (see folder `samples`)
 * `1_minimal.cpp` Minimal application. 3 lines of code.
 * `2_raster.cpp` Paint background and display raw ARGB raster image.
 * `3_sound.cpp` Play wave file (synchronous!).

[language.url]:   https://isocpp.org/
[language.badge]: https://img.shields.io/badge/language-C++-blue.svg

[standard.url]:   https://en.wikipedia.org/wiki/C%2B%2B#Standardization
[standard.badge]: https://img.shields.io/badge/C%2B%2B-20-blue.svg

[license.url]:    https://github.com/tstih/nice/blob/master/LICENSE
[license.badge]:  https://img.shields.io/badge/license-MIT-blue.svg

[status.badge]:  https://img.shields.io/badge/status-unstable-red.svg