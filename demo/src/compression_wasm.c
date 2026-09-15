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
#include "wl_codec.h"

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

    /* wl_codec_compress() requires an output buffer of at least
       WL_BIP39_MAX_WORDS * 2 (two bytes per word index). */
    uint8_t raw[WL_BIP39_MAX_WORDS * 2];
    size_t out_len = 0;
    if (wl_codec_compress(WORD_LIST_BIP39, secret, in_len, raw, sizeof(raw), &out_len) != 0)
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
    if (in_len > WL_BIP39_MAX_WORDS * 2)
    {
        goto exit;
    }

    uint8_t raw[WL_BIP39_MAX_WORDS * 2];
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

    char out[WL_BIP39_MAX_WORDS * WL_MAX_WORD_LEN + WL_BIP39_MAX_WORDS + 1];
    size_t out_len = 0;
    if (wl_codec_decompress(WORD_LIST_BIP39, raw, in_len, out, sizeof(out), &out_len) != 0)
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

/**
 * @brief Compress a seed phrase + passphrase into a hex string.
 *
 * Same as wasm_bip39_compress(), but the seed phrase is followed by the
 * WL_PASSPHRASE_SEPARATOR (';') and an arbitrary passphrase which is kept
 * verbatim (only the seed phrase words are compressed).
 *
 * @param secret  Null-terminated text ("abandon zoo zoo; My Passphrase").
 * @return        Malloc'd uppercase hex string (free with _free), or NULL on
 *                error (missing separator, invalid word, ...).
 */
char *wasm_bip39_compress_passphrase(const char *secret)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char *ret = NULL;

    if (secret == NULL)
    {
        goto exit;
    }

    size_t in_len = strlen(secret);

    // compress the seed phrase (and the passphrase)
    uint8_t raw[SSS_MAX_SECRET_LEN];
    size_t out_len = 0;
    if (wl_codec_compress_passphrase(WORD_LIST_BIP39, secret, in_len, raw, sizeof(raw), &out_len) != 0)
    {
        goto exit;
    }

    // create the hex string
    char *hex = malloc(out_len * 2 + 1);
    if (hex == NULL)
    {
        goto exit;
    }

    // convert the rw byts into hex string
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
 * @brief Decompress a hex-encoded seed phrase + passphrase back to its text.
 *
 * The input is the hex string produced by wasm_bip39_compress_passphrase().
 * The output is the canonical text "<seed words> ; <passphrase>".
 *
 * @param hex  Null-terminated hex string.
 * @return     Malloc'd seed phrase (free with _free), or NULL on error.
 */
char *wasm_bip39_decompress_passphrase(const char *hex)
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
    if (in_len > SSS_MAX_SECRET_LEN)
    {
        goto exit;
    }

    // convert the hex string into rw bytes
    uint8_t raw[SSS_MAX_SECRET_LEN];
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

    // decompress the seed phrase + passphrase
    char out[SSS_MAX_SECRET_LEN * 2];
    size_t out_len = 0;
    if (wl_codec_decompress_passphrase(WORD_LIST_BIP39, raw, in_len, out, sizeof(out), &out_len) != 0)
    {
        goto exit;
    }

    // copy the decompressed seed phrase + passphrase
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

/**
 * @brief Compress a SLIP-39 mnemonic into a hex string of word indices.
 *
 * The compressed representation is two little-endian bytes per word; this
 * wrapper hex-encodes it so the result is a plain NUL-terminated string that
 * JavaScript can read back with UTF8ToString().
 *
 * @param secret  Null-terminated mnemonic ("academic zero zero").
 * @return        Malloc'd uppercase hex string (free with _free), or NULL on
 *                error (invalid word, too many words, ...).
 */
char *wasm_slip39_compress(const char *secret)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char *ret = NULL;

    if (secret == NULL)
    {
        goto exit;
    }

    size_t in_len = strlen(secret);

    /* wl_codec_compress() requires an output buffer of at least
       WL_SLIP39_MAX_WORDS * 2 (two bytes per word index). */
    uint8_t raw[WL_SLIP39_MAX_WORDS * 2];
    size_t out_len = 0;
    if (wl_codec_compress(WORD_LIST_SLIP39, secret, in_len, raw, sizeof(raw), &out_len) != 0)
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
 * @brief Decompress a hex-encoded compressed mnemonic back to its words.
 *
 * The input is the hex string produced by wasm_slip39_compress() (two
 * little-endian bytes per word index). The output is the reconstructed
 * SLIP-39 mnemonic.
 *
 * @param hex  Null-terminated hex string of the compressed word indices.
 * @return     Malloc'd mnemonic (free with _free), or NULL on error.
 */
char *wasm_slip39_decompress(const char *hex)
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
    if (in_len > WL_SLIP39_MAX_WORDS * 2)
    {
        goto exit;
    }

    uint8_t raw[WL_SLIP39_MAX_WORDS * 2];
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

    char out[WL_SLIP39_MAX_WORDS * WL_MAX_WORD_LEN + WL_SLIP39_MAX_WORDS + 1];
    size_t out_len = 0;
    if (wl_codec_decompress(WORD_LIST_SLIP39, raw, in_len, out, sizeof(out), &out_len) != 0)
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

/**
 * @brief Compress a mnemonic + passphrase into a hex string.
 *
 * Same as wasm_slip39_compress(), but the mnemonic is followed by the
 * WL_PASSPHRASE_SEPARATOR (';') and an arbitrary passphrase which is kept
 * verbatim (only the mnemonic words are compressed).
 *
 * @param secret  Null-terminated text ("academic zero zero; My Passphrase").
 * @return        Malloc'd uppercase hex string (free with _free), or NULL on
 *                error (missing separator, invalid word, ...).
 */
char *wasm_slip39_compress_passphrase(const char *secret)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char *ret = NULL;

    if (secret == NULL)
    {
        goto exit;
    }

    size_t in_len = strlen(secret);

    // compress the mnemonic (and the passphrase)
    uint8_t raw[SSS_MAX_SECRET_LEN];
    size_t out_len = 0;
    if (wl_codec_compress_passphrase(WORD_LIST_SLIP39, secret, in_len, raw, sizeof(raw), &out_len) != 0)
    {
        goto exit;
    }

    // create the hex string
    char *hex = malloc(out_len * 2 + 1);
    if (hex == NULL)
    {
        goto exit;
    }

    // convert the raw bytes into hex string
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
 * @brief Decompress a hex-encoded mnemonic + passphrase back to its text.
 *
 * The input is the hex string produced by wasm_slip39_compress_passphrase().
 * The output is the canonical text "<mnemonic words> ; <passphrase>".
 *
 * @param hex  Null-terminated hex string.
 * @return     Malloc'd mnemonic (free with _free), or NULL on error.
 */
char *wasm_slip39_decompress_passphrase(const char *hex)
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
    if (in_len > SSS_MAX_SECRET_LEN)
    {
        goto exit;
    }

    // convert the hex string into raw bytes
    uint8_t raw[SSS_MAX_SECRET_LEN];
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

    // decompress the mnemonic + passphrase
    char out[SSS_MAX_SECRET_LEN * 2];
    size_t out_len = 0;
    if (wl_codec_decompress_passphrase(WORD_LIST_SLIP39, raw, in_len, out, sizeof(out), &out_len) != 0)
    {
        goto exit;
    }

    // copy the decompressed mnemonic + passphrase
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
