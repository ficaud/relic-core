/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file wl_codec.c
 *
 * @brief Word list codec handling both the BIP-39 and SLIP-39 word lists.
 *
 * @author Julien F.
 * @date 2026-09-15
 *
 * @details This file contains the generic word-list parsing and compression /
 *          decompression functions. The "wl" stands for "word list": these functions
 *          are used along with word lists to convert a word into its index. At the
 *          moment, BIP-39 and SLIP-39 word lists are supported.
 */

#include "wl_codec.h"

#include "bip39_words.h"
#include "slip39_words.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

// ===========================================================================
// Structure and variables definition
// ===========================================================================
static const wl_codec_t wl_codec_list[WORD_LIST_COUNT] = {
    [WORD_LIST_BIP39] =
        {
            .wl_config =
                {
                    .list_word_count = BIP39_WORD_COUNT,
                    .list_words = bip39_words,
                },
            .sp_config =
                {
                    .max_word_count = WL_BIP39_MAX_WORDS,
                    .max_word_len = WL_MAX_WORD_LEN,
                    .passphrase_separator = WL_PASSPHRASE_SEPARATOR,
                },
        },
    [WORD_LIST_SLIP39] =
        {
            .wl_config =
                {
                    .list_word_count = SLIP39_WORD_COUNT,
                    .list_words = slip39_words,
                },
            .sp_config =
                {
                    .max_word_count = WL_SLIP39_MAX_WORDS,
                    .max_word_len = WL_MAX_WORD_LEN,
                    .passphrase_separator = WL_PASSPHRASE_SEPARATOR,
                },
        },
    // Add more here is needed
};

// ===========================================================================
// Static function declarations
// ===========================================================================
/**
 * @brief Return the codec configuration for a word list, or NULL if invalid.
 *
 * @param[in] list  Word list selector.
 *
 * @return Pointer to the configuration, or NULL if @p list is out of range.
 */
static const wl_codec_t *wl_codec_cfg(word_list_e list);

/**
 * @brief Parse a phrase substring into its word indices.
 *
 * Splits @p in on spaces (skipping empty tokens), looks each token up in the
 * word list and fills @p indices with the corresponding word indices.
 *
 * @param[in]  cfg        Word list codec configuration.
 * @param[in]  in         Phrase bytes.
 * @param[in]  in_len     Number of input bytes.
 * @param[out] indices    Output array of word indices.
 * @param[out] word_count Receives the number of parsed words.
 *
 * @return 0 on success, -EINVAL if a token is not in the word list, exceeds the
 *         maximum word length or exceeds the maximum word count.
 */
static int wl_codec_parse_words(const wl_codec_t *cfg,
                                const char *in,
                                size_t in_len,
                                uint16_t *indices,
                                uint16_t *word_count);

/// ===========================================================================
// Public function definition
// ===========================================================================
int wl_codec_compress(word_list_e list, const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;
    uint16_t indices[WL_MAX_WORD_COUNT]; // output buffer that contains at most 33 words
    uint16_t word_count = 0;
    const wl_codec_t *cfg = wl_codec_cfg(list);

    if (cfg == NULL)
    {
        goto exit;
    }

    // Check that in respect the word list rules
    // Check that out_size and in_len respect the word list rules (for the maximum word count)
    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL ||
        in_len >
            ((size_t)cfg->sp_config.max_word_count * cfg->sp_config.max_word_len + cfg->sp_config.max_word_count) ||
        out_size < ((size_t)cfg->sp_config.max_word_count * 2))
    {
        goto exit;
    }

    if (wl_codec_parse_words(cfg, in, in_len, indices, &word_count) != 0)
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

