/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file bip39.c
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

#include "bip39.h"

#include "bip39_words.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
// ===========================================================================
// Structure and variables definition
// ===========================================================================
// ===========================================================================
// Static function declarations
// ===========================================================================
/**
 * @brief Parse a seed phrase substring into its BIP-39 word indices.
 *
 * Splits @p in on spaces (skipping empty tokens), looks each token up in the
 * BIP-39 wordlist and fills @p indices with the corresponding word indices.
 *
 * @param[in]  in         Seed phrase bytes.
 * @param[in]  in_len     Number of input bytes.
 * @param[out] indices    Output array of word indices (≥ SEEDPHRASE_MAX_WORDS_COUNT entries).
 * @param[out] word_count Receives the number of parsed words.
 *
 * @return 0 on success, -EINVAL if a token is not in the wordlist, exceeds the
 *         maximum word length or exceeds the maximum word count.
 */
static int bip39_parse_words(const char *in, size_t in_len, uint16_t *indices, uint16_t *word_count);

/// ===========================================================================
// Public function definition
// ===========================================================================
int bip39_compress(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;
    uint16_t indices[SEEDPHRASE_MAX_WORDS_COUNT]; // output buffer that contains at most 24 words
    uint16_t word_count = 0;

    // Check that in respect the BIP-39 rules
    // Check that out_size and in_len respect the BIP-39 rules (for a 24 words seed phrase maximum)
    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL ||
        in_len > (SEEDPHRASE_MAX_WORDS_COUNT * SEEDPHRASE_MAX_WORD_LEN + SEEDPHRASE_MAX_WORDS_COUNT) ||
        out_size < (SEEDPHRASE_MAX_WORDS_COUNT * 2))
    {
        goto exit;
    }

    if (bip39_parse_words(in, in_len, indices, &word_count) != 0)
    {
        goto exit;
    }

    // turn indices uint16_t list into a uint8_t list
    uint16_t out_index = 0;
    for (uint16_t i = 0; i < word_count; i++)
    {
        out[out_index] = (uint8_t)(indices[i] & 0xFF);
        out_index++;
        out[out_index] = (uint8_t)(indices[i] >> 8);
        out_index++;
    }

    *out_len = out_index;
    ret = 0;

exit:
    return ret;
}

int bip39_compress_passphrase(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;
    uint16_t indices[SEEDPHRASE_MAX_WORDS_COUNT];
    uint16_t word_count = 0;

    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL)
    {
        goto exit;
    }

    // Find the first separator: seed phrase before, passphrase after.
    const char *sep = memchr(in, BIP39_PASSPHRASE_SEPARATOR, in_len);
    if (sep == NULL)
    {
        goto exit; // no separator
    }

    const char *seed_start = in;
    const char *seed_end = sep; // exclusive
    const char *pp_start = sep + 1; // passphrase start
    const char *pp_end = in + in_len; // exclusive

    // Trim surrounding whitespace (formatting) on both parts.
    // Remove the space and tabulation before and after the seed phrase.
    while (seed_start < seed_end && (*(seed_start) == ' ' || *(seed_start) == '\t'))
    {
        seed_start++;
    }
    while (seed_end > seed_start && (*(seed_end - 1) == ' ' || *(seed_end - 1) == '\t'))
    {
        seed_end--;
    }

    // Remove the space and tabulation before and after the passphrase.
    while (pp_start < pp_end && (*pp_start == ' ' || *pp_start == '\t'))
    {
        pp_start++;
    }
    while (pp_end > pp_start && (*(pp_end - 1) == ' ' || *(pp_end - 1) == '\t'))
    {
        pp_end--;
    }

    size_t seed_len = (size_t)(seed_end - seed_start);
    size_t pp_len = (size_t)(pp_end - pp_start);

    if (bip39_parse_words(seed_start, seed_len, indices, &word_count) != 0 || word_count == 0)
    {
        goto exit;
    }

    // Output: [word_count: 1 byte][word_count x 2 bytes][passphrase bytes]
    size_t total = 1 + (size_t)word_count * 2 + pp_len;
    if (out_size < total)
    {
        ret = -ENOSPC;
        goto exit;
    }

    out[0] = (uint8_t)word_count;
    size_t pos = 1;
    for (uint16_t i = 0; i < word_count; i++)
    {
        out[pos++] = (uint8_t)(indices[i] & 0xFF);
        out[pos++] = (uint8_t)(indices[i] >> 8);
    }
    memcpy(&out[pos], pp_start, pp_len);
    pos += pp_len;

    *out_len = pos;
    ret = 0;

exit:
    return ret;
}

