# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

import Config

# Shared SPI bus pins (ESP32-S3 GPIO numbers)
config :sample_app,
  spi_config: [
    bus_config: [sclk: 7, miso: 8, mosi: 9],
    device_config: []
  ],
  sd_cs_pin: 43,
  sd_root: "/sdcard"
