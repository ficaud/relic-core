/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file wl_codec.h
 *
 * @brief Word list codec handling both the BIP-39 and SLIP-39 word lists.
 *
 * @author Julien F.
 * @date 2026-09-15
 *
 * @details This header defines the generic word-list codec API. The "wl" stands for
 *          "word list": these functions convert a seed phrase / mnemonic into its word
 *          indices and back, using a word list selected at runtime (BIP-39 or SLIP-39).
 */

#ifndef WL_CODEC_H
#define WL_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/// ===========================================================================
// Type definitions
// ===========================================================================
/**
 * @brief Seed phrase (or mnemonic) configuration.
 */
typedef struct _seedphrase_config_t
{
    uint8_t max_word_count; ///< Maximum number of words in the phrase.
    uint8_t max_word_len; ///< Length of the longest word (excluding NUL).
    char passphrase_separator; ///< Separator between the phrase and its optional passphrase.
} seedphrase_config_t;

/**
 * @brief Word list configuration.
 */
typedef struct _word_list_config_t
{
    uint16_t list_word_count; ///< Number of words in the list.
    const char *const *list_words; ///< Sorted word list (word -> index).
} word_list_config_t;

/**
 * @brief Word list codec configuration: a word list plus its seed phrase constraints.
 */
typedef struct _wl_codec_t
{
    word_list_config_t wl_config;
    seedphrase_config_t sp_config;
} wl_codec_t;

/**
 * @brief Word list selector.
 */
typedef enum
{
    WORD_LIST_BIP39 = 0,
    WORD_LIST_SLIP39,
    WORD_LIST_COUNT,
} word_list_e;

/// ===========================================================================
// Definitions
// ===========================================================================
#define WL_BIP39_MAX_WORDS      (24) // BIP-39 seed phrase up to 24 words
#define WL_SLIP39_MAX_WORDS     (33) // SLIP-39 mnemonic up to 33 words
#define WL_MAX_WORD_LEN         (8) // the longest word (BIP-39 and SLIP-39) is 8 characters
#define WL_PASSPHRASE_SEPARATOR ';' // separator between the phrase and its optional passphrase

/* Largest max_word_count across all word lists, used to size generic stack buffers. */
#define WL_MAX_WORD_COUNT WL_SLIP39_MAX_WORDS

// ===========================================================================
// Public function declaration
// ===========================================================================
/*
 * Compress a seed phrase / mnemonic into a smaller representation.
 *
 * The output is a byte sequence that represents all the phrase words as two
 * little-endian bytes per word index.
 *
 * @param[in]  list      Word list to use (WORD_LIST_BIP39 or WORD_LIST_SLIP39).
 * @param[in]  in        Input bytes (phrase that respects the word list rules).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output buffer (two little-endian bytes per word index).
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of bytes written to @p out.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int wl_codec_compress(word_list_e list, const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len);

/*
 * Decompress a seed phrase / mnemonic from a smaller representation.
 *
 * @param[in]  list      Word list to use (WORD_LIST_BIP39 or WORD_LIST_SLIP39).
 * @param[in]  in        Input bytes (two little-endian bytes per word index).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output phrase bytes (that respects the word list rules).
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of decoded bytes written.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int wl_codec_decompress(word_list_e list,
                        const uint8_t *in,
                        size_t in_len,
                        char *out,
                        size_t out_size,
                        size_t *out_len);

/*
 * Compress a phrase followed by an optional passphrase.
 *
 * The input is the raw text "phrase" optionally followed by a
 * WL_PASSPHRASE_SEPARATOR (';') and a free-form passphrase. The phrase (everything
 * before the first separator) is compressed into its word indices; the passphrase
 * (everything after the first separator) is kept verbatim (surrounding whitespace
 * trimmed). The output format is:
 *
 *     [word_count: 1 byte][word_count x 2 little-endian bytes][passphrase bytes]
 *
 * @param[in]  list      Word list to use (WORD_LIST_BIP39 or WORD_LIST_SLIP39).
 * @param[in]  in        Input bytes (phrase optionally + ';' + passphrase).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output buffer (word_count byte + word indices + passphrase).
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of bytes written to @p out.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int wl_codec_compress_passphrase(word_list_e list,
                                 const char *in,
                                 size_t in_len,
                                 uint8_t *out,
                                 size_t out_size,
                                 size_t *out_len);

/*
 * Decompress a phrase + passphrase from the passphrase-aware form.
 *
 * The input is the format produced by wl_codec_compress_passphrase(). The output is
 * the canonical text "<words joined by spaces> ; <passphrase>" (the " ; " separator is
 * omitted when the passphrase is empty).
 *
 * @param[in]  list      Word list to use (WORD_LIST_BIP39 or WORD_LIST_SLIP39).
 * @param[in]  in        Input bytes (word_count + indices + passphrase).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output text buffer.
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of bytes written to @p out.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int wl_codec_decompress_passphrase(word_list_e list,
                                   const uint8_t *in,
                                   size_t in_len,
                                   char *out,
                                   size_t out_size,
                                   size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* WL_CODEC_H */