int bip39_decompress(const uint8_t *in, size_t in_len, char *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;

    // Check that in respect the BIP-39 rules
    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL)
    {
        goto exit;
    }

    // Input is a sequence of 2-byte little-endian word indices
    if ((in_len % 2) != 0 || in_len > (SEEDPHRASE_MAX_WORDS_COUNT * 2))
    {
        goto exit;
    }

    uint16_t word_count = (uint16_t)(in_len / 2);

    // Iterate on each compressed word index and uncompress them into a seed phrase
    size_t pos = 0;
    for (uint16_t i = 0; i < word_count; i++)
    {
        uint16_t index = (uint16_t)(in[i * 2] | (in[i * 2 + 1] << 8));
        if (index >= BIP39_WORD_COUNT)
        {
            goto exit;
        }

        const char *word = bip39_words[index];
        size_t word_len = strlen(word);
        size_t needed = word_len + (i > 0 ? 1 : 0);

        if (pos + needed + 1 > out_size)
        {
            ret = -ENOSPC;
            goto exit;
        }

        if (i > 0)
        {
            out[pos++] = ' ';
        }
        memcpy(&out[pos], word, word_len);
        pos += word_len;
    }

    out[pos] = '\0';
    *out_len = pos;

    ret = 0;

exit:
    return ret;
}

int bip39_decompress_passphrase(const uint8_t *in, size_t in_len, char *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;

    if (in == NULL || in_len < 1 || out == NULL || out_len == NULL)
    {
        goto exit;
    }

    // Format: [word_count: 1 byte][word_count x 2 bytes][passphrase bytes]
    uint16_t word_count = in[0];
    if (word_count == 0 || word_count > SEEDPHRASE_MAX_WORDS_COUNT)
    {
        goto exit;
    }

    size_t index_bytes = (size_t)word_count * 2;
    if (in_len < 1 + index_bytes)
    {
        goto exit;
    }

    const uint8_t *pp_start = in + 1 + index_bytes;
    size_t pp_len = in_len - 1 - index_bytes;

    // Reconstruct the seed phrase words (space-separated).
    size_t pos = 0;
    for (uint16_t i = 0; i < word_count; i++)
    {
        uint16_t index = (uint16_t)(in[1 + i * 2] | (in[1 + i * 2 + 1] << 8));
        if (index >= BIP39_WORD_COUNT)
        {
            goto exit;
        }

        const char *word = bip39_words[index];
        size_t word_len = strlen(word);
        size_t needed = word_len + (i > 0 ? 1 : 0);

        if (pos + needed + 1 > out_size)
        {
            ret = -ENOSPC;
            goto exit;
        }

        if (i > 0)
        {
            out[pos++] = ' ';
        }
        memcpy(&out[pos], word, word_len);
        pos += word_len;
    }

    // Append the canonical separator and the passphrase (if any).
    if (pp_len > 0)
    {
        if (pos + 3 + pp_len + 1 > out_size)
        {
            ret = -ENOSPC;
            goto exit;
        }
        out[pos++] = ' ';
        out[pos++] = BIP39_PASSPHRASE_SEPARATOR;
        out[pos++] = ' ';
        memcpy(&out[pos], pp_start, pp_len);
        pos += pp_len;
    }

    out[pos] = '\0';
    *out_len = pos;

    ret = 0;

exit:
    return ret;
}
// ===========================================================================
// Static function definition
// ===========================================================================

static int bip39_parse_words(const char *in, size_t in_len, uint16_t *indices, uint16_t *word_count)
{
    uint16_t count = 0;
    size_t i = 0;

    while (i < in_len)
    {
        char word[SEEDPHRASE_MAX_WORD_LEN + 1];
        uint8_t wlen = 0;

        while (i < in_len && in[i] != ' ')
        {
            if (wlen >= SEEDPHRASE_MAX_WORD_LEN)
            {
                return -EINVAL; // word longer than the BIP-39 maximum
            }
            word[wlen++] = in[i++];
        }
        word[wlen] = '\0';

        if (wlen == 0)
        {
            i++; // skip empty token (double space, leading space)
            continue;
        }

        int found = 0;
        for (uint16_t j = 0; j < BIP39_WORD_COUNT; j++)
        {
            if (strcmp(word, bip39_words[j]) == 0)
            {
                if (count >= SEEDPHRASE_MAX_WORDS_COUNT)
                {
                    return -EINVAL; // more words than the BIP-39 maximum
                }
                indices[count++] = j;
                found = 1;
                break;
            }
        }

        if (!found)
        {
            return -EINVAL; // word not in the BIP-39 wordlist
        }

        if (i < in_len)
        {
            i++; // skip the single space separator after a word
        }
    }

    *word_count = count;
    return 0;
}
