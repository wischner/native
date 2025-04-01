# Chapter 1: application

This chapter introduces the **application** class, which serves as the entry point for a graphical program in **native**. It is responsible for initializing the underlying system, starting the event loop, and performing cleanup on shutdown.

The goal is to establish a consistent and minimal interface for starting applications across Linux, Windows, and Haiku.

## Objective

By the end of this chapter, the application structure will allow a minimal program to be written as follows:

```cpp
#include "native.h"

int program(int argc, char* argv[])
{
    return native::app::run(app_wnd("Hello World!"));
}
```
