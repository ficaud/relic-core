// SPDX-License-Identifier: GPL-3.0-or-later
//
// WASM entry point — exposes QR code decoding (via ZXing-cpp) to JavaScript.
//
// This file is a thin wrapper: the actual decoding logic lives in
// src/qrcode/zxing/qr_decode_zxing.cpp. It is built ONLY for the WASM demo (see
// demo/Makefile) and is never added to the ESP32 firmware build.

#include "qr_decode.h"

#include <stdint.h>
#include <string.h>

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

/**
 * @brief Decode a QR code from a raw RGBA image.
 *
 * The caller provides a raw RGBA buffer (four bytes per pixel: R,G,B,A) and
 * its dimensions. ZXing performs the RGB->luma conversion internally, so the
 * browser can pass the frame through without a per-pixel grayscale conversion
 * in JavaScript.
 *
 * @param rgba     RGBA pixel buffer (width * height * 4 bytes).
 * @param width    Image width in pixels.
 * @param height   Image height in pixels.
 * @param out      Output buffer for the decoded payload (null-terminated).
 * @param out_size Size of the output buffer.
 *
 * @return The payload length on success (>= 0), or a negative value on error.
 */
int wasm_qr_decode_rgba(const uint8_t *rgba, int width, int height,
                        char *out, size_t out_size)
{
    struct qr_decode_ctx *ctx = qr_decode_begin(width, height, QR_DECODE_BPP_RGBA);
    if (ctx == NULL)
    {
        return -1;
    }

    uint8_t *buf = qr_decode_buffer(ctx);
    if (buf == NULL)
    {
        qr_decode_destroy(ctx);
        return -1;
    }

    memcpy(buf, rgba, (size_t)width * (size_t)height * QR_DECODE_BPP_RGBA);

    int len = qr_decode_commit(ctx, out, out_size, NULL);
    qr_decode_destroy(ctx);
    return len;
}

