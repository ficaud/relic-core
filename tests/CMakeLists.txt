# SPDX-License-Identifier: GPL-3.0-or-later

# Allow this file to be used both as a standalone project
# (cmake -S tests -B tests/build) and as a subdirectory of the
# root dispatcher (cmake -S . -B build -DBUILD_TESTS=ON).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20.0)
    project(relic-core-tests LANGUAGES C CXX)
    enable_testing()
endif()

# ---------------------------------------------------------------------------
# Google Test (from system packages)
# ---------------------------------------------------------------------------
find_package(GTest REQUIRED)

# ---------------------------------------------------------------------------
# sss_ref — dsprenkels/sss (hazmat API) for cross-validation
# ---------------------------------------------------------------------------
set(SSS_REF_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/../external/sss)
set(SSS_RANDOMBYTES_DIR ${SSS_REF_DIR}/randombytes)

# ---------------------------------------------------------------------------
# Source paths
# ---------------------------------------------------------------------------
set(SSS_SRC_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/../src/sss)
set(QRCODE_SRC_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/../src/qrcode)
set(SVG_SRC_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/../src/svg)
set(COMPRESSION_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/compression)

# ---------------------------------------------------------------------------
# sss_test — Shamir + QR Code tests (combined executable)
# ---------------------------------------------------------------------------
add_executable(sss_test
    main.cpp
    test_sss.cpp
    test_qrcode.cpp
    mockup/sys_rand_stub.c                   # Linux stub for Zephyr's sys_rand_get()
    ${SSS_SRC_DIR}/sss.c                      # Our Shamir implementation
    ${QRCODE_SRC_DIR}/qr_encode.c             # QR Code generator
    ${SSS_REF_DIR}/hazmat.c                      # sss_ref low-level SSS
    ${SSS_RANDOMBYTES_DIR}/randombytes.c         # dsprenkels/randombytes
)

target_include_directories(sss_test PRIVATE
    ${SSS_SRC_DIR}                               # for shamir.h
    ${QRCODE_SRC_DIR}                            # for qr_encode.h
    ${SSS_REF_DIR}                               # for hazmat.h
    ${SSS_RANDOMBYTES_DIR}                       # for randombytes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs            # for <zephyr/random/random.h>
)

target_link_libraries(sss_test PRIVATE
    GTest::GTest
    GTest::Main
)

# ---------------------------------------------------------------------------
# base32_test — Base32 codec tests only (standalone executable)
# Build with: cmake --build tests/build-native --target base32_test
# ---------------------------------------------------------------------------
add_executable(base32_test
    main.cpp
    test_base32.cpp
    ${COMPRESSION_SRC_DIR}/base32.c           # Base32 codec
)

target_include_directories(base32_test PRIVATE
    ${COMPRESSION_SRC_DIR}                       # for base32.h
)

target_link_libraries(base32_test PRIVATE
    GTest::GTest
    GTest::Main
)

# ---------------------------------------------------------------------------
# share_base32_test — Share "x:hex <-> x:base32" codec tests (standalone)
# Build with: cmake --build tests/build-native --target share_base32_test
# ---------------------------------------------------------------------------
add_executable(share_base32_test
    main.cpp
    test_share_base32.cpp
    ${COMPRESSION_SRC_DIR}/share_base32.c           # Share text codec
    ${COMPRESSION_SRC_DIR}/base32.c                 # Base32 codec
)

target_include_directories(share_base32_test PRIVATE
    ${COMPRESSION_SRC_DIR}                       # for share_base32.h
    ${SSS_SRC_DIR}                               # for sss.h (SSS_MAX_SECRET_LEN)
)

target_link_libraries(share_base32_test PRIVATE
    GTest::GTest
    GTest::Main
)

# ---------------------------------------------------------------------------
# qrcode_test — QR Code tests only (standalone executable)
# Build with: cmake --build tests/build-native --target qrcode_test
# ---------------------------------------------------------------------------
add_executable(qrcode_test
    main.cpp
    test_qrcode.cpp
    ${QRCODE_SRC_DIR}/qr_encode.c             # QR Code generator
)

target_include_directories(qrcode_test PRIVATE
    ${QRCODE_SRC_DIR}                            # for qr_encode.h
)

target_link_libraries(qrcode_test PRIVATE
    GTest::GTest
    GTest::Main
)

# ---------------------------------------------------------------------------
# svg_test — QR Code → SVG conversion tests only (standalone executable)
# Build with: cmake --build tests/build-native --target svg_test
# ---------------------------------------------------------------------------
add_executable(svg_test
    main.cpp
    test_svg.cpp
    ${QRCODE_SRC_DIR}/qr_encode.c             # QR Code generator
    ${SVG_SRC_DIR}/qrcode_to_svg.c            # QR Code → SVG converter
)

target_include_directories(svg_test PRIVATE
    ${QRCODE_SRC_DIR}                            # for qr_encode.h
    ${SVG_SRC_DIR}                               # for qrcode_to_svg.h
)

# Absolute path to the golden reference SVG directory so the tests can locate
# the seed-phrase share reference files regardless of the working directory.
target_compile_definitions(svg_test PRIVATE
    REFERENCE_SVG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/reference/svg"
)

target_link_libraries(svg_test PRIVATE
    GTest::GTest
    GTest::Main
)

# ---------------------------------------------------------------------------
# Register tests with ctest
# ---------------------------------------------------------------------------
add_test(NAME sss_test COMMAND sss_test)

# Launch the Base32 codec unit tests (Base32Test suite) via the dedicated binary.
add_test(NAME base32_test COMMAND base32_test)

# Launch the share base32 codec unit tests (ShareBase32Test suite).
add_test(NAME share_base32_test COMMAND share_base32_test)

# Launch the QR code unit tests (QRCodeTest suite) via the dedicated binary.
add_test(NAME qrcode_test COMMAND qrcode_test)

# Launch the SVG unit tests (SVGTest suite) via the dedicated binary.
add_test(NAME svg_test COMMAND svg_test)
