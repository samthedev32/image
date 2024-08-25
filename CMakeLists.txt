cmake_minimum_required(VERSION 3.14)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(image)

set(SOURCES
    src/image.c
    src/draw.c

    src/bmp.c
    src/png.c
    src/qoi.c
    src/tiff.c
)

add_library(image ${SOURCES})
target_include_directories(image PUBLIC "include")
target_link_libraries(image PUBLIC z)

add_subdirectory(examples)