# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20.0)

option(BUILD_TESTS "Build unit tests with Google Test" OFF)

# ─────────────────────────────────────────────────────────────────────────────
#  Dispatch: native unit tests  vs  Zephyr firmware
# ─────────────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)

  project(relic-core-tests LANGUAGES C CXX)
  enable_testing()
  add_subdirectory(tests)

else()

  # Explicitly list Zephyr modules to avoid the module scanner picking up
  # the self project (root) which contains zephyr/ as a subdirectory.
  set(ZEPHYR_MODULES
      ${CMAKE_CURRENT_SOURCE_DIR}/modules/hal/espressif
      ${CMAKE_CURRENT_SOURCE_DIR}/modules/crypto/mbedtls
      ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/mcuboot
      ${CMAKE_CURRENT_SOURCE_DIR}/modules/lib/picolibc
      ${CMAKE_CURRENT_SOURCE_DIR}/modules/crypto/tf-psa-crypto
      ${CMAKE_CURRENT_SOURCE_DIR}/modules/lib/zcbor
  )

  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(wifi_apsta_mode)

  target_sources(app PRIVATE
      src/main.c
      src/access-point/http/src/http_server.c
      src/access-point/http/src/http_handlers.c
      src/access-point/http/src/http_types.c
      src/access-point/http/src/http_router.c
      src/access-point/net/src/dns.c
      src/access-point/net/src/dhcp.c
      src/access-point/wifi/src/wifi_mgr.c
      src/sss/sss.c
      src/compression/base32.c
      src/qrcode/qr_encode.c
      src/svg/qrcode_to_svg.c
  )

  # On-device QR decoding (quirc). Only enabled where the board has enough
  # RAM (ESP32-S3). The classic ESP32 (DevKit V1) keeps the decoder out of the
  # firmware entirely and the web page falls back to local jsQR decoding.
  if(CONFIG_RELIC_QR_DECODE_SERVER)
    target_sources(app PRIVATE
        src/qrcode/qr_decode.c
        external/quirc/lib/quirc.c
        external/quirc/lib/decode.c
        external/quirc/lib/identify.c
        external/quirc/lib/version_db.c
    )
    target_include_directories(app PRIVATE external/quirc/lib)
    target_compile_definitions(app PRIVATE
        # Per-board QR decode image limit (Kconfig: RELIC_QR_DECODE_MAX_DIM).
        QR_DECODE_MAX_DIM=${CONFIG_RELIC_QR_DECODE_MAX_DIM}
        # Shrink quirc's fixed-size buffers for the firmware:
        # - QUIRC_MAX_PAYLOAD: QR shares are at most a few hundred bytes
        #   (the WASM demo links without this and keeps the 8896-byte limit).
        # - QUIRC_MAX_CAPSTONES / QUIRC_MAX_GRIDS: the device decodes a single
        #   QR share per image, so a small finder-pattern pool is enough. This
        #   cuts ~9 KB off quirc's heap footprint, letting a 224x224 image fit.
        QUIRC_MAX_PAYLOAD=2048
        QUIRC_MAX_CAPSTONES=8
        QUIRC_MAX_GRIDS=16
        # The ESP32-S3 FPU is single-precision only; double is emulated in
        # software. quirc's perspective mapping runs per grid cell, so using
        # float cuts the decode from ~2 s down to tens of milliseconds.
        QUIRC_FLOAT_TYPE=float
    )
  endif()

  # Auto-generate page_captive.h from assets (HTML + CSS)
  set(ASSETS_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/src/access-point/assets)
  set(ASSETS_OUTPUT ${ASSETS_DIR}/page_captive.h)

  if(CONFIG_RELIC_QR_DECODE_SERVER)
    set(QR_DECODE_SERVER_FLAG 1)
  else()
    set(QR_DECODE_SERVER_FLAG 0)
  endif()

  add_custom_command(
      OUTPUT  ${ASSETS_OUTPUT}
      COMMAND ${PYTHON_EXECUTABLE}
              ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-assets.py
              --qr-max-dim ${CONFIG_RELIC_QR_DECODE_MAX_DIM}
              --qr-decode-server ${QR_DECODE_SERVER_FLAG}
              ${ASSETS_DIR}
              ${ASSETS_OUTPUT}
      DEPENDS ${ASSETS_DIR}/index.html
              ${ASSETS_DIR}/split.html
              ${ASSETS_DIR}/unsplit.html
              ${ASSETS_DIR}/style.css
              ${ASSETS_DIR}/scripts/split.js
              ${ASSETS_DIR}/scripts/unsplit.js
              ${ASSETS_DIR}/scripts/jsQR.js
              ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-assets.py
      COMMENT "Generating page_captive.h from assets..."
  )

  target_sources(app PRIVATE ${ASSETS_OUTPUT})

  target_include_directories(app PRIVATE
      src/access-point/http/inc
      src/access-point/net/inc
      src/access-point/wifi/inc
      src/access-point/assets
      src/sss
      src/compression
      src/qrcode
      src/svg
  )

endif()
