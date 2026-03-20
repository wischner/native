# Native library

## Build

## Architecture
- Library is divided into three levels: C++ pure level, platform level and toolkit level
- C++ pure level classes are in src/ and are the same for all platforms
- platform level target operating systems such as haiku, l;inux, mac or windows
- toolkit level targets toolkit on operating system such as sdl on linux, openmotif on linux...
- not all OSes need toolkits, some implement all logic inside platform
- we do not place any native code inside classes of native, that are exposed to the user
- all native code i.e. platform or toolkit specific code is located in classes and structures out of native classes
- native handles and resources are mapped to C++ classes using external native::bindings, for example: native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings 
- All such binding are in file global.cpp and in namespace which is the name of toolkit or platform if there is not toolkit

## Interface (library user view)
- User sees native as pure modern C++ library
- User uses latest C++ version i.e. 20+
- User does not see any native code
- All user classes are exposed in /include folder in single header called native.h
- There are NO NATIVE ELEMENTS in native.h, it is all pure c++

## Samples
- .vscode should be configured to run latest sample
- First example should simply create a window
- Second example should create a simple painter software where mouse down on window starts a stroke, mouse move paints it and mouse up button ends it. strokes are remembered.
