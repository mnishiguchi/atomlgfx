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
# Wiring defaults (compile definitions; override from the parent project with -D...)
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
set(LGFX_PORT_SPI_SCLK_GPIO   "7"        CACHE STRING "SPI SCLK GPIO")
set(LGFX_PORT_SPI_MOSI_GPIO   "9"        CACHE STRING "SPI MOSI GPIO")
set(LGFX_PORT_SPI_MISO_GPIO   "8"        CACHE STRING "SPI MISO GPIO")
set(LGFX_PORT_LCD_SPI_HOST    "SPI2_HOST" CACHE STRING "SPI host token for LCD (e.g. SPI2_HOST)")

set(LGFX_PORT_LCD_CS_GPIO     "43"       CACHE STRING "LCD CS GPIO")
set(LGFX_PORT_LCD_DC_GPIO     "3"        CACHE STRING "LCD DC GPIO")
set(LGFX_PORT_LCD_RST_GPIO    "2"        CACHE STRING "LCD RST GPIO")

set(LGFX_PORT_TOUCH_CS_GPIO      "44"        CACHE STRING "XPT2046 CS GPIO; -1 disables attach")
set(LGFX_PORT_TOUCH_IRQ_GPIO     "-1"        CACHE STRING "XPT2046 IRQ GPIO; -1 disables IRQ")
set(LGFX_PORT_TOUCH_SPI_HOST     "SPI2_HOST" CACHE STRING "SPI host token for touch (e.g. SPI2_HOST)")
set(LGFX_PORT_TOUCH_SPI_FREQ_HZ  "1000000u"  CACHE STRING "Touch SPI frequency (Hz, token)")

# Custom minimal JP font subset (generated .cpp)
# - Keep this in-repo to avoid pulling the huge IPA font blob.
# - You can rename/move the file as you like; update this path accordingly.
set(LGFX_PORT_JP_FONT_CPP "${CMAKE_CURRENT_LIST_DIR}/src/fonts/ui_font_ja_16_min.cpp"
    CACHE STRING "Path to custom JP font subset .cpp (compiled when LGFX_PORT_ENABLE_JP_FONTS=ON)")

