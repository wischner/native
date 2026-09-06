# The upstream SDK exports libraries and headers but no pkg-config files.
# Keep Native's existing module names for direct AES/VDI consumers.
file(MAKE_DIRECTORY /usr/local/lib/pkgconfig)
foreach(module aes vdi rasta platform-linux)
    string(REPLACE "-" "_" library "${module}")
    set(requires "")
    if(module STREQUAL "aes")
        set(requires "Requires: gemix-vdi\n")
    elseif(module STREQUAL "vdi")
        set(requires "Requires: gemix-rasta\n")
    endif()
    file(WRITE "/usr/local/lib/pkgconfig/gemix-${module}.pc"
        "prefix=/usr/local\n"
        "libdir=\${prefix}/lib\n"
        "includedir=\${prefix}/include\n"
        "Name: GEM ${module}\n"
        "Description: GEM v1.0.0 ${module} library\n"
        "Version: 1.0.0\n"
        "Cflags: -I\${includedir}\n"
        "Libs: -L\${libdir} -l${library}\n"
        "${requires}")
endforeach()
file(WRITE /usr/local/lib/pkgconfig/gemix.pc
    "Name: GEM\nDescription: GEM v1.0.0 SDK\nVersion: 1.0.0\n"
    "Requires: gemix-aes\n")
