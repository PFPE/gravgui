cmake_minimum_required(VERSION 3.0)

project(GravGUI)

# Add your source files
set(SOURCES
    gravgui.cpp
    filt.cpp
    window_functions.c
)

# Add the executable target
add_executable(gravgui ${SOURCES})

# Find and link GTK
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
target_include_directories(gravgui PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_libraries(gravgui PRIVATE ${GTK3_LIBRARIES})
target_compile_options(gravgui PRIVATE ${GTK3_CFLAGS_OTHER})
file(COPY database DESTINATION ${CMAKE_BINARY_DIR})
file(COPY style.css DESTINATION ${CMAKE_BINARY_DIR})
