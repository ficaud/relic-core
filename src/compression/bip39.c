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
/// ===========================================================================
// Public function definition
// ===========================================================================
int bip39_compress(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    int ret = -EINVAL;
    uint16_t compressed_sp[SEEDPHRASE_MAX_WORDS_COUNT]; // output buffer that contains at most 24 words (in
                                                        // bytes)

    // Check that in respect the BIP-39 rules
    // Check that out_size and in_len respect the BIP-39 rules (for a 24 words seed phrase maximum)
    if (in == NULL || in_len == 0 || out == NULL || out_len == NULL ||
        in_len > (SEEDPHRASE_MAX_WORDS_COUNT * SEEDPHRASE_MAX_WORD_LEN + SEEDPHRASE_MAX_WORDS_COUNT) ||
        out_size < (SEEDPHRASE_MAX_WORDS_COUNT * 2))
    {
        goto exit;
    }

    // Iterate on all words (check that they are conform and compress them up)
    uint16_t compressed_sp_index = 0;
    uint8_t current_word_index = 0;
    for (uint16_t i = 0; i < in_len; i++)
    {
        // parse the word
        char word[SEEDPHRASE_MAX_WORD_LEN + 1];
        current_word_index = 0;
        while (i < in_len && in[i] != ' ' && in[i] != '\0')
        {
            if (current_word_index >= SEEDPHRASE_MAX_WORD_LEN)
            {
                goto exit; // word longer than the BIP-39 maximum
            }

            word[current_word_index] = in[i];
            current_word_index++;
            i++;
        }
        word[current_word_index] = '\0';

        if (current_word_index == 0)
        {
            continue; // skip empty word (double space, leading/trailing space)
        }

        // Check that the word index is in the BIP-39 words list
        int found = 0;
        for (uint16_t j = 0; j < BIP39_WORD_COUNT; j++)
        {
            if (strcmp(word, bip39_words[j]) == 0)
            {
                if (compressed_sp_index >= SEEDPHRASE_MAX_WORDS_COUNT)
                {
                    goto exit; // more words than the BIP-39 maximum
                }

                // Add the word to the compressed_sp list
                compressed_sp[compressed_sp_index] = j;
                compressed_sp_index++;
                found = 1;
                break;
            }
        }

        if (!found)
        {
            goto exit; // word not in the BIP-39 wordlist
        }
    }

    // turn compressed_sp uint16_t list into a uint8_t list
    uint16_t out_index = 0;
    for (uint16_t i = 0; i < compressed_sp_index; i++)
    {
        out[out_index] = (uint8_t)(compressed_sp[i] & 0xFF);
        out_index++;
        out[out_index] = (uint8_t)(compressed_sp[i] >> 8);
        out_index++;
    }

    *out_len = out_index;
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
// ===========================================================================
// Static function definition
// ===========================================================================
