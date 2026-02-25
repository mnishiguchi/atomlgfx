# CMakeLists.txt

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
    # Port entrypoint / lifecycle
    "ports/lgfx_port.c"
    "ports/lgfx_worker.c"

    # Port protocol / dispatch / metadata
    "ports/lgfx_atoms.c"
    "ports/proto_term_decode.c"
    "ports/proto_term_encode.c"
    "ports/op_meta.c"
    "ports/proto_caps.c"
    "ports/dispatch_table.c"

    # Port handlers (match protocol grouping)
    "ports/handlers/control.c"
    "ports/handlers/setup.c"
    "ports/handlers/primitives.c"
    "ports/handlers/text.c"
    "ports/handlers/images.c"
    "ports/handlers/sprites.c"

    # Device adapter core
    "src/lgfx_device_state.cpp"
    "src/lgfx_device_caps.cpp"
    "src/lgfx_device_control.cpp"
    "src/lgfx_device_size.cpp"
    "src/lgfx_device_transactions.cpp"

    # Device adapter feature groups
    "src/lgfx_device_primitives.cpp"
    "src/lgfx_device_text.cpp"
    "src/lgfx_device_images.cpp"
    "src/lgfx_device_sprites.cpp"
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
