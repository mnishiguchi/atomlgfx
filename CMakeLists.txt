# CMakeLists.txt

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
set(LGFX_ROOT     "${CMAKE_CURRENT_LIST_DIR}/third_party/LovyanGFX")
set(LGFX_SRC_ROOT "${LGFX_ROOT}/src")

set(LGFX_PORT_CONFIG_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/lgfx_port/cmake/lgfx_port_config.h.in")

set(LGFX_PORT_ENABLE_JP_FONTS ON CACHE BOOL "Enable Japanese font data for setFontPreset jp_*")
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
set(LGFX_PORT_PANEL_DRIVER "ILI9488" CACHE STRING "Build-default LCD panel driver preset (ILI9488, ILI9341, ILI9341_2, or ST7789)")
set_property(CACHE LGFX_PORT_PANEL_DRIVER PROPERTY STRINGS ILI9488 ILI9341 ILI9341_2 ST7789)

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

# Apply build-default panel baseline values without clobbering user overrides.
#
# Behavior:
# - first configure:
#     seed the cache with the selected build-default panel preset
# - later re-configure:
#     if the current cache value still matches the last auto-applied preset,
#     update it to the new preset
# - explicit user overrides:
#     preserved
#
function(lgfx_port_apply_panel_preset cache_name cache_type docstring preset_value)
  set(_auto_key "_LGFX_PORT_AUTO_${cache_name}")

  if(NOT DEFINED ${cache_name})
    set(${cache_name} "${preset_value}" CACHE ${cache_type} "${docstring}")
    set(${_auto_key} "${preset_value}" CACHE INTERNAL "Last auto-applied default for ${cache_name}" FORCE)
    return()
  endif()

  if(DEFINED ${_auto_key} AND "${${cache_name}}" STREQUAL "${${_auto_key}}")
    set(${cache_name} "${preset_value}" CACHE ${cache_type} "${docstring}" FORCE)
  endif()

  set(${_auto_key} "${preset_value}" CACHE INTERNAL "Last auto-applied default for ${cache_name}" FORCE)
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
  lgfx_port_apply_panel_preset(
    LGFX_PORT_PANEL_WIDTH
    STRING
    "Panel width (pixels)"
    "${panel_width}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_PANEL_HEIGHT
    STRING
    "Panel height (pixels)"
    "${panel_height}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_OFFSET_ROTATION
    STRING
    "LCD panel offset_rotation (0..7)"
    "${lcd_offset_rotation}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_TOUCH_OFFSET_ROTATION
    STRING
    "Touch offset_rotation (0..7); panel-preset baseline, overridable independently"
    "${touch_offset_rotation}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_DUMMY_READ_PIXEL
    STRING
    "LCD dummy_read_pixel"
    "${lcd_dummy_read_pixel}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_DUMMY_READ_BITS
    STRING
    "LCD dummy_read_bits"
    "${lcd_dummy_read_bits}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_READABLE
    BOOL
    "LCD readable"
    "${lcd_readable}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_INVERT
    BOOL
    "LCD invert colors"
    "${lcd_invert}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_RGB_ORDER
    BOOL
    "LCD rgb_order swap"
    "${lcd_rgb_order}"
  )

  lgfx_port_apply_panel_preset(
    LGFX_PORT_LCD_DLEN_16BIT
    BOOL
    "LCD dlen_16bit"
    "${lcd_dlen_16bit}"
  )
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
  set(LGFX_PORT_PANEL_DRIVER_ILI9488_INT 0 PARENT_SCOPE)
  set(LGFX_PORT_PANEL_DRIVER_ILI9341_INT 0 PARENT_SCOPE)
  set(LGFX_PORT_PANEL_DRIVER_ILI9341_2_INT 0 PARENT_SCOPE)
  set(LGFX_PORT_PANEL_DRIVER_ST7789_INT 0 PARENT_SCOPE)

  if("${selected_driver}" STREQUAL "ILI9488")
    set(LGFX_PORT_PANEL_DRIVER_ILI9488_INT 1 PARENT_SCOPE)
  elseif("${selected_driver}" STREQUAL "ILI9341")
    set(LGFX_PORT_PANEL_DRIVER_ILI9341_INT 1 PARENT_SCOPE)
  elseif("${selected_driver}" STREQUAL "ILI9341_2")
    set(LGFX_PORT_PANEL_DRIVER_ILI9341_2_INT 1 PARENT_SCOPE)
  elseif("${selected_driver}" STREQUAL "ST7789")
    set(LGFX_PORT_PANEL_DRIVER_ST7789_INT 1 PARENT_SCOPE)
  else()
    message(FATAL_ERROR
      "LGFX_PORT_PANEL_DRIVER must be one of: ILI9488, ILI9341, ILI9341_2, ST7789 (got: \"${selected_driver}\")"
    )
  endif()
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

# Collect sources from multiple glob patterns into a single output list.
function(lgfx_port_collect_globbed out_var)
  set(_collected)
  foreach(_pattern ${ARGN})
    file(GLOB_RECURSE _matches "${_pattern}")
    list(APPEND _collected ${_matches})
  endforeach()
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
if(LGFX_PORT_PANEL_DRIVER STREQUAL "ILI9488")
  lgfx_port_apply_panel_baseline(
    "320" "480"
    "0" "0"
    "8" "1"
    OFF OFF OFF OFF
  )

elseif(LGFX_PORT_PANEL_DRIVER STREQUAL "ILI9341")
  lgfx_port_apply_panel_baseline(
    "240" "320"
    "0" "0"
    "8" "1"
    OFF OFF OFF OFF
  )

elseif(LGFX_PORT_PANEL_DRIVER STREQUAL "ILI9341_2")
  # Baseline for the mirrored 240x320 ILI9341_2-family boards we are testing.
  #
  # 4 instead of 6 keeps the same mirrored family but corrects a 180-degree flip.
  #
  # Common quirks for the current ILI9341_2 module under test:
  # - colors are inverted unless invert=true
  # - native channel order appears correct with rgb_order=false
  # - keep readable off unless readback is known-good on this wiring
  lgfx_port_apply_panel_baseline(
    "240" "320"
    "4" "4"
    "8" "1"
    OFF ON OFF OFF
  )

elseif(LGFX_PORT_PANEL_DRIVER STREQUAL "ST7789")
  lgfx_port_apply_panel_baseline(
    "240" "240"
    "0" "0"
    "16" "1"
    OFF OFF OFF OFF
  )

else()
  message(FATAL_ERROR
    "LGFX_PORT_PANEL_DRIVER must be one of: ILI9488, ILI9341, ILI9341_2, ST7789 (got: \"${LGFX_PORT_PANEL_DRIVER}\")"
  )
endif()

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
# Keep these aligned with src/lgfx_device_state.cpp:
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

set(LGFX_PORT_TOUCH_SPI_HOST
    "${LGFX_PORT_LCD_SPI_HOST}"
    CACHE STRING "SPI host token for touch (defaults to LCD host)")
set(LGFX_PORT_TOUCH_SPI_FREQ_HZ "1000000u" CACHE STRING "Touch SPI frequency (Hz, token)")

# Custom minimal JP font subset
set(LGFX_PORT_JP_FONT_CPP
    "${CMAKE_CURRENT_LIST_DIR}/src/fonts/generated/ui_font_ja_16_min.cpp"
    CACHE STRING "Path to custom JP font subset .cpp (compiled when LGFX_PORT_ENABLE_JP_FONTS=ON)")

# -----------------------------------------------------------------------------
# Register component
# -----------------------------------------------------------------------------
idf_component_register(
  SRCS
    # Port entrypoint / lifecycle
    "lgfx_port/lgfx_port.c"

    # Worker split
    "lgfx_port/lgfx_worker_core.c"
    "lgfx_port/lgfx_worker_device.c"

    # Protocol / dispatch / metadata
    "lgfx_port/proto_term.c"

    # Handlers
    "lgfx_port/handlers/control.c"
    "lgfx_port/handlers/device.c"
    "lgfx_port/handlers/primitives.c"
    "lgfx_port/handlers/text.c"
    "lgfx_port/handlers/images.c"
    "lgfx_port/handlers/sprites.c"
    "lgfx_port/handlers/touch.c"

    # Device adapter
    "src/lgfx_device_state.cpp"
    "src/lgfx_device_control.cpp"
    "src/lgfx_device_text.cpp"
    "src/lgfx_device_primitives.cpp"
    "src/lgfx_device_images.cpp"
    "src/lgfx_device_sprites.cpp"
  INCLUDE_DIRS
    "include"
  PRIV_INCLUDE_DIRS
    "lgfx_port/include_internal"
    "src"
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
# Protocol constants
# -----------------------------------------------------------------------------
set(LGFX_PORT_MAX_SPRITES "8u" CACHE STRING "Max sprites (numeric literal, e.g. 8u; must be 0..254)")

# -----------------------------------------------------------------------------
# Validate protocol literals
# -----------------------------------------------------------------------------
lgfx_port_validate_u8_literal(LGFX_PORT_MAX_SPRITES)

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
# Japanese font data
# -----------------------------------------------------------------------------
if(LGFX_PORT_ENABLE_JP_FONTS)
  if(NOT EXISTS "${LGFX_PORT_JP_FONT_CPP}")
    message(FATAL_ERROR
      "Custom JP font subset .cpp not found:\n  ${LGFX_PORT_JP_FONT_CPP}\n\n"
      "Set LGFX_PORT_JP_FONT_CPP to the path of your generated subset, e.g.:\n"
      "  -DLGFX_PORT_JP_FONT_CPP=${CMAKE_CURRENT_LIST_DIR}/src/fonts/generated/ui_font_ja_16_min.cpp\n"
    )
  endif()

  target_sources(${COMPONENT_LIB} PRIVATE "${LGFX_PORT_JP_FONT_CPP}")
endif()

list(REMOVE_DUPLICATES LGFX_SRCS)

# -----------------------------------------------------------------------------
# Reduce LovyanGFX build surface area
# -----------------------------------------------------------------------------
#
# Keep SPI and optionally XPT2046.
# Panel driver selection is handled in src/lgfx_device_state.cpp.
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

# -----------------------------------------------------------------------------
# Compile LovyanGFX as part of this component
# -----------------------------------------------------------------------------
target_sources(${COMPONENT_LIB} PRIVATE ${LGFX_SRCS})

target_include_directories(${COMPONENT_LIB} PRIVATE
  "${LGFX_SRC_ROOT}"
)

# Build-default panel-driver selection macros come from the generated config header
# (lgfx_port/lgfx_port_config.h).
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  LGFX_USE_V1=1
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
