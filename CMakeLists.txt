# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)

set(PROJECT_VER "1.0.0")

project(esp32_school_bell)

# Auto-patch the CH32V003 IO expander header to wrap the I2C address in #ifndef,
# allowing it to be overridden via Kconfig (CONFIG_BSP_IO_EXPANDER_I2C_ADDR).
# MUST run AFTER project() because the component manager downloads managed
# components during project(), which would overwrite an earlier patch.
set(_ch32v003_hdr "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/waveshare__custom_io_expander_ch32v003/include/custom_io_expander_ch32v003.h")
if(EXISTS "${_ch32v003_hdr}")
    file(READ "${_ch32v003_hdr}" _ch32v003_content)
    # Idempotent patch: strip any previous (possibly broken) guards, then re-apply.
    string(REGEX REPLACE "#ifndef CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS\n" "" _ch32v003_clean "${_ch32v003_content}")
    string(REGEX REPLACE "#endif[^\n]*CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS[^\n]*\n" "" _ch32v003_clean "${_ch32v003_clean}")
    string(REGEX REPLACE
        "#define (CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS[^\n]*)"
        "#ifndef CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS\n#define \\1\n#endif // CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS"
        _ch32v003_patched "${_ch32v003_clean}")
    if(NOT "${_ch32v003_patched}" STREQUAL "${_ch32v003_content}")
        file(WRITE "${_ch32v003_hdr}" "${_ch32v003_patched}")
        message(STATUS "Patched CH32V003 header with #ifndef guard for configurable I2C address")
    endif()
endif()

# Override CH32V003 I2C address from Kconfig on the actual component targets.
# add_compile_definitions() does NOT propagate to component targets already
# configured during project(), so we must use target_compile_definitions().
idf_component_get_property(waveshare_lib waveshare__esp32_s3_touch_lcd_4 COMPONENT_LIB)
idf_component_get_property(ch32v003_lib waveshare__custom_io_expander_ch32v003 COMPONENT_LIB)
target_compile_definitions(${waveshare_lib} PRIVATE CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS=${CONFIG_BSP_IO_EXPANDER_I2C_ADDR})
target_compile_definitions(${ch32v003_lib} PRIVATE CUSTOM_IO_EXPANDER_I2C_CH32V003_ADDRESS=${CONFIG_BSP_IO_EXPANDER_I2C_ADDR})

# Fix missing #include <string.h> in managed waveshare component (memcpy usage)
target_compile_options(${waveshare_lib} PRIVATE -include "string.h")

add_subdirectory(data)