int wl_codec_compress_passphrase(word_list_e list,
                                 const char *in,
                                 size_t in_len,
                                 uint8_t *out,
                                 size_t out_size,
                                 size_t *out_len)
{
    int ret = -EINVAL;
    uint16_t indices[WL_MAX_WORD_COUNT];
    uint16_t word_count = 0;
    const wl_codec_t *cfg = wl_codec_cfg(list);

    if (cfg == NULL)
    {
        goto exit;
    }

    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL)
    {
        goto exit;
    }

    // Find the first separator: phrase before, passphrase after.
    const char *sep = memchr(in, cfg->sp_config.passphrase_separator, in_len);
    if (sep == NULL)
    {
        goto exit; // no separator
    }

    const char *seed_start = in;
    const char *seed_end = sep; // exclusive
    const char *pp_start = sep + 1; // passphrase start
    const char *pp_end = in + in_len; // exclusive

    // Trim surrounding whitespace (formatting) on both parts.
    // Remove the space and tabulation before and after the phrase.
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

    if (wl_codec_parse_words(cfg, seed_start, seed_len, indices, &word_count) != 0 || word_count == 0)
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

int wl_codec_decompress(word_list_e list, const uint8_t *in, size_t in_len, char *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;
    const wl_codec_t *cfg = wl_codec_cfg(list);

    if (cfg == NULL)
    {
        goto exit;
    }

    // Check that in respect the word list rules
    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL)
    {
        goto exit;
    }

    // Input is a sequence of 2-byte little-endian word indices
    if ((in_len % 2) != 0 || in_len > ((size_t)cfg->sp_config.max_word_count * 2))
    {
        goto exit;
    }

    uint16_t word_count = (uint16_t)(in_len / 2);

    // Iterate on each compressed word index and uncompress them into a phrase
    size_t pos = 0;
    for (uint16_t i = 0; i < word_count; i++)
    {
        uint16_t index = (uint16_t)(in[i * 2] | (in[i * 2 + 1] << 8));
        if (index >= cfg->wl_config.list_word_count)
        {
            goto exit;
        }

        const char *word = cfg->wl_config.list_words[index];
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

int wl_codec_decompress_passphrase(word_list_e list,
                                   const uint8_t *in,
                                   size_t in_len,
                                   char *out,
                                   size_t out_size,
                                   size_t *out_len)
{
    int ret = -EINVAL;
    const wl_codec_t *cfg = wl_codec_cfg(list);

    if (cfg == NULL)
    {
        goto exit;
    }

    if (in == NULL || in_len < 1 || out == NULL || out_len == NULL)
    {
        goto exit;
    }

    // Format: [word_count: 1 byte][word_count x 2 bytes][passphrase bytes]
    uint16_t word_count = in[0];
    if (word_count == 0 || word_count > cfg->sp_config.max_word_count)
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

    // Reconstruct the phrase words (space-separated).
    size_t pos = 0;
    for (uint16_t i = 0; i < word_count; i++)
    {
        uint16_t index = (uint16_t)(in[1 + i * 2] | (in[1 + i * 2 + 1] << 8));
        if (index >= cfg->wl_config.list_word_count)
        {
            goto exit;
        }

        const char *word = cfg->wl_config.list_words[index];
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
        out[pos++] = cfg->sp_config.passphrase_separator;
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
static const wl_codec_t *wl_codec_cfg(word_list_e list)
{
    if (list >= WORD_LIST_COUNT)
    {
        return NULL;
    }

    return &wl_codec_list[list];
}

static int wl_codec_parse_words(const wl_codec_t *cfg,
                                const char *in,
                                size_t in_len,
                                uint16_t *indices,
                                uint16_t *word_count)
{
    uint16_t count = 0;
    size_t i = 0;

    while (i < in_len)
    {
        char word[WL_MAX_WORD_LEN + 1];
        uint8_t wlen = 0;

        while (i < in_len && in[i] != ' ')
        {
            if (wlen >= cfg->sp_config.max_word_len)
            {
                return -EINVAL; // word longer than the maximum
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
        for (uint16_t j = 0; j < cfg->wl_config.list_word_count; j++)
        {
            if (strcmp(word, cfg->wl_config.list_words[j]) == 0)
            {
                if (count >= cfg->sp_config.max_word_count)
                {
                    return -EINVAL; // more words than the maximum
                }
                indices[count++] = j;
                found = 1;
                break;
            }
        }

        if (!found)
        {
            return -EINVAL; // word not in the word list
        }

        if (i < in_len)
        {
            i++; // skip the single space separator after a word
        }
    }

    *word_count = count;
    return 0;
}
