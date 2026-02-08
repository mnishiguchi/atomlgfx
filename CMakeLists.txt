# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
set(LGFX_ROOT     "${CMAKE_CURRENT_LIST_DIR}/third_party/LovyanGFX")
set(LGFX_SRC_ROOT "${LGFX_ROOT}/src")

# -----------------------------------------------------------------------------
# Register component (must be script-mode safe)
# -----------------------------------------------------------------------------
idf_component_register(
  SRCS
    "ports/lgfx_port.c"
    "src/lgfx_device.cpp"

  INCLUDE_DIRS
    "ports/include"
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
# Compile LovyanGFX as part of this component
# -----------------------------------------------------------------------------
target_sources(${COMPONENT_LIB} PRIVATE ${LGFX_SRCS})

target_include_directories(${COMPONENT_LIB} PRIVATE
  "${LGFX_SRC_ROOT}"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  LGFX_USE_V1=1
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

