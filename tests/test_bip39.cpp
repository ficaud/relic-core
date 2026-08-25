// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the BIP-39 seed phrase compression (bip39.c / bip39.h).

#include "bip39.h"
#include "bip39_words.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

// ===========================================================================
// Test helpers
// ===========================================================================

namespace
{

/**
 * @brief Build the little-endian 2-byte representation of a word index.
 */
std::vector<uint8_t> indexBytes(uint16_t index)
{
    return {(uint8_t)(index & 0xFF), (uint8_t)(index >> 8)};
}

/**
 * @brief Return the byte-compressed form of a seed phrase.
 */
std::vector<uint8_t> compress(const std::string &phrase)
{
    std::vector<uint8_t> out(216);
    size_t out_len = 0;
    int ret = bip39_compress(phrase.c_str(), phrase.size(), out.data(), out.size(), &out_len);
    if (ret != 0)
    {
        return {};
    }
    out.resize(out_len);
    return out;
}

/**
 * @brief Lookup the BIP-39 index of a word (linear scan, test-only helper).
 */
int wordIndex(const std::string &word)
{
    for (int i = 0; i < BIP39_WORD_COUNT; i++)
    {
        if (std::strcmp(word.c_str(), bip39_words[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class Bip39Test : public ::testing::Test
{
  protected:
};

// ===========================================================================
// Compress
// ===========================================================================

TEST_F(Bip39Test, Compress_SingleWord)
{
    struct
    {
        const char *word;
        uint16_t index;
    } cases[] = {
        {"abandon", 0},
        {"ability", 1},
        {"abstract", 7},
        {"zoo", 2047},
    };

    for (const auto &c : cases)
    {
        uint8_t out[216];
        size_t out_len = 0;
        size_t in_len = std::strlen(c.word);
        ASSERT_EQ(bip39_compress(c.word, in_len, out, sizeof(out), &out_len), 0) << c.word;

        std::vector<uint8_t> expected = indexBytes(c.index);
        EXPECT_EQ(out[0], expected[0]) << c.word;
        EXPECT_EQ(out[1], expected[1]) << c.word;
        EXPECT_EQ(out_len, 2u) << c.word;
    }
}

TEST_F(Bip39Test, Compress_MultiWord)
{
    const char *phrase = "abandon zoo zoo";
    std::vector<uint8_t> expected;
    auto idx0 = indexBytes(0);
    auto idxZoo = indexBytes(2047);
    expected.insert(expected.end(), idx0.begin(), idx0.end());
    expected.insert(expected.end(), idxZoo.begin(), idxZoo.end());
    expected.insert(expected.end(), idxZoo.begin(), idxZoo.end());

    uint8_t out[216];
    size_t in_len = std::strlen(phrase);
    size_t out_len = 0;
    ASSERT_EQ(bip39_compress(phrase, in_len, out, sizeof(out), &out_len), 0);

    for (size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_EQ(out[i], expected[i]) << "byte " << i;
    }
    EXPECT_EQ(out_len, expected.size());
}

TEST_F(Bip39Test, Compress_MatchesWordlistIndex)
{
    const char *phrase = "abandon ability abstract zoo";
    uint8_t out[216];
    size_t in_len = std::strlen(phrase);
    size_t out_len = 0;
    ASSERT_EQ(bip39_compress(phrase, in_len, out, sizeof(out), &out_len), 0);

    const char *words[] = {"abandon", "ability", "abstract", "zoo"};
    for (size_t w = 0; w < 4; w++)
    {
        uint16_t index = (uint16_t)wordIndex(words[w]);
        EXPECT_EQ(out[w * 2], (uint8_t)(index & 0xFF)) << words[w];
        EXPECT_EQ(out[w * 2 + 1], (uint8_t)(index >> 8)) << words[w];
    }
}

TEST_F(Bip39Test, Compress_EmptyWordsAreSkipped)
{
    const char *phrase = "abandon  zoo";
    uint8_t out[216];
    size_t in_len = std::strlen(phrase);
    size_t out_len = 0;
    ASSERT_EQ(bip39_compress(phrase, in_len, out, sizeof(out), &out_len), 0);

    std::vector<uint8_t> expected;
    auto idx0 = indexBytes(0);
    auto idxZoo = indexBytes(2047);
    expected.insert(expected.end(), idx0.begin(), idx0.end());
    expected.insert(expected.end(), idxZoo.begin(), idxZoo.end());

    for (size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_EQ(out[i], expected[i]) << "byte " << i;
    }
}

// ===========================================================================
// Golden vectors (deterministic)
// ===========================================================================

namespace
{

// 24-word seed phrase shared with golden_shares.h:
//   23 x "abandon" (index 0) followed by "art" (index 102).
const char *const GOLDEN_SEED_PHRASE =
    "abandon abandon abandon abandon abandon abandon abandon abandon abandon"
    " abandon abandon abandon abandon abandon abandon abandon abandon abandon"
    " abandon abandon abandon abandon abandon art";

// Compressed form: two little-endian bytes per word index.
// 23 x index 0 (0x0000), then index 102 (0x0066).
const uint8_t GOLDEN_COMPRESSED[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x00,
};

} // namespace

TEST_F(Bip39Test, Golden_Compress)
{
    uint8_t out[48];
    size_t out_len = 0;
    ASSERT_EQ(bip39_compress(GOLDEN_SEED_PHRASE, std::strlen(GOLDEN_SEED_PHRASE), out, sizeof(out), &out_len), 0);

    ASSERT_EQ(out_len, sizeof(GOLDEN_COMPRESSED));
    EXPECT_EQ(std::memcmp(out, GOLDEN_COMPRESSED, out_len), 0);
}

TEST_F(Bip39Test, Golden_Decompress)
{
    char out[256];
    size_t out_len = 0;
    ASSERT_EQ(bip39_decompress(GOLDEN_COMPRESSED, sizeof(GOLDEN_COMPRESSED), out, sizeof(out), &out_len), 0);

    EXPECT_STREQ(out, GOLDEN_SEED_PHRASE);
    EXPECT_EQ(out_len, std::strlen(GOLDEN_SEED_PHRASE));
}

// ===========================================================================
// Decompress
// ===========================================================================

TEST_F(Bip39Test, Decompress_SingleWord)
{
    struct
    {
        uint16_t index;
        const char *word;
    } cases[] = {
        {0, "abandon"},
        {1, "ability"},
        {7, "abstract"},
        {2047, "zoo"},
    };

    for (const auto &c : cases)
    {
        std::vector<uint8_t> in = indexBytes(c.index);
        char out[256];
        size_t out_len = 0;
        ASSERT_EQ(bip39_decompress(in.data(), in.size(), out, sizeof(out), &out_len), 0) << c.index;

        EXPECT_STREQ(out, c.word) << c.index;
        EXPECT_EQ(out_len, std::strlen(c.word)) << c.index;
    }
}

TEST_F(Bip39Test, Decompress_MultiWord)
{
    std::vector<uint8_t> in;
    auto idx0 = indexBytes(0);
    auto idxZoo = indexBytes(2047);
    in.insert(in.end(), idx0.begin(), idx0.end());
    in.insert(in.end(), idxZoo.begin(), idxZoo.end());
    in.insert(in.end(), idxZoo.begin(), idxZoo.end());

    char out[256];
    size_t out_len = 0;
    ASSERT_EQ(bip39_decompress(in.data(), in.size(), out, sizeof(out), &out_len), 0);

    EXPECT_STREQ(out, "abandon zoo zoo");
    EXPECT_EQ(out_len, std::strlen("abandon zoo zoo"));
}

// ===========================================================================
// Round-trip
// ===========================================================================

TEST_F(Bip39Test, RoundTrip)
{
    const char *phrases[] = {
        "abandon",
        "zoo",
        "abandon zoo zoo",
        "ability abstract",
        "about able above absorb",
    };

    for (const char *phrase : phrases)
    {
        uint8_t compressed[216];
        size_t in_len = std::strlen(phrase);
        size_t compressed_len = 0;
        ASSERT_EQ(bip39_compress(phrase, in_len, compressed, sizeof(compressed), &compressed_len), 0) << phrase;

        // A phrase of N words compresses to 2N bytes.
        size_t word_count = 1;
        for (size_t i = 0; i < in_len; i++)
        {
            if (phrase[i] == ' ')
            {
                word_count++;
            }
        }
        EXPECT_EQ(compressed_len, word_count * 2) << phrase;

        char out[256];
        size_t out_len = 0;
        ASSERT_EQ(bip39_decompress(compressed, compressed_len, out, sizeof(out), &out_len), 0) << phrase;

        EXPECT_STREQ(out, phrase) << phrase;
        EXPECT_EQ(out_len, in_len) << phrase;
    }
}

TEST_F(Bip39Test, RoundTrip_MaxWords)
{
    // 24 words — the maximum allowed.
    std::string phrase;
    for (int i = 0; i < SEEDPHRASE_MAX_WORDS_COUNT; i++)
    {
        if (i > 0)
        {
            phrase += ' ';
        }
        phrase += "abandon";
    }

    uint8_t compressed[216];
    size_t compressed_len = 0;
    ASSERT_EQ(bip39_compress(phrase.c_str(), phrase.size(), compressed, sizeof(compressed), &compressed_len), 0);

    char out[256];
    size_t out_len = 0;
    ASSERT_EQ(bip39_decompress(compressed, compressed_len, out, sizeof(out), &out_len), 0);

    EXPECT_STREQ(out, phrase.c_str());
    EXPECT_EQ(out_len, phrase.size());
}

// ===========================================================================
// Compress error handling
// ===========================================================================

TEST_F(Bip39Test, Compress_NullInput)
{
    uint8_t out[216];
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress(nullptr, 5, out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Compress_EmptyInput)
{
    uint8_t out[216];
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress("", 0, out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Compress_NullOutput)
{
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress("abandon", 7, nullptr, 216, &out_len), -EINVAL);
}

TEST_F(Bip39Test, Compress_NullOutLen)
{
    uint8_t out[216];
    EXPECT_EQ(bip39_compress("abandon", 7, out, sizeof(out), nullptr), -EINVAL);
}

TEST_F(Bip39Test, Compress_UnknownWord)
{
    uint8_t out[216];
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress("notaword", 8, out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Compress_WordTooLong)
{
    uint8_t out[216];
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress("toolongword", 11, out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Compress_TooManyWords)
{
    // 25 words exceeds SEEDPHRASE_MAX_WORDS_COUNT (24).
    std::string phrase;
    for (int i = 0; i < SEEDPHRASE_MAX_WORDS_COUNT + 1; i++)
    {
        if (i > 0)
        {
            phrase += ' ';
        }
        phrase += "abandon";
    }

    uint8_t out[216];
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress(phrase.c_str(), phrase.size(), out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Compress_OutputTooSmall)
{
    uint8_t out[8];
    size_t out_len = 0;
    EXPECT_EQ(bip39_compress("abandon", 7, out, sizeof(out), &out_len), -EINVAL);
}

// ===========================================================================
// Decompress error handling
// ===========================================================================

TEST_F(Bip39Test, Decompress_NullPointers)
{
    char out[256];
    size_t out_len = 0;
    std::vector<uint8_t> in = indexBytes(0);

    EXPECT_EQ(bip39_decompress(nullptr, in.size(), out, sizeof(out), &out_len), -EINVAL);
    EXPECT_EQ(bip39_decompress(in.data(), 0, out, sizeof(out), &out_len), -EINVAL);
    EXPECT_EQ(bip39_decompress(in.data(), in.size(), nullptr, sizeof(out), &out_len), -EINVAL);
    EXPECT_EQ(bip39_decompress(in.data(), in.size(), out, sizeof(out), nullptr), -EINVAL);
}

TEST_F(Bip39Test, Decompress_OddLength)
{
    uint8_t in[3] = {0x00, 0x00, 0x00};
    char out[256];
    size_t out_len = 0;
    EXPECT_EQ(bip39_decompress(in, sizeof(in), out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Decompress_TooManyWords)
{
    // 25 indices (50 bytes) exceeds SEEDPHRASE_MAX_WORDS_COUNT (24).
    uint8_t in[50] = {0};
    char out[256];
    size_t out_len = 0;
    EXPECT_EQ(bip39_decompress(in, sizeof(in), out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Decompress_IndexOutOfRange)
{
    std::vector<uint8_t> in = indexBytes(2048); // 2048 >= BIP39_WORD_COUNT
    char out[256];
    size_t out_len = 0;
    EXPECT_EQ(bip39_decompress(in.data(), in.size(), out, sizeof(out), &out_len), -EINVAL);

    uint8_t inMax[2] = {0xFF, 0xFF}; // 65535
    EXPECT_EQ(bip39_decompress(inMax, sizeof(inMax), out, sizeof(out), &out_len), -EINVAL);
}

TEST_F(Bip39Test, Decompress_OutputTooSmall)
{
    std::vector<uint8_t> in = indexBytes(0); // "abandon" (7 chars + NUL = 8)
    char out[4];
    size_t out_len = 0;
    EXPECT_EQ(bip39_decompress(in.data(), in.size(), out, sizeof(out), &out_len), -ENOSPC);
}
