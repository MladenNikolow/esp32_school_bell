# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)

# Auto-patch the CH32V003 IO expander header to wrap the I2C address in #ifndef,
# allowing it to be overridden via Kconfig (CONFIG_BSP_IO_EXPANDER_I2C_ADDR).
# This runs at configure time and survives managed component re-downloads.
set(_ch32v003_hdr "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/waveshare__custom_io_expander_ch32v003/include/custom_io_expander_ch32v003.h")
if(EXISTS "${_ch32v003_hdr}")
    file(READ "${_ch32v003_hdr}" _ch32v003_content)
    if(NOT _ch32v003_content MATCHES "#ifndef CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS")
        string(REPLACE
            "#define CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS"
            "#ifndef CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS\n#define CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS"
            _ch32v003_patched "${_ch32v003_content}")
        string(REPLACE
            "#ifdef __cplusplus"
            "#endif // CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS\n\n#ifdef __cplusplus"
            _ch32v003_patched "${_ch32v003_patched}")
        file(WRITE "${_ch32v003_hdr}" "${_ch32v003_patched}")
        message(STATUS "Patched CH32V003 header with #ifndef guard for configurable I2C address")
    endif()
endif()

set(PROJECT_VER "1.0.0")

project(esp32_school_bell)

# Override CH32V003 I2C address from Kconfig
add_compile_definitions(CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS=${CONFIG_BSP_IO_EXPANDER_I2C_ADDR})

# Fix missing #include <string.h> in managed waveshare component (memcpy usage)
idf_component_get_property(waveshare_lib waveshare__esp32_s3_touch_lcd_4 COMPONENT_LIB)
target_compile_options(${waveshare_lib} PRIVATE -include "string.h")

add_subdirectory(data)