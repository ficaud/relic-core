/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file base32.c
 *
 * @brief Base32 codec (RFC 4648) for compressing share payloads.
 *
 * @author Julien F.
 * @date 2026-08-20
 *
 * @details Encodes bytes as unpadded base32 (alphabet A-Z 2-7, uppercase).
 *          Decoding is case-insensitive and rejects any character outside
 *          the alphabet as well as malformed input lengths.
 */

#include "base32.h"

#include <errno.h>

// ===========================================================================
// Structure and variables definition
// ===========================================================================
static const char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// ===========================================================================
// Static function declarations
// ===========================================================================
/**
 * @brief Convert a base32 character to its 5-bit value.
 *
 * @param[in] c  Input character (case-insensitive).
 *
 * @return 0..31 on success, -1 if the character is outside the alphabet.
 */
static int base32_char_to_value(char c);

// ===========================================================================
// Static function definition
// ===========================================================================
static int base32_char_to_value(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z')
    {
        return c - 'a';
    }
    if (c >= '2' && c <= '7')
    {
        return c - '2' + 26;
    }
    return -1;
}

// ===========================================================================
// Public function definition
// ===========================================================================
size_t base32_encoded_len(size_t in_len)
{
    size_t chars = (in_len * 8 + 4) / 5; /* ceil(in_len * 8 / 5) */
    return chars + 1; /* + terminating NUL */
}

int base32_encode(const uint8_t *in, size_t in_len, char *out, size_t out_size)
{
    int ret = -1;
    size_t required;

    if (out == NULL)
    {
        ret = -EINVAL;
        goto exit;
    }

    if (in == NULL && in_len > 0)
    {
        ret = -EINVAL;
        goto exit;
    }

    required = base32_encoded_len(in_len);
    if (out_size < required)
    {
        ret = -ENOSPC;
        goto exit;
    }

    size_t out_idx = 0;
    size_t i = 0;

    /* Process input in 5-byte groups; each yields 8 characters. */
    while (i < in_len)
    {
        size_t remaining = in_len - i;

        /* Process the group. */
        size_t group_len = 0;

        if (remaining >= 5)
        {
            group_len = 5;
        }
        else
        {
            group_len = remaining;
        }

        /* Pack the group into a 40-bit chunk (top-aligned)? */
        /* This create a chunk that hold at most 5 bytes from the input buffer */
        uint64_t chunk = 0;
        for (size_t j = 0; j < group_len; j++)
        {
            chunk |= (uint64_t)in[i + j] << (32 - 8 * j);
        }

        /* Number of output characters for a group of 1..5 bytes. */
        static const uint8_t group_chars[] = {0, 2, 4, 5, 7, 8};
        for (size_t j = 0; j < group_chars[group_len]; j++)
        {
            /*
             * Select the right most 5 bits of the chunk and shift them to the
             * rightmost position in the output buffer. The output buffer is
             * big-endian, so the bits are shifted to the left.
             *
             * 39 - 35
             ° 34 - 30
             ° 29 - 25
             ° 24 - 20
             ° 19 - 15
             ° 14 - 10
             °  9 -  5
             °  4 -  0
             */
            out[out_idx++] = BASE32_ALPHABET[(chunk >> (35 - 5 * j)) & 0x1F];
        }

        i += group_len;
    }

    out[out_idx] = '\0';
    ret = 0;

exit:
    return ret;
}

int base32_decode(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    int ret = -1;

    if (out == NULL || out_len == NULL)
    {
        ret = -EINVAL;
        goto exit;
    }

    if (in == NULL && in_len > 0)
    {
        ret = -EINVAL;
        goto exit;
    }

    /* Valid unpadded base32 lengths (mod 8): 0, 2, 4, 5, 7. */
    switch (in_len % 8)
    {
        case 0:
        case 2:
        case 4:
        case 5:
        case 7:
            break;
        default:
            ret = -EINVAL;
            goto exit;
    }

    size_t decoded_len = (in_len * 5) / 8;
    if (decoded_len > out_size)
    {
        ret = -ENOSPC;
        goto exit;
    }

    uint32_t chunk = 0;
    int bits = 0;
    size_t out_idx = 0;

    for (size_t i = 0; i < in_len; i++)
    {
        // Decode the current character into a value in the range 0..31.
        int val = base32_char_to_value(in[i]);
        if (val < 0)
        {
            ret = -EINVAL;
            goto exit;
        }

        // reconstruct the chunk by taking the current character decoded, and
        // shifting its current overall value of 5 bits to the left.
        chunk = (chunk << 5) | (uint32_t)val;
        bits += 5;

        // Once we reach 8bits or more, we can write a byte to the output.
        if (bits >= 8)
        {
            bits -= 8;
            out[out_idx++] = (uint8_t)(chunk >> bits);
        }
    }

    /* The unused trailing bits of the final character must be zero. */
    if (bits > 0)
    {
        // create a mask with the rightmost bits set to 1.
        uint32_t mask = (1u << bits) - 1;
        if ((chunk & mask) != 0)
        {
            ret = -EINVAL;
            goto exit;
        }
    }

    *out_len = out_idx;
    ret = 0;

exit:
    return ret;
}
