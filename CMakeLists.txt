# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

# CMakeLists.txt

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
set(LGFX_ROOT     "${CMAKE_CURRENT_LIST_DIR}/third_party/LovyanGFX")
set(LGFX_SRC_ROOT "${LGFX_ROOT}/src")

set(LGFX_PORT_CONFIG_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/lgfx_port/cmake/lgfx_port_config.h.in")

set(LGFX_PORT_ENABLE_JP_FONTS ON CACHE BOOL "Enable Japanese font data for the Japanese-capable text preset")
set(LGFX_PORT_ENABLE_TOUCH ON CACHE BOOL "Enable XPT2046 touch driver (Touch_XPT2046)")

# -----------------------------------------------------------------------------
# LovyanGFX config knobs
# -----------------------------------------------------------------------------

# Build-default panel driver preset
#
# This is the build-time default profile used to seed baseline cache values such as:
# - panel width / height
# - LCD offset_rotation
# - touch offset_rotation
# - dummy read settings
# - readable / invert / rgb_order / dlen_16bit
#
# Runtime note:
# - open_port/open-time config may still override panel_driver and related fields later.
# - This cache value remains useful as the default panel profile for builds and examples.
#
# Supported LovyanGFX panel names:
# - ILI9488
#     Maps to LovyanGFX Panel_ILI9488
# - ILI9341
#     Maps to LovyanGFX Panel_ILI9341
# - ILI9341_2
#     Maps to LovyanGFX Panel_ILI9341_2 for mirrored 240x320 boards
#     that need the alternate init sequence.
# - ST7789
#     Maps to LovyanGFX Panel_ST7789
#     Common starting point for 240x240 SPI boards.
#
set(LGFX_PORT_SUPPORTED_PANEL_DRIVERS
  ILI9488
  ILI9341
  ILI9341_2
  ST7789
)

set(LGFX_PORT_PANEL_DRIVER
  "ILI9488"
  CACHE STRING
  "Build-default LCD panel driver preset (ILI9488, ILI9341, ILI9341_2, or ST7789)"
)

set_property(
  CACHE LGFX_PORT_PANEL_DRIVER
  PROPERTY STRINGS ${LGFX_PORT_SUPPORTED_PANEL_DRIVERS}
)

# Baseline panel profiles:
#   width ; height ; lcd_offset_rotation ; touch_offset_rotation ;
#   lcd_dummy_read_pixel ; lcd_dummy_read_bits ;
#   lcd_readable ; lcd_invert ; lcd_rgb_order ; lcd_dlen_16bit
set(LGFX_PORT_PANEL_BASELINE_ILI9488
  "320;480;0;0;8;1;OFF;OFF;OFF;OFF"
)

set(LGFX_PORT_PANEL_BASELINE_ILI9341
  "240;320;0;0;8;1;OFF;OFF;OFF;OFF"
)

# Baseline for the mirrored 240x320 ILI9341_2-family boards we are testing.
#
# 4 instead of 6 keeps the same mirrored family but corrects a 180-degree flip.
#
# Common quirks for the current ILI9341_2 module under test:
# - colors are inverted unless invert=true
# - native channel order appears correct with rgb_order=false
# - keep readable off unless readback is known-good on this wiring
set(LGFX_PORT_PANEL_BASELINE_ILI9341_2
  "240;320;4;4;8;1;OFF;ON;OFF;OFF"
)

set(LGFX_PORT_PANEL_BASELINE_ST7789
  "240;240;0;0;16;1;OFF;OFF;OFF;OFF"
)

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

# Apply a tracked default without clobbering an explicit user override.
#
# Behavior:
# - first configure:
#     seed the cache with the provided default
# - later re-configure:
#     if the current cache value still matches the last auto-applied default,
#     update it to the new default
# - explicit user overrides:
#     preserved
#
function(lgfx_port_apply_tracked_default cache_name cache_type docstring default_value)
  set(_auto_key "_LGFX_PORT_AUTO_${cache_name}")

  if(NOT DEFINED ${cache_name})
    set(${cache_name} "${default_value}" CACHE ${cache_type} "${docstring}")
    set(${_auto_key} "${default_value}" CACHE INTERNAL "Last auto-applied default for ${cache_name}" FORCE)
    return()
  endif()

  if(DEFINED ${_auto_key} AND "${${cache_name}}" STREQUAL "${${_auto_key}}")
    set(${cache_name} "${default_value}" CACHE ${cache_type} "${docstring}" FORCE)
  endif()

  set(${_auto_key} "${default_value}" CACHE INTERNAL "Last auto-applied default for ${cache_name}" FORCE)
endfunction()

# Apply the full build-default panel baseline set in one place.
function(
  lgfx_port_apply_panel_baseline
  panel_width
  panel_height
  lcd_offset_rotation
  touch_offset_rotation
  lcd_dummy_read_pixel
  lcd_dummy_read_bits
  lcd_readable
  lcd_invert
  lcd_rgb_order
  lcd_dlen_16bit
)
  lgfx_port_apply_tracked_default(
    LGFX_PORT_PANEL_WIDTH
    STRING
    "Panel width (pixels)"
    "${panel_width}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_PANEL_HEIGHT
    STRING
    "Panel height (pixels)"
    "${panel_height}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_OFFSET_ROTATION
    STRING
    "LCD panel offset_rotation (0..7)"
    "${lcd_offset_rotation}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_TOUCH_OFFSET_ROTATION
    STRING
    "Touch offset_rotation (0..7); panel-preset baseline, overridable independently"
    "${touch_offset_rotation}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_DUMMY_READ_PIXEL
    STRING
    "LCD dummy_read_pixel"
    "${lcd_dummy_read_pixel}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_DUMMY_READ_BITS
    STRING
    "LCD dummy_read_bits"
    "${lcd_dummy_read_bits}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_READABLE
    BOOL
    "LCD readable"
    "${lcd_readable}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_INVERT
    BOOL
    "LCD invert colors"
    "${lcd_invert}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_RGB_ORDER
    BOOL
    "LCD rgb_order swap"
    "${lcd_rgb_order}"
  )

  lgfx_port_apply_tracked_default(
    LGFX_PORT_LCD_DLEN_16BIT
    BOOL
    "LCD dlen_16bit"
    "${lcd_dlen_16bit}"
  )
endfunction()

# Require a variable value to be one of the provided allowed strings.
function(lgfx_port_require_one_of var_name)
  list(FIND ARGN "${${var_name}}" _index)
  if(_index EQUAL -1)
    string(JOIN ", " _allowed ${ARGN})
    message(FATAL_ERROR
      "${var_name} must be one of: ${_allowed} (got: \"${${var_name}}\")"
    )
  endif()
endfunction()

# Apply the selected panel-driver baseline from the panel baseline table above.
function(lgfx_port_apply_selected_panel_baseline selected_driver)
  set(_baseline_key "LGFX_PORT_PANEL_BASELINE_${selected_driver}")

  if(NOT DEFINED ${_baseline_key})
    string(JOIN ", " _drivers ${LGFX_PORT_SUPPORTED_PANEL_DRIVERS})
    message(FATAL_ERROR
      "LGFX_PORT_PANEL_DRIVER must be one of: ${_drivers} (got: \"${selected_driver}\")"
    )
  endif()

  lgfx_port_apply_panel_baseline(${${_baseline_key}})
endfunction()

# Convert one or more BOOL cache variables into paired *_INT values (0/1).
function(lgfx_port_bool_to_int_many)
  foreach(_name ${ARGN})
    if(${_name})
      set(${_name}_INT 1 PARENT_SCOPE)
    else()
      set(${_name}_INT 0 PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

# Derive one-hot build-default panel driver selection ints for header substitution.
function(lgfx_port_set_panel_driver_ints selected_driver)
  foreach(_driver ${LGFX_PORT_SUPPORTED_PANEL_DRIVERS})
    set(LGFX_PORT_PANEL_DRIVER_${_driver}_INT 0 PARENT_SCOPE)
  endforeach()

  set(LGFX_PORT_PANEL_DRIVER_${selected_driver}_INT 1 PARENT_SCOPE)
endfunction()

# Validate numeric literal like 8u, 42, etc. and enforce <= 254.
function(lgfx_port_validate_u8_literal var_name)
  string(STRIP "${${var_name}}" _v)

  if(NOT _v MATCHES "^[0-9]+u?$")
    message(FATAL_ERROR
      "${var_name} must be a numeric literal like 8u (got: \"${${var_name}}\")\n"
      "Hint: pass -D${var_name}=8u (or another value in 0..254)"
    )
  endif()

  string(REGEX REPLACE "u$" "" _num "${_v}")
  if(_num GREATER 254)
    message(FATAL_ERROR
      "${var_name} must be <= 254 (got: \"${${var_name}}\")\n"
      "Hint: pass -D${var_name}=254u (or smaller)"
    )
  endif()
endfunction()

# Validate that a variable is a signed integer literal.
function(lgfx_port_validate_int_literal var_name)
  string(STRIP "${${var_name}}" _value)

  if(NOT _value MATCHES "^-?[0-9]+$")
    message(FATAL_ERROR
      "${var_name} must be an integer literal (got: \"${${var_name}}\")"
    )
  endif()
endfunction()

# Validate that a variable is a signed integer literal within an inclusive range.
function(lgfx_port_validate_int_range var_name min_value max_value)
  lgfx_port_validate_int_literal(${var_name})

  string(STRIP "${${var_name}}" _value)
  if(_value LESS ${min_value} OR _value GREATER ${max_value})
    message(FATAL_ERROR
      "${var_name} must be in ${min_value}..${max_value} (got: ${_value})"
    )
  endif()
endfunction()

# Collect sources from multiple glob patterns into a single output list.
function(lgfx_port_collect_globbed out_var)
  set(_collected)

  foreach(_pattern ${ARGN})
    file(GLOB_RECURSE _matches CONFIGURE_DEPENDS "${_pattern}")
    list(APPEND _collected ${_matches})
  endforeach()

  list(REMOVE_DUPLICATES _collected)
  list(SORT _collected)

  set(${out_var} "${_collected}" PARENT_SCOPE)
endfunction()

# Filter a source list by excluding one or more regexes.
function(lgfx_port_filter_out var_name)
  set(_filtered "${${var_name}}")
  foreach(_regex ${ARGN})
    list(FILTER _filtered EXCLUDE REGEX "${_regex}")
  endforeach()
  set(${var_name} "${_filtered}" PARENT_SCOPE)
endfunction()

# -----------------------------------------------------------------------------
# Build-default panel baseline defaults
# -----------------------------------------------------------------------------
#
# Keep these as the baseline profile for the selected build-default panel family.
# Runtime open-time config may override panel_driver and related values later.
#
lgfx_port_require_one_of(LGFX_PORT_PANEL_DRIVER ${LGFX_PORT_SUPPORTED_PANEL_DRIVERS})
lgfx_port_apply_selected_panel_baseline("${LGFX_PORT_PANEL_DRIVER}")

# Bus_SPI knobs
set(LGFX_PORT_LCD_SPI_MODE       "0"               CACHE STRING "LCD SPI mode (0..3)")
set(LGFX_PORT_LCD_FREQ_WRITE_HZ  "20*1000*1000"    CACHE STRING "LCD SPI write freq (Hz, token/expression OK)")
set(LGFX_PORT_LCD_FREQ_READ_HZ   "10*1000*1000"    CACHE STRING "LCD SPI read freq (Hz, token/expression OK)")
set(LGFX_PORT_LCD_DMA_CHANNEL    "SPI_DMA_CH_AUTO" CACHE STRING "LCD DMA channel token (e.g. SPI_DMA_CH_AUTO, 1, 2)")

set(LGFX_PORT_LCD_SPI_3WIRE OFF CACHE BOOL "LCD SPI 3-wire mode")
set(LGFX_PORT_LCD_USE_LOCK  ON  CACHE BOOL "LCD SPI use_lock")

# Panel knobs
set(LGFX_PORT_LCD_PIN_BUSY "-1" CACHE STRING "LCD busy pin GPIO (-1 disables)")
set(LGFX_PORT_LCD_OFFSET_X "0"  CACHE STRING "LCD panel offset_x")
set(LGFX_PORT_LCD_OFFSET_Y "0"  CACHE STRING "LCD panel offset_y")

set(LGFX_PORT_LCD_BUS_SHARED   ON CACHE BOOL "LCD bus_shared")
set(LGFX_PORT_TOUCH_BUS_SHARED ON CACHE BOOL "Touch bus_shared")

# -----------------------------------------------------------------------------
# Wiring defaults
# -----------------------------------------------------------------------------
#
# Keep these aligned with lgfx_device/lgfx_device_state.cpp:
# - SPI:   SCLK=7 MOSI=9 MISO=8
# - LCD:   CS=43 DC=3 RST=2
# - Touch: CS=44 IRQ=-1
#
# If TOUCH CS is -1, touch may still compile but will not be advertised.
#
set(LGFX_PORT_SPI_SCLK_GPIO "7" CACHE STRING "SPI SCLK GPIO")
set(LGFX_PORT_SPI_MOSI_GPIO "9" CACHE STRING "SPI MOSI GPIO")
set(LGFX_PORT_SPI_MISO_GPIO "8" CACHE STRING "SPI MISO GPIO")

set(LGFX_PORT_LCD_SPI_HOST "SPI2_HOST" CACHE STRING "SPI host token for LCD")
set(LGFX_PORT_LCD_CS_GPIO  "43"        CACHE STRING "LCD CS GPIO")
set(LGFX_PORT_LCD_DC_GPIO  "3"         CACHE STRING "LCD DC GPIO")
set(LGFX_PORT_LCD_RST_GPIO "2"         CACHE STRING "LCD RST GPIO")

set(LGFX_PORT_TOUCH_CS_GPIO  "44" CACHE STRING "XPT2046 CS GPIO; -1 disables attach")
set(LGFX_PORT_TOUCH_IRQ_GPIO "-1" CACHE STRING "XPT2046 IRQ GPIO; -1 disables IRQ")

lgfx_port_apply_tracked_default(
  LGFX_PORT_TOUCH_SPI_HOST
  STRING
  "SPI host token for touch (defaults to LCD host)"
  "${LGFX_PORT_LCD_SPI_HOST}"
)

set(LGFX_PORT_TOUCH_SPI_FREQ_HZ "1000000u" CACHE STRING "Touch SPI frequency (Hz, token)")

# Custom minimal Japanese font subset used by the Japanese-capable text preset
lgfx_port_apply_tracked_default(
  LGFX_PORT_JP_FONT_CPP
  STRING
  "Path to custom Japanese font subset .cpp (compiled when LGFX_PORT_ENABLE_JP_FONTS=ON)"
  "${CMAKE_CURRENT_LIST_DIR}/lgfx_device/fonts/generated/ui_font_ja_16_min.cpp"
)

# -----------------------------------------------------------------------------
# Register component
# -----------------------------------------------------------------------------
idf_component_register(
  SRCS
    # Port entrypoint / lifecycle
    "lgfx_port/lgfx_port.c"

    # Protocol / dispatch / metadata
    "lgfx_port/proto_term.c"

    # Handlers
    "lgfx_port/handlers/control.c"
    "lgfx_port/handlers/device.c"
    "lgfx_port/handlers/text.c"
    "lgfx_port/handlers/primitives.c"
    "lgfx_port/handlers/images.c"
    "lgfx_port/handlers/sprites.c"
    "lgfx_port/handlers/clip.c"
    "lgfx_port/handlers/touch.c"

    # Device adapter
    "lgfx_device/state.cpp"
    "lgfx_device/control.cpp"
    "lgfx_device/text.cpp"
    "lgfx_device/primitives.cpp"
    "lgfx_device/images.cpp"
    "lgfx_device/sprites.cpp"
    "lgfx_device/clip.cpp"
  INCLUDE_DIRS
    "include"
  PRIV_INCLUDE_DIRS
    "lgfx_port/include_internal"
    "lgfx_device"
    "lgfx_port"
    "${LGFX_SRC_ROOT}"
  REQUIRES
    efuse
  PRIV_REQUIRES
    libatomvm
    avm_sys
    freertos
    esp_timer
    driver
    fatfs
    sdmmc
  WHOLE_ARCHIVE
)

# -----------------------------------------------------------------------------
# ESP-IDF requirements scan / script mode
# -----------------------------------------------------------------------------
if(DEFINED __COMPONENT_GET_REQUIREMENTS AND __COMPONENT_GET_REQUIREMENTS)
  return()
endif()

if(CMAKE_SCRIPT_MODE_FILE)
  return()
endif()

# -----------------------------------------------------------------------------
# Protocol constants
# -----------------------------------------------------------------------------
set(LGFX_PORT_MAX_SPRITES "8u" CACHE STRING "Max sprites (numeric literal, e.g. 8u; must be 0..254)")

# -----------------------------------------------------------------------------
# Validate literal-only knobs
# -----------------------------------------------------------------------------
lgfx_port_validate_int_range(LGFX_PORT_LCD_SPI_MODE 0 3)

lgfx_port_validate_int_range(LGFX_PORT_LCD_OFFSET_ROTATION 0 7)
lgfx_port_validate_int_range(LGFX_PORT_TOUCH_OFFSET_ROTATION 0 7)

lgfx_port_validate_int_range(LGFX_PORT_PANEL_WIDTH 1 65535)
lgfx_port_validate_int_range(LGFX_PORT_PANEL_HEIGHT 1 65535)

lgfx_port_validate_int_literal(LGFX_PORT_LCD_PIN_BUSY)
lgfx_port_validate_int_literal(LGFX_PORT_LCD_OFFSET_X)
lgfx_port_validate_int_literal(LGFX_PORT_LCD_OFFSET_Y)

lgfx_port_validate_int_literal(LGFX_PORT_SPI_SCLK_GPIO)
lgfx_port_validate_int_literal(LGFX_PORT_SPI_MOSI_GPIO)
lgfx_port_validate_int_literal(LGFX_PORT_SPI_MISO_GPIO)

lgfx_port_validate_int_literal(LGFX_PORT_LCD_CS_GPIO)
lgfx_port_validate_int_literal(LGFX_PORT_LCD_DC_GPIO)
lgfx_port_validate_int_literal(LGFX_PORT_LCD_RST_GPIO)

lgfx_port_validate_int_literal(LGFX_PORT_TOUCH_CS_GPIO)
lgfx_port_validate_int_literal(LGFX_PORT_TOUCH_IRQ_GPIO)

# -----------------------------------------------------------------------------
# Validate protocol literals
# -----------------------------------------------------------------------------
lgfx_port_validate_u8_literal(LGFX_PORT_MAX_SPRITES)

# -----------------------------------------------------------------------------
# Derive config ints for header substitution (0/1)
# -----------------------------------------------------------------------------
lgfx_port_bool_to_int_many(
  LGFX_PORT_ENABLE_JP_FONTS
  LGFX_PORT_ENABLE_TOUCH
  LGFX_PORT_LCD_SPI_3WIRE
  LGFX_PORT_LCD_USE_LOCK
  LGFX_PORT_LCD_READABLE
  LGFX_PORT_LCD_INVERT
  LGFX_PORT_LCD_RGB_ORDER
  LGFX_PORT_LCD_DLEN_16BIT
  LGFX_PORT_LCD_BUS_SHARED
  LGFX_PORT_TOUCH_BUS_SHARED
)

# -----------------------------------------------------------------------------
# Derive build-default panel driver selection ints for header substitution (0/1)
# -----------------------------------------------------------------------------
lgfx_port_set_panel_driver_ints("${LGFX_PORT_PANEL_DRIVER}")

# -----------------------------------------------------------------------------
# Derive touch advertisement gate
# -----------------------------------------------------------------------------
#
# Keep only the gate that actually varies. Sprite / pushImage / lastError are
# always part of the current compiled protocol surface, so they no longer need
# generated 0/1 macros.
#
set(LGFX_PORT_SUPPORTS_TOUCH_INT 0)
if(LGFX_PORT_ENABLE_TOUCH)
  if(NOT "${LGFX_PORT_TOUCH_CS_GPIO}" STREQUAL "-1")
    set(LGFX_PORT_SUPPORTS_TOUCH_INT 1)
  endif()
endif()

if(LGFX_PORT_ENABLE_TOUCH AND "${LGFX_PORT_TOUCH_CS_GPIO}" STREQUAL "-1")
  message(STATUS "LGFX touch: enabled but unattached (LGFX_PORT_TOUCH_CS_GPIO=-1) -> touch will NOT be advertised")
endif()

# -----------------------------------------------------------------------------
# Generate config header
# -----------------------------------------------------------------------------
set(LGFX_PORT_GEN_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LGFX_PORT_GEN_INC_DIR}/lgfx_port")

configure_file(
  "${LGFX_PORT_CONFIG_TEMPLATE}"
  "${LGFX_PORT_GEN_INC_DIR}/lgfx_port/lgfx_port_config.h"
  @ONLY
)

target_include_directories(${COMPONENT_LIB} PUBLIC "${LGFX_PORT_GEN_INC_DIR}")

# -----------------------------------------------------------------------------
# Validate LovyanGFX availability
# -----------------------------------------------------------------------------
if(NOT EXISTS "${LGFX_SRC_ROOT}/LovyanGFX.hpp")
  message(FATAL_ERROR
    "LovyanGFX not found at: ${LGFX_SRC_ROOT}\n"
    "Run: git submodule update --init --recursive"
  )
endif()

# -----------------------------------------------------------------------------
# Collect LovyanGFX sources
# -----------------------------------------------------------------------------
lgfx_port_collect_globbed(
  LGFX_V1_COMMON_SRCS
  "${LGFX_SRC_ROOT}/LovyanGFX*.cpp"
  "${LGFX_SRC_ROOT}/lgfx/v1/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/*.cpp"
  "${LGFX_SRC_ROOT}/lgfx/v1/**/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/**/*.cpp"
)

lgfx_port_filter_out(
  LGFX_V1_COMMON_SRCS
  ".*/lgfx/v1/platforms/.*"
)

lgfx_port_collect_globbed(
  LGFX_V1_ESP32_SRCS
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/*.cpp"
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/**/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/**/*.cpp"
)

set(LGFX_SRCS
  ${LGFX_V1_COMMON_SRCS}
  ${LGFX_V1_ESP32_SRCS}
)

# JPEG decoder support
list(APPEND LGFX_SRCS
  "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_tjpgd.c"
)

# -----------------------------------------------------------------------------
# Reduce LovyanGFX build surface area
# -----------------------------------------------------------------------------
#
# Keep SPI and optionally XPT2046.
# Panel driver selection is handled in lgfx_device/lgfx_device_state.cpp.
# Exclude EPD, I2C, and other touch drivers.
#
lgfx_port_filter_out(
  LGFX_SRCS
  ".*/Bus_EPD\\.(c|cpp)$"
  ".*/Panel_EPD\\.(c|cpp)$"
  ".*/panel/Panel_EPD.*\\.(c|cpp)$"
  ".*/panel/Panel_GDEW.*\\.(c|cpp)$"
  ".*/Bus_I2C\\.(c|cpp)$"
  ".*/touch/.*\\.(c|cpp)$"
  ".*/Touch_.*\\.(c|cpp)$"
  ".*/[Ii]2[Cc].*\\.(c|cpp)$"
)

# -----------------------------------------------------------------------------
# Touch: re-add only XPT2046
# -----------------------------------------------------------------------------
if(LGFX_PORT_ENABLE_TOUCH)
  list(APPEND LGFX_SRCS
    "${LGFX_SRC_ROOT}/lgfx/v1/touch/Touch_XPT2046.cpp"
  )
endif()

list(REMOVE_DUPLICATES LGFX_SRCS)
list(SORT LGFX_SRCS)

# -----------------------------------------------------------------------------
# Compile LovyanGFX as part of this component
# -----------------------------------------------------------------------------
target_sources(${COMPONENT_LIB} PRIVATE ${LGFX_SRCS})

# Optional Japanese font data backing the Japanese-capable text preset
if(LGFX_PORT_ENABLE_JP_FONTS)
  if(NOT EXISTS "${LGFX_PORT_JP_FONT_CPP}")
    message(FATAL_ERROR
      "Custom Japanese font subset .cpp not found:\n  ${LGFX_PORT_JP_FONT_CPP}\n\n"
      "Set LGFX_PORT_JP_FONT_CPP to the path of your generated subset, e.g.:\n"
      "  -DLGFX_PORT_JP_FONT_CPP=${CMAKE_CURRENT_LIST_DIR}/lgfx_device/fonts/generated/ui_font_ja_16_min.cpp\n"
    )
  endif()

  target_sources(${COMPONENT_LIB} PRIVATE "${LGFX_PORT_JP_FONT_CPP}")
endif()

# Build-default panel-driver selection macros come from the generated config header
# (lgfx_port/lgfx_port_config.h).
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  LGFX_USE_V1=1
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
