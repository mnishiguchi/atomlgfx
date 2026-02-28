# CMakeLists.txt
#
# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
set(LGFX_ROOT     "${CMAKE_CURRENT_LIST_DIR}/third_party/LovyanGFX")
set(LGFX_SRC_ROOT "${LGFX_ROOT}/src")

# Optional build knob (helps when flash is tight)
option(LGFX_PORT_ENABLE_JP_FONTS "Enable Japanese font data for setFontPreset jp_*" ON)

# Optional build knob: XPT2046 touch support (compile in Touch_XPT2046 only)
option(LGFX_PORT_ENABLE_TOUCH "Enable XPT2046 touch driver (Touch_XPT2046)" ON)

# -----------------------------------------------------------------------------
# Extra LovyanGFX config knobs
# -----------------------------------------------------------------------------

# Panel geometry
set(LGFX_PORT_PANEL_WIDTH  "320" CACHE STRING "Panel width (pixels)")
set(LGFX_PORT_PANEL_HEIGHT "480" CACHE STRING "Panel height (pixels)")

# Bus_SPI knobs
set(LGFX_PORT_LCD_SPI_MODE       "0"               CACHE STRING "LCD SPI mode (0..3)")
set(LGFX_PORT_LCD_FREQ_WRITE_HZ  "20*1000*1000"    CACHE STRING "LCD SPI write freq (Hz, token/expression OK)")
set(LGFX_PORT_LCD_FREQ_READ_HZ   "10*1000*1000"    CACHE STRING "LCD SPI read freq (Hz, token/expression OK)")
set(LGFX_PORT_LCD_DMA_CHANNEL    "SPI_DMA_CH_AUTO" CACHE STRING "LCD DMA channel token (e.g. SPI_DMA_CH_AUTO, 1, 2)")

option(LGFX_PORT_LCD_SPI_3WIRE "LCD SPI 3-wire mode" OFF)
option(LGFX_PORT_LCD_USE_LOCK  "LCD SPI use_lock" ON)

# Panel knobs
set(LGFX_PORT_LCD_PIN_BUSY        "-1" CACHE STRING "LCD busy pin GPIO (-1 disables)")
set(LGFX_PORT_LCD_OFFSET_X        "0"  CACHE STRING "LCD panel offset_x")
set(LGFX_PORT_LCD_OFFSET_Y        "0"  CACHE STRING "LCD panel offset_y")
set(LGFX_PORT_LCD_OFFSET_ROTATION "0"  CACHE STRING "LCD panel offset_rotation (0..7)")

set(LGFX_PORT_LCD_DUMMY_READ_PIXEL "8" CACHE STRING "LCD dummy_read_pixel")
set(LGFX_PORT_LCD_DUMMY_READ_BITS  "1" CACHE STRING "LCD dummy_read_bits")

option(LGFX_PORT_LCD_READABLE   "LCD readable" OFF)
option(LGFX_PORT_LCD_INVERT     "LCD invert colors" OFF)
option(LGFX_PORT_LCD_RGB_ORDER  "LCD rgb_order swap" OFF)
option(LGFX_PORT_LCD_DLEN_16BIT "LCD dlen_16bit" OFF)
option(LGFX_PORT_LCD_BUS_SHARED "LCD bus_shared" ON)

# Touch bus sharing (Touch_XPT2046 cfg.bus_shared)
option(LGFX_PORT_TOUCH_BUS_SHARED "Touch bus_shared" ON)

# -----------------------------------------------------------------------------
# Wiring defaults
# -----------------------------------------------------------------------------
#
# Keep defaults aligned with src/lgfx_device_state.cpp:
# - SPI:  SCLK=7 MOSI=9 MISO=8
# - LCD:  CS=43 DC=3 RST=2
# - Touch (XPT2046): CS=44 IRQ=-1 (not wired)
#
# If you set TOUCH CS to -1:
# - touch code may still compile (LGFX_PORT_ENABLE_TOUCH=ON)
# - CAP_TOUCH is suppressed (LGFX_PORT_SUPPORTS_TOUCH=0)
#
set(LGFX_PORT_SPI_SCLK_GPIO     "7"         CACHE STRING "SPI SCLK GPIO")
set(LGFX_PORT_SPI_MOSI_GPIO     "9"         CACHE STRING "SPI MOSI GPIO")
set(LGFX_PORT_SPI_MISO_GPIO     "8"         CACHE STRING "SPI MISO GPIO")

