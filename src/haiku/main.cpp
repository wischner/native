#include <native.h>

extern char **environ;

int main(int argc, char **argv)
{
    native::app::argc = argc;
    native::app::argv = argv;
    native::app::envp = environ;

    return program(argc, argv);
}
