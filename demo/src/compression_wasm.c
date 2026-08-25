/**
 * @file compression_wasm.c
 *
 * @brief WASM entry point — exposes the share/base32 and BIP-39 codecs to
 *        JavaScript.
 *
 * @author Julien F.
 * @date 2026-08-26
 *
 * @details Thin wrappers around the src/compression codecs (share_base32,
 *          bip39). They reuse the exact same C code as the embedded firmware
 *          so the demo and the device produce byte-identical payloads.
 *
 *          All functions return malloc'd NUL-terminated strings (or NULL on
 *          error); the caller frees them with _free().
 */

#include "share_base32.h"
#include "bip39.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── Public WASM API ── */

/**
 * @brief Decode a hex nibble into its 4-bit value.
 *
 * @param c  Hex digit ('0'-'9', 'a'-'f', 'A'-'F').
 * @return   0..15, or -1 if @p c is not a hex digit.
 */
static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

/**
 * @brief Compress a share ("x:hex...") to its base32 QR payload ("x:base32...").
 *
 * @param hex_text  Null-terminated share text ("x:hex...").
 * @return          Malloc'd base32 text (free with _free), or NULL on error.
 */
char *wasm_share_to_base32(const char *hex_text)
{
    char *ret = NULL;

    if (hex_text == NULL)
    {
        goto exit;
    }

    char *out = malloc(SHARE_B32_BUF_SIZE + 4); /* "x:" prefix + payload + NUL */
    if (out == NULL)
    {
        goto exit;
    }

    if (share_to_base32(hex_text, out, SHARE_B32_BUF_SIZE + 4) != 0)
    {
        free(out);
        goto exit;
    }

    ret = out;

exit:
    return ret;
}

/**
 * @brief Decompress a base32 QR payload ("x:base32...") back to hex ("x:hex...").
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

/**
 * @brief Compress a BIP-39 seed phrase into a hex string of word indices.
 *
 * The compressed representation is two little-endian bytes per word; this
 * wrapper hex-encodes it so the result is a plain NUL-terminated string that
 * JavaScript can read back with UTF8ToString().
 *
 * @param secret  Null-terminated seed phrase ("abandon zoo zoo").
 * @return        Malloc'd uppercase hex string (free with _free), or NULL on
 *                error (invalid word, too many words, ...).
 */
char *wasm_bip39_compress(const char *secret)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char *ret = NULL;

    if (secret == NULL)
    {
        goto exit;
    }

    size_t in_len = strlen(secret);

    /* bip39_compress() requires an output buffer of at least
       SEEDPHRASE_MAX_WORDS_COUNT * 2 (two bytes per word index). */
    uint8_t raw[SEEDPHRASE_MAX_WORDS_COUNT * 2];
    size_t out_len = 0;
    if (bip39_compress(secret, in_len, raw, sizeof(raw), &out_len) != 0)
    {
        goto exit;
    }

    char *hex = malloc(out_len * 2 + 1);
    if (hex == NULL)
    {
        goto exit;
    }

    for (size_t i = 0; i < out_len; i++)
    {
        hex[i * 2] = hex_digits[raw[i] >> 4];
        hex[i * 2 + 1] = hex_digits[raw[i] & 0x0F];
    }
    hex[out_len * 2] = '\0';

    ret = hex;

exit:
    return ret;
}

/**
 * @brief Decompress a hex-encoded compressed seed phrase back to its words.
 *
 * The input is the hex string produced by wasm_bip39_compress() (two
 * little-endian bytes per word index). The output is the reconstructed
 * BIP-39 seed phrase.
 *
 * @param hex  Null-terminated hex string of the compressed word indices.
 * @return     Malloc'd seed phrase (free with _free), or NULL on error.
 */
char *wasm_bip39_decompress(const char *hex)
{
    char *ret = NULL;

    if (hex == NULL)
    {
        goto exit;
    }

    size_t hex_len = strlen(hex);
    if ((hex_len % 2) != 0)
    {
        goto exit;
    }

    size_t in_len = hex_len / 2;
    if (in_len > SEEDPHRASE_MAX_WORDS_COUNT * 2)
    {
        goto exit;
    }

    uint8_t raw[SEEDPHRASE_MAX_WORDS_COUNT * 2];
    for (size_t i = 0; i < in_len; i++)
    {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
        {
            goto exit;
        }
        raw[i] = (uint8_t)((hi << 4) | lo);
    }

    char out[SEEDPHRASE_MAX_WORDS_COUNT * SEEDPHRASE_MAX_WORD_LEN + SEEDPHRASE_MAX_WORDS_COUNT + 1];
    size_t out_len = 0;
    if (bip39_decompress(raw, in_len, out, sizeof(out), &out_len) != 0)
    {
        goto exit;
    }

    char *copy = malloc(out_len + 1);
    if (copy == NULL)
    {
        goto exit;
    }
    memcpy(copy, out, out_len + 1);

    ret = copy;

exit:
    return ret;
}
