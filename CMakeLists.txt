# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)

set(PROJECT_VER "1.0.0")

project(esp32_school_bell)

# Fix missing #include <string.h> in managed waveshare component (memcpy usage)
idf_component_get_property(waveshare_lib waveshare__esp32_s3_touch_lcd_4 COMPONENT_LIB)
target_compile_options(${waveshare_lib} PRIVATE -include "string.h")

add_subdirectory(data)