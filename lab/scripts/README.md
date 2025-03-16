 # Nice build scripts

by Tomaz Stih

To create the `nice.hpp` (single) header library inside the `include` 
directory, we need to merge all source files (found in the `src` directory) 
into one. We do this with a help of a special tool called `sm`, which we 
build on the fly.


## The sm tool

The `sm` tool extracts tagged content from source files and inserts it into 
the target file, using a template. 

Because it is crucial to the library build - the `sm` tool is the first 
executable created by the make process. It is compiled into the `tmp` folder.

 > The template file used to generate `nice.hpp` is called `nice.template`, 
 > and contains pseudo conditional include statements. 


## Build steps

Following is the build process for the library.

`Makefile -> creates >- sm -> uses >- nice.template -> to produce >- nice.hpp`

The make process fist creates the `tmp/sm` tool. This tool then uses
the `scripts/nice.template` file and sources from the `src` folder to 
generate the final `include/nice.hpp` library.

 > You can build demos on all supported systems, but you can only
 > build the single header library on unix. 