set(LGFX_PORT_LCD_SPI_HOST      "SPI2_HOST" CACHE STRING "SPI host token for LCD")
set(LGFX_PORT_LCD_CS_GPIO       "43"        CACHE STRING "LCD CS GPIO")
set(LGFX_PORT_LCD_DC_GPIO       "3"         CACHE STRING "LCD DC GPIO")
set(LGFX_PORT_LCD_RST_GPIO      "2"         CACHE STRING "LCD RST GPIO")

set(LGFX_PORT_TOUCH_CS_GPIO     "44"        CACHE STRING "XPT2046 CS GPIO; -1 disables attach")
set(LGFX_PORT_TOUCH_IRQ_GPIO    "-1"        CACHE STRING "XPT2046 IRQ GPIO; -1 disables IRQ")

# Defaults should track LCD settings (keeps touch + display aligned unless you override explicitly).
set(LGFX_PORT_TOUCH_SPI_HOST
    "${LGFX_PORT_LCD_SPI_HOST}"
    CACHE STRING "SPI host token for touch (defaults to LCD host)")
set(LGFX_PORT_TOUCH_SPI_FREQ_HZ "1000000u"  CACHE STRING "Touch SPI frequency (Hz, token)")

set(LGFX_PORT_TOUCH_OFFSET_ROTATION
    "${LGFX_PORT_LCD_OFFSET_ROTATION}"
    CACHE STRING "Touch offset_rotation (0..7); defaults to LCD offset_rotation")

# Custom minimal JP font subset (generated .cpp)
# - Keep this in-repo to avoid pulling the huge IPA font blob.
# - You can rename/move the file as you like; update this path accordingly.
set(LGFX_PORT_JP_FONT_CPP "${CMAKE_CURRENT_LIST_DIR}/src/fonts/generated/ui_font_ja_16_min.cpp"
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
    "lgfx_port/lgfx_worker_mailbox.c"

    # Port protocol / dispatch / metadata
    "lgfx_port/proto_term.c"

    # Port handlers
    "lgfx_port/handlers/setup.c"
    "lgfx_port/handlers/primitives.c"
    "lgfx_port/handlers/text.c"
    "lgfx_port/handlers/images.c"
    "lgfx_port/handlers/sprites.c"
    "lgfx_port/handlers/touch.c"

    # Device adapter core
    "src/lgfx_device_state.cpp"
    "src/lgfx_device_api.cpp"
    "src/lgfx_device_draw.cpp"
    "src/lgfx_device_sprites.cpp"
  INCLUDE_DIRS
    "include"
  PRIV_INCLUDE_DIRS
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
macro(lgfx_port_bool_to_int in_var out_var)
  set(${out_var} 0)
  if(${in_var})
    set(${out_var} 1)
  endif()
endmacro()

lgfx_port_bool_to_int(LGFX_PORT_ENABLE_JP_FONTS     LGFX_PORT_ENABLE_JP_FONTS_INT)
lgfx_port_bool_to_int(LGFX_PORT_ENABLE_TOUCH        LGFX_PORT_ENABLE_TOUCH_INT)

lgfx_port_bool_to_int(LGFX_PORT_LCD_SPI_3WIRE       LGFX_PORT_LCD_SPI_3WIRE_INT)
lgfx_port_bool_to_int(LGFX_PORT_LCD_USE_LOCK        LGFX_PORT_LCD_USE_LOCK_INT)
lgfx_port_bool_to_int(LGFX_PORT_LCD_READABLE        LGFX_PORT_LCD_READABLE_INT)
lgfx_port_bool_to_int(LGFX_PORT_LCD_INVERT          LGFX_PORT_LCD_INVERT_INT)
lgfx_port_bool_to_int(LGFX_PORT_LCD_RGB_ORDER       LGFX_PORT_LCD_RGB_ORDER_INT)
lgfx_port_bool_to_int(LGFX_PORT_LCD_DLEN_16BIT      LGFX_PORT_LCD_DLEN_16BIT_INT)
lgfx_port_bool_to_int(LGFX_PORT_LCD_BUS_SHARED      LGFX_PORT_LCD_BUS_SHARED_INT)
lgfx_port_bool_to_int(LGFX_PORT_TOUCH_BUS_SHARED    LGFX_PORT_TOUCH_BUS_SHARED_INT)

# -----------------------------------------------------------------------------
# Compute CAP_TOUCH advertisement
# -----------------------------------------------------------------------------
set(LGFX_PORT_SUPPORTS_TOUCH_INT 0)
if(LGFX_PORT_ENABLE_TOUCH)
  if(NOT "${LGFX_PORT_TOUCH_CS_GPIO}" STREQUAL "-1")
    set(LGFX_PORT_SUPPORTS_TOUCH_INT 1)
  endif()
endif()

if(LGFX_PORT_ENABLE_TOUCH AND "${LGFX_PORT_TOUCH_CS_GPIO}" STREQUAL "-1")
  message(STATUS "LGFX touch: enabled but unattached (LGFX_PORT_TOUCH_CS_GPIO=-1) -> CAP_TOUCH will NOT be advertised")
endif()

# -----------------------------------------------------------------------------
# Generate config header (build dir) from cache vars
# -----------------------------------------------------------------------------
set(LGFX_PORT_GEN_INC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LGFX_PORT_GEN_INC_DIR}/lgfx_port")

configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/include/lgfx_port/lgfx_port_config.h.in"
  "${LGFX_PORT_GEN_INC_DIR}/lgfx_port/lgfx_port_config.h"
  @ONLY
)

