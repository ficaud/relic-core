// SPDX-License-Identifier: GPL-3.0-or-later
//
// WASM entry point — exposes QR code decoding (via quirc) to JavaScript.
//
// This file is a thin wrapper: the actual decoding logic lives in
// src/qrcode/qr_decode.c. It is built ONLY for the WASM demo (see
// demo/Makefile) and is never added to the ESP32 firmware build.

#include "qr_decode.h"

#include <stdint.h>

/**
 * @brief Decode a QR code from a grayscale image.
 *
 * The caller provides a raw grayscale buffer (one byte per pixel) and its
 * dimensions. On success the decoded payload is written to @p out as a
 * null-terminated string and its length is returned.
 *
 * @param gray     Grayscale pixel buffer (width * height bytes).
 * @param width    Image width in pixels.
 * @param height   Image height in pixels.
 * @param out      Output buffer for the decoded payload (null-terminated).
 * @param out_size Size of the output buffer.
 *
 * @return The payload length on success (>= 0), or a negative value on error.
 */
int wasm_qr_decode(const uint8_t *gray, int width, int height,
                   char *out, size_t out_size)
{
    return qr_decode_gray(gray, width, height, out, out_size);
}