# -----------------------------------------------------------------------------
# Register component (must be script-mode safe)
# -----------------------------------------------------------------------------
idf_component_register(
  SRCS
    # Port entrypoint / lifecycle
    "lgfx_port/lgfx_port.c"
    "lgfx_port/lgfx_worker.c"

    # Port protocol / dispatch / metadata
    "lgfx_port/proto_term.c"

    # Port handlers (match protocol grouping)
    "lgfx_port/handlers/setup.c"
    "lgfx_port/handlers/primitives.c"
    "lgfx_port/handlers/text.c"
    "lgfx_port/handlers/images.c"
    "lgfx_port/handlers/sprites.c"
    "lgfx_port/handlers/touch.c"

    # Device adapter core
    "src/lgfx_device_state.cpp"
    "src/lgfx_device_capabilities.cpp"
    "src/lgfx_device_control.cpp"
    "src/lgfx_device_size.cpp"
    "src/lgfx_device_transactions.cpp"

    # Device adapter feature groups
    "src/lgfx_device_primitives.cpp"
    "src/lgfx_device_text.cpp"
    "src/lgfx_device_images.cpp"
    "src/lgfx_device_sprites.cpp"
    "src/lgfx_device_touch.cpp"
  INCLUDE_DIRS
    "include"
    "src"
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
# Validate LovyanGFX availability
# -----------------------------------------------------------------------------
if(NOT EXISTS "${LGFX_SRC_ROOT}/LovyanGFX.hpp")
  message(FATAL_ERROR
    "LovyanGFX not found at: ${LGFX_SRC_ROOT}\n"
    "Run: git submodule update --init --recursive"
  )
endif()

# -----------------------------------------------------------------------------
# Collect LovyanGFX sources (v1)
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
# Japanese font data (CUSTOM minimal subset)
# -----------------------------------------------------------------------------
# We intentionally do NOT compile LovyanGFX's IPA JapanGothic blob because it
# tends to blow the app partition. Instead, compile a generated subset .cpp.
if(LGFX_PORT_ENABLE_JP_FONTS)
  if(EXISTS "${LGFX_PORT_JP_FONT_CPP}")
    target_sources(${COMPONENT_LIB} PRIVATE "${LGFX_PORT_JP_FONT_CPP}")
  else()
    message(FATAL_ERROR
      "Custom JP font .cpp not found:\n  ${LGFX_PORT_JP_FONT_CPP}\n\n"
      "Set LGFX_PORT_JP_FONT_CPP to the path of your generated font subset, e.g.:\n"
      "  -DLGFX_PORT_JP_FONT_CPP=${CMAKE_CURRENT_LIST_DIR}/src/fonts/ui_font_ja_16_min.cpp\n"
    )
  endif()
endif()

# Avoid double-compiling if any sources overlap
list(REMOVE_DUPLICATES LGFX_SRCS)

# -----------------------------------------------------------------------------
# MVP exclusions (reduce build surface area)
# -----------------------------------------------------------------------------
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
# Compute CAP_TOUCH advertisement (mirrors caps.h contract)
# -----------------------------------------------------------------------------
set(LGFX_PORT_TOUCH_ATTACHED 0)
if(LGFX_PORT_ENABLE_TOUCH)
  if(NOT "${LGFX_PORT_TOUCH_CS_GPIO}" STREQUAL "-1")
    set(LGFX_PORT_TOUCH_ATTACHED 1)
  endif()
endif()

if(LGFX_PORT_ENABLE_TOUCH AND "${LGFX_PORT_TOUCH_CS_GPIO}" STREQUAL "-1")
  message(STATUS "LGFX touch: enabled but unattached (LGFX_PORT_TOUCH_CS_GPIO=-1) -> CAP_TOUCH will NOT be advertised")
endif()

# -----------------------------------------------------------------------------
# Compile LovyanGFX as part of this component
# -----------------------------------------------------------------------------
target_sources(${COMPONENT_LIB} PRIVATE ${LGFX_SRCS})

target_include_directories(${COMPONENT_LIB} PRIVATE
  "${LGFX_SRC_ROOT}"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  LGFX_USE_V1=1

  # LCD/SPI wiring defaults (override via -D...)
  LGFX_PORT_SPI_SCLK_GPIO=${LGFX_PORT_SPI_SCLK_GPIO}
  LGFX_PORT_SPI_MOSI_GPIO=${LGFX_PORT_SPI_MOSI_GPIO}
  LGFX_PORT_SPI_MISO_GPIO=${LGFX_PORT_SPI_MISO_GPIO}
  LGFX_PORT_LCD_SPI_HOST=${LGFX_PORT_LCD_SPI_HOST}

  LGFX_PORT_LCD_CS_GPIO=${LGFX_PORT_LCD_CS_GPIO}
  LGFX_PORT_LCD_DC_GPIO=${LGFX_PORT_LCD_DC_GPIO}
  LGFX_PORT_LCD_RST_GPIO=${LGFX_PORT_LCD_RST_GPIO}

  # Touch wiring defaults (override via -D...)
  LGFX_PORT_TOUCH_CS_GPIO=${LGFX_PORT_TOUCH_CS_GPIO}
  LGFX_PORT_TOUCH_IRQ_GPIO=${LGFX_PORT_TOUCH_IRQ_GPIO}
  LGFX_PORT_TOUCH_SPI_HOST=${LGFX_PORT_TOUCH_SPI_HOST}
  LGFX_PORT_TOUCH_SPI_FREQ_HZ=${LGFX_PORT_TOUCH_SPI_FREQ_HZ}

  # Override caps.h auto-gate explicitly (keeps behavior deterministic)
  LGFX_PORT_SUPPORTS_TOUCH=${LGFX_PORT_TOUCH_ATTACHED}
)

if(LGFX_PORT_ENABLE_JP_FONTS)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE LGFX_PORT_ENABLE_JP_FONTS=1)
else()
  target_compile_definitions(${COMPONENT_LIB} PRIVATE LGFX_PORT_ENABLE_JP_FONTS=0)
endif()

if(LGFX_PORT_ENABLE_TOUCH)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE LGFX_PORT_ENABLE_TOUCH=1)
else()
  target_compile_definitions(${COMPONENT_LIB} PRIVATE LGFX_PORT_ENABLE_TOUCH=0)
endif()

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
