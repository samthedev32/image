cmake_minimum_required(VERSION 3.14)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(image)

set(SOURCES
    src/image.c
    src/draw.c

    src/bmp.c
    src/qoi.c
)

add_library(image ${SOURCES})
target_include_directories(image PUBLIC "include")

add_subdirectory(examples)