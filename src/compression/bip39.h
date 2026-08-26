/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file bip39.h
 *
 * @brief
 *
 * @author Julien F.
 * @date 2026-08-25
 *
 * @details Use the BIP-39 words list to replace a seed phrase with its word's index.
 *
 *          For example, word "abandon" is at index "0001", word "zoo" at index "2048".
 *          This means the result of a seed phrase compression is an hex that represents all the seed phrase
 *          words.
 *
 *          For example, the seed phrase "abandon zoo zoo" is compressed to "0x000108000800" turning 16 bytes into 6
 *          hex words.
 */

#ifndef BIP39_H
#define BIP39_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/// ===========================================================================
// Definitions
// ===========================================================================
#define SEEDPHRASE_MAX_WORDS_COUNT (24) // we want seed phrasee up to 24 words
#define SEEDPHRASE_MAX_WORD_LEN    (8) // the longest BIP-39 word is 8 characters
#define BIP39_PASSPHRASE_SEPARATOR ';' // separator between the seed phrase and its optional passphrase
// ===========================================================================
// Public function declaration
// ===========================================================================
/*
 * Compress a seed phrase (BIP-39) into a smaller representation.
 *
 * The output is a hex string that represents all the seed phrase words
 * concatenated index.
 *
 * @param[in]  in        Input bytes (seed phrase that respects BIP-39 rules).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output buffer that represents each seed phrase word
 *                       index as two little-endian bytes.
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of bytes written to @p out.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int bip39_compress(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len);

/*
 * Decompress a seed phrase (BIP-39) from a smaller representation.
 *
 * The input is a hex string that represents all the seed phrase words index concatenated.
 *
 * @param[in]  in        Input hex string.
 * @param[in]  in_len    Number of input characters (excluding any NUL).
 * @param[out] out       Output seed phrase bytyes (that respects BIP-39 rules).
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of decoded bytes written.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int bip39_decompress(const uint8_t *in, size_t in_len, char *out, size_t out_size, size_t *out_len);

/*
 * Compress a seed phrase (BIP-39) followed by an optional passphrase.
 *
 * The input is the raw text "seed phrase" optionally followed by a
 * BIP39_PASSPHRASE_SEPARATOR (';') and a free-form passphrase:
 *
 *     "abandon zoo zoo; My Passphrase"
 *
 * The seed phrase (everything before the first separator) is compressed into
 * its word indices; the passphrase (everything after the first separator) is
 * kept verbatim (only surrounding whitespace is trimmed). The output format is:
 *
 *     [word_count: 1 byte][word_count x 2 little-endian bytes][passphrase bytes]
 *
 * so the decompressor always knows exactly where the seed phrase ends.
 *
 * @param[in]  in        Input bytes (seed phrase optionally + ';' + passphrase).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output buffer (word_count byte + word indices + passphrase).
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of bytes written to @p out.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int bip39_compress_passphrase(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len);

/*
 * Decompress a seed phrase + passphrase from the passphrase-aware form.
 *
 * The input is the format produced by bip39_compress_passphrase(): a leading
 * word_count byte, the word indices (two little-endian bytes each), then the
 * raw passphrase bytes. The output is the canonical text:
 *
 *     "<seed words joined by spaces> ; <passphrase>"
 *
 * (the " ; " separator is omitted when the passphrase is empty).
 *
 * @param[in]  in        Input bytes (word_count + indices + passphrase).
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output text buffer.
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of bytes written to @p out.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int bip39_decompress_passphrase(const uint8_t *in, size_t in_len, char *out, size_t out_size, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* BIP39_H */