# Public because public headers include lgfx_port_config.h
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
file(GLOB_RECURSE LGFX_V1_COMMON_SRCS
  "${LGFX_SRC_ROOT}/LovyanGFX*.cpp"
  "${LGFX_SRC_ROOT}/lgfx/v1/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/*.cpp"
  "${LGFX_SRC_ROOT}/lgfx/v1/**/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/**/*.cpp"
)
list(FILTER LGFX_V1_COMMON_SRCS EXCLUDE REGEX ".*/lgfx/v1/platforms/.*")

file(GLOB_RECURSE LGFX_V1_ESP32_SRCS
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/*.cpp"
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/**/*.c"
  "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/**/*.cpp"
)

set(LGFX_SRCS
  ${LGFX_V1_COMMON_SRCS}
  ${LGFX_V1_ESP32_SRCS}
)

# -----------------------------------------------------------------------------
# JPEG decoder support (TJpgDec wrapper)
# -----------------------------------------------------------------------------
list(APPEND LGFX_SRCS
  "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_tjpgd.c"
)

# -----------------------------------------------------------------------------
# Japanese font data (custom minimal subset)
# -----------------------------------------------------------------------------
#
# LovyanGFX ships a large IPA JapanGothic blob, which can easily exceed the
# app partition budget on small ESP32 builds. This component compiles a
# generated subset .cpp instead.
#
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

# Avoid double-compiling if any sources overlap
list(REMOVE_DUPLICATES LGFX_SRCS)

# -----------------------------------------------------------------------------
# Reduce LovyanGFX build surface area
# -----------------------------------------------------------------------------
#
# For this component we only need:
# - SPI bus support
# - our target LCD panel (ILI9488)
# - optionally, a single touch driver (XPT2046)
#
# So we aggressively exclude:
# - EPD/e-paper buses and panels
# - I2C bus support
# - all touch drivers (we add back only Touch_XPT2046 when enabled)
#
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/Bus_EPD\\.(c|cpp)$")
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/Panel_EPD\\.(c|cpp)$")
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/panel/Panel_EPD.*\\.(c|cpp)$")
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/panel/Panel_GDEW.*\\.(c|cpp)$")

list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/Bus_I2C\\.(c|cpp)$")
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/touch/.*\\.(c|cpp)$")
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/Touch_.*\\.(c|cpp)$")

# Extra safety: exclude any I2C-related compilation units
list(FILTER LGFX_SRCS EXCLUDE REGEX ".*/[Ii]2[Cc].*\\.(c|cpp)$")

# -----------------------------------------------------------------------------
# Touch (XPT2046): re-add only the one driver we need
# -----------------------------------------------------------------------------
if(LGFX_PORT_ENABLE_TOUCH)
  list(APPEND LGFX_SRCS
    "${LGFX_SRC_ROOT}/lgfx/v1/touch/Touch_XPT2046.cpp"
  )
endif()

# Avoid double-compiling if any sources overlap (again, after touch re-add)
list(REMOVE_DUPLICATES LGFX_SRCS)

# -----------------------------------------------------------------------------
# Compile LovyanGFX as part of this component
# -----------------------------------------------------------------------------
target_sources(${COMPONENT_LIB} PRIVATE ${LGFX_SRCS})

target_include_directories(${COMPONENT_LIB} PRIVATE
  "${LGFX_SRC_ROOT}"
)

# Keep only non-config macros here; config lives in generated header.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  LGFX_USE_V1=1
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
