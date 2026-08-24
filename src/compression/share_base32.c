/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file share_base32.c
 *
 * @brief Share text conversion between the "x:hex..." and "x:base32..." forms.
 *
 * @author Julien F.
 * @date 2026-08-24
 *
 * @details Converts a Shamir share between its canonical hex representation
 *          ("x:hex...") and the base32-compressed representation used as the
 *          QR payload ("x:base32..."), shrinking the QR code.
 */

#include "share_base32.h"

#include "base32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// Static function declaration
// ===========================================================================
/**
 * @brief Parse a hex string (e.g. "5a021ac0") into raw bytes.
 *
 * @param hex[in]    Null-terminated hex string (even length).
 * @param out[out]   Destination buffer (at least len/2 bytes).
 * @param out_max[in] Maximum bytes to write.
 *
 * @return Number of bytes written, or -1 on error.
 */
static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_max);

/**
 * @brief Encode raw bytes as an uppercase hex string.
 *
 * @param bytes[in]     Input bytes.
 * @param len[in]       Number of input bytes.
 * @param out[out]      Output buffer (NUL-terminated).
 * @param out_size[in]  Size of the output buffer (>= 2*len + 1).
 *
 * @return 0 on success, -1 on error.
 */
static int bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size);

// ===========================================================================
// Public function definition
// ===========================================================================
int share_to_base32(const char *text, char *out, size_t out_size)
{
    int ret = -1;

    if (text == NULL || out == NULL)
    {
        goto exit;
    }

    const char *colon = strchr(text, ':');
    if (colon == NULL || colon == text)
    {
        goto exit;
    }

    /* Validate the x-coordinate (decimal, 1..255). */
    char x_str[8];
    size_t x_len = (size_t)(colon - text);
    if (x_len >= sizeof(x_str))
    {
        goto exit;
    }
    memcpy(x_str, text, x_len);
    x_str[x_len] = '\0';

    char *endptr;
    long x_val = strtol(x_str, &endptr, 10);
    if (*endptr != '\0' || x_val < 1 || x_val > 255)
    {
        goto exit;
    }

    /* Decode the hex payload. */
    uint8_t bytes[SSS_MAX_SECRET_LEN];
    int n = hex_to_bytes(colon + 1, bytes, sizeof(bytes));
    if (n <= 0)
    {
        goto exit;
    }

    /* Encode as base32 and rebuild "x:base32". */
    char b32[SHARE_B32_BUF_SIZE];
    if (base32_encode(bytes, (size_t)n, b32, sizeof(b32)) != 0)
    {
        goto exit;
    }

    int w = snprintf(out, out_size, "%s:%s", x_str, b32);
    if (w < 0 || (size_t)w >= out_size)
    {
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

int share_from_base32(const char *text, size_t text_len, char *out, size_t out_size)
{
    int ret = -1;

    if (text == NULL || out == NULL)
    {
        goto exit;
    }

    const char *colon = memchr(text, ':', text_len);
    if (colon == NULL || colon == text)
    {
        goto exit;
    }

    /* Validate the x-coordinate (decimal, 1..255). */
    char x_str[8];
    size_t x_len = (size_t)(colon - text);
    if (x_len >= sizeof(x_str))
    {
        goto exit;
    }
    memcpy(x_str, text, x_len);
    x_str[x_len] = '\0';

    char *endptr;
    long x_val = strtol(x_str, &endptr, 10);
    if (*endptr != '\0' || x_val < 1 || x_val > 255)
    {
        goto exit;
    }

    /* Decode the base32 payload. */
    const char *b32 = colon + 1;
    size_t b32_len = text_len - x_len - 1;

    uint8_t bytes[SSS_MAX_SECRET_LEN];
    size_t n = 0;
    if (base32_decode(b32, b32_len, bytes, sizeof(bytes), &n) != 0)
    {
        goto exit;
    }

    /* Re-encode as uppercase hex and rebuild "x:hex". */
    int w = snprintf(out, out_size, "%s:", x_str);
    if (w < 0 || (size_t)w >= out_size)
    {
        goto exit;
    }
    if (bytes_to_hex(bytes, n, out + w, out_size - (size_t)w) != 0)
    {
        goto exit;
    }

    ret = 0;
exit:
    return ret;
}

// ===========================================================================
// Static function definition
// ===========================================================================
static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_max)
{
    size_t len = strlen(hex);
    if (len % 2 != 0 || len / 2 > out_max)
    {
        return -1;
    }

    for (size_t i = 0; i < len; i += 2)
    {
        char byte_str[3] = {hex[i], hex[i + 1], '\0'};
        char *endptr;
        long b = strtol(byte_str, &endptr, 16);
        if (*endptr != '\0')
        {
            return -1;
        }
        out[i / 2] = (uint8_t)b;
    }
    return (int)(len / 2);
}

static int bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    if (bytes == NULL || out == NULL || out_size < 2 * len + 1)
    {
        return -1;
    }

    for (size_t i = 0; i < len; i++)
    {
        out[2 * i] = hex_digits[(bytes[i] >> 4) & 0x0F];
        out[2 * i + 1] = hex_digits[bytes[i] & 0x0F];
    }
    out[2 * len] = '\0';

    return 0;
}
