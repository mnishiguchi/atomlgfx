# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

# 1.3.0 の hub と独立した翻訳単位を一度ずつ構築する。
set(LGFX_SRCS
    "${LGFX_SRC_ROOT}/lgfx/v1/lgfx_v1.cpp"
    "${LGFX_SRC_ROOT}/lgfx/v1/lgfx_v1_panel.cpp"
    "${LGFX_SRC_ROOT}/lgfx/v1/lgfx_v1_platforms.cpp"
    "${LGFX_SRC_ROOT}/lgfx/v1/LGFXBase.cpp"
    "${LGFX_SRC_ROOT}/lgfx/v1/lgfx_fonts.cpp"
    "${LGFX_SRC_ROOT}/lgfx/v1/panel/Panel_M5HDMI.cpp"
    "${LGFX_SRC_ROOT}/lgfx/Fonts/efont/lgfx_efont_cn.c"
    "${LGFX_SRC_ROOT}/lgfx/Fonts/efont/lgfx_efont_ja.c"
    "${LGFX_SRC_ROOT}/lgfx/Fonts/efont/lgfx_efont_kr.c"
    "${LGFX_SRC_ROOT}/lgfx/Fonts/efont/lgfx_efont_tw.c"
    "${LGFX_SRC_ROOT}/lgfx/Fonts/IPA/lgfx_font_japan.c"
    "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_miniz.c"
    "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_pngle.c"
    "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_qoi.c"
    "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_qrcode.c"
    "${LGFX_SRC_ROOT}/lgfx/utility/lgfx_tjpgd.c"
)
if(LGFX_PORT_ENABLE_TOUCH)
    list(APPEND LGFX_SRCS
        "${LGFX_SRC_ROOT}/lgfx/v1/lgfx_v1_touch.cpp"
        "${LGFX_SRC_ROOT}/lgfx/v1/touch/Touch_GSLx680.cpp"
    )
endif()

# AtomVM と基板制御に合わせて旧 I2C を維持する。submodule は変更しない。
if(LGFX_PORT_FORCE_LEGACY_I2C)
    set(_common "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32/common.inl")
    set(_hub "${LGFX_SRC_ROOT}/lgfx/v1/lgfx_v1_platforms.cpp")
    set(_overlay "${CMAKE_CURRENT_BINARY_DIR}/generated/lovyangfx")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_common}" "${_hub}")
    file(READ "${_common}" _original)
    string(REPLACE "__has_include(<driver/i2c_master.h>)" "0" _patched "${_original}")
    if(_patched STREQUAL _original)
        message(FATAL_ERROR "LovyanGFX の旧 I2C 切り替え位置を確認できません: ${_common}")
    endif()
    file(MAKE_DIRECTORY "${_overlay}")
    file(WRITE "${_overlay}/esp32_common.inl" "${_patched}")
    file(READ "${_hub}" _original_hub)
    string(REPLACE "\"platforms/esp32/common.inl\""
        "\"${_overlay}/esp32_common.inl\"" _patched_hub "${_original_hub}")
    if(_patched_hub STREQUAL _original_hub)
        message(FATAL_ERROR "LovyanGFX の platform hub を確認できません: ${_hub}")
    endif()
    file(WRITE "${_overlay}/lgfx_v1_platforms.cpp" "${_patched_hub}")
    list(REMOVE_ITEM LGFX_SRCS "${_hub}")
    list(APPEND LGFX_SRCS "${_overlay}/lgfx_v1_platforms.cpp")
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${LGFX_SRC_ROOT}/lgfx/v1"
        "${LGFX_SRC_ROOT}/lgfx/v1/platforms/esp32"
    )
endif()

option(LGFX_PORT_DISABLE_SPI_SCT "Disable LovyanGFX SPI SCT for diagnostics" OFF)
if(LGFX_PORT_DISABLE_SPI_SCT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LGFX_SPI_SCT_DISABLE)
endif()
