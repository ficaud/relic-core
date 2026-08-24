// SPDX-License-Identifier: GPL-3.0-or-later
//
// WASM entry point — exposes QR code decoding (via quirc) to JavaScript.
//
// This file is a thin wrapper: the actual decoding logic lives in
// src/qrcode/qr_decode.c. It is built ONLY for the WASM demo (see
// demo/Makefile) and is never added to the ESP32 firmware build.

#include "qr_decode.h"
#include "share_base32.h"

#include <stdint.h>
#include <stdlib.h>
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
 * @brief Decompress a base32 QR payload ("x:base32...") back to hex ("x:hex...").
 *
 * Reuses the same share_base32 codec as the embedded firmware so the demo can
 * consume QR codes produced by the device (and vice-versa).
 *
 * @param b32_text  Null-terminated share text ("x:base32...").
 * @return          Malloc'd hex text (free with _free), or NULL on error.
 */
char *wasm_share_from_base32(const char *b32_text)
{
    char *ret = NULL;

    if (b32_text == NULL)
    {
        goto exit;
    }

    /* "x:" prefix (up to 3 digits + ':') + hex payload (2 * 256) + NUL. */
    const size_t hex_len = SSS_MAX_SECRET_LEN * 2 + 5;
    char *out = malloc(hex_len);
    if (out == NULL)
    {
        goto exit;
    }

    if (share_from_base32(b32_text, strlen(b32_text), out, hex_len) != 0)
    {
        free(out);
        goto exit;
    }

    ret = out;

exit:
    return ret;
}

