// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the generic word-list codec (wl_codec.c / wl_codec.h).
//
// The same code path is exercised for both the BIP-39 and the SLIP-39 word
// lists; the per-list differences (words, indices, word counts) are described
// by a small parameter block so the shared logic is tested only once.

#include "bip39_words.h"
#include "slip39_words.h"
#include "wl_codec.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

// ===========================================================================
// Test parameters
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
 * @brief Per-word-list test parameters.
 */
struct wl_params
{
    const char *name;
    word_list_e list;
    std::vector<std::pair<const char *, uint16_t>> single; // {word, index}
    const char *last_word; // last word of the list
    uint16_t last_index; // its index
    const char *golden_last_word; // final word of the golden phrase
    uint16_t golden_last_index; // its index
    std::vector<const char *> match_words; // words for the index-matching test
    int max_words; // max number of words per phrase
    uint16_t out_of_range_index; // first index >= list size
    std::vector<const char *> roundtrip; // round-trip phrases
};

const wl_params kBip39 = {
    "bip39",
    WORD_LIST_BIP39,
    {{"abandon", 0}, {"ability", 1}, {"abstract", 7}, {"zoo", 2047}},
    "zoo",
    2047,
    "art",
    102,
    {"abandon", "ability", "abstract", "zoo"},
    24,
    2048,
    {"abandon", "zoo", "abandon zoo zoo", "ability abstract", "about able above absorb"},
};

const wl_params kSlip39 = {
    "slip39",
    WORD_LIST_SLIP39,
    {{"academic", 0}, {"acid", 1}, {"acrobat", 4}, {"zero", 1023}},
    "zero",
    1023,
    "zero",
    1023,
    {"academic", "acid", "acne", "zero"},
    33,
    1024,
    {"academic", "zero", "academic zero zero", "acid acrobat", "acne acid acrobat acquire"},
};

const wl_params *const kParams[] = {&kBip39, &kSlip39};

/**
 * @brief Lookup the index of a word (linear scan, test-only helper).
 */
int wordIndex(word_list_e list, const std::string &word)
{
    if (list == WORD_LIST_BIP39)
    {
        for (int i = 0; i < BIP39_WORD_COUNT; i++)
        {
            if (std::strcmp(word.c_str(), bip39_words[i]) == 0)
            {
                return i;
            }
        }
    }
    else
    {
        for (int i = 0; i < SLIP39_WORD_COUNT; i++)
        {
            if (std::strcmp(word.c_str(), slip39_words[i]) == 0)
            {
                return i;
            }
        }
    }
    return -1;
}

/**
 * @brief Join @p n copies of @p word with spaces.
 */
std::string repeatWord(const char *word, int n)
{
    std::string out;
    for (int i = 0; i < n; i++)
    {
        if (i > 0)
        {
            out += ' ';
        }
        out += word;
    }
    return out;
}

/**
 * @brief "first last last" phrase used by several tests.
 */
std::string multiPhrase(const wl_params &p)
{
    return std::string(p.single[0].first) + " " + p.last_word + " " + p.last_word;
}

/**
 * @brief The maximum-length golden phrase and its compressed form.
 */
std::string goldenPhrase(const wl_params &p)
{
    return repeatWord(p.single[0].first, p.max_words - 1) + " " + p.golden_last_word;
}

std::vector<uint8_t> goldenBytes(const wl_params &p)
{
    std::vector<uint8_t> out;
    for (int i = 0; i < p.max_words - 1; i++)
    {
        out.push_back(0x00);
        out.push_back(0x00);
    }
    auto last = indexBytes(p.golden_last_index);
    out.insert(out.end(), last.begin(), last.end());
    return out;
}

/**
 * @brief Byte-compressed form of a phrase (empty on error).
 */
std::vector<uint8_t> compress(word_list_e list, const std::string &phrase)
{
    std::vector<uint8_t> out(512);
    size_t out_len = 0;
    int ret = wl_codec_compress(list, phrase.c_str(), phrase.size(), out.data(), out.size(), &out_len);
    if (ret != 0)
    {
        return {};
    }
    out.resize(out_len);
    return out;
}

/**
 * @brief Decompressed phrase text (empty on error).
 */
std::string decompress(word_list_e list, const std::vector<uint8_t> &in)
{
    std::string out(512, '\0');
    size_t out_len = 0;
    int ret = wl_codec_decompress(list, in.data(), in.size(), &out[0], out.size(), &out_len);
    if (ret != 0)
    {
        return {};
    }
    out.resize(out_len);
    return out;
}

/**
 * @brief Byte-compressed form of a phrase + passphrase (empty on error).
 */
std::vector<uint8_t> compressPassphrase(word_list_e list, const std::string &phrase)
{
    std::vector<uint8_t> out(512);
    size_t out_len = 0;
    int ret = wl_codec_compress_passphrase(list, phrase.c_str(), phrase.size(), out.data(), out.size(), &out_len);
    if (ret != 0)
    {
        return {};
    }
    out.resize(out_len);
    return out;
}

/**
 * @brief Decompressed phrase + passphrase text (empty on error).
 */
std::string decompressPassphrase(word_list_e list, const std::vector<uint8_t> &in)
{
    std::string out(512, '\0');
    size_t out_len = 0;
    int ret = wl_codec_decompress_passphrase(list, in.data(), in.size(), &out[0], out.size(), &out_len);
    if (ret != 0)
    {
        return {};
    }
    out.resize(out_len);
    return out;
}

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class WlCodecTest : public ::testing::Test
{
  protected:
};

// ===========================================================================
// Compress
// ===========================================================================

TEST_F(WlCodecTest, Compress_SingleWord)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        for (const auto &c : p->single)
        {
            uint8_t out[512];
            size_t out_len = 0;
            size_t in_len = std::strlen(c.first);
            ASSERT_EQ(wl_codec_compress(p->list, c.first, in_len, out, sizeof(out), &out_len), 0) << c.first;

            std::vector<uint8_t> expected = indexBytes(c.second);
            EXPECT_EQ(out[0], expected[0]) << c.first;
            EXPECT_EQ(out[1], expected[1]) << c.first;
            EXPECT_EQ(out_len, 2u) << c.first;
        }
    }
}

TEST_F(WlCodecTest, Compress_MultiWord)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = multiPhrase(*p);
        std::vector<uint8_t> expected;
        auto idx0 = indexBytes(0);
        auto idxLast = indexBytes(p->last_index);
        expected.insert(expected.end(), idx0.begin(), idx0.end());
        expected.insert(expected.end(), idxLast.begin(), idxLast.end());
        expected.insert(expected.end(), idxLast.begin(), idxLast.end());

        uint8_t out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_compress(p->list, phrase.c_str(), phrase.size(), out, sizeof(out), &out_len), 0);

        ASSERT_EQ(out_len, expected.size());
        for (size_t i = 0; i < expected.size(); i++)
        {
            EXPECT_EQ(out[i], expected[i]) << "byte " << i;
        }
    }
}

TEST_F(WlCodecTest, Compress_MatchesWordlistIndex)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase;
        for (size_t w = 0; w < p->match_words.size(); w++)
        {
            if (w > 0)
            {
                phrase += ' ';
            }
            phrase += p->match_words[w];
        }

        uint8_t out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_compress(p->list, phrase.c_str(), phrase.size(), out, sizeof(out), &out_len), 0);

        for (size_t w = 0; w < p->match_words.size(); w++)
        {
            uint16_t index = (uint16_t)wordIndex(p->list, p->match_words[w]);
            EXPECT_EQ(out[w * 2], (uint8_t)(index & 0xFF)) << p->match_words[w];
            EXPECT_EQ(out[w * 2 + 1], (uint8_t)(index >> 8)) << p->match_words[w];
        }
    }
}

TEST_F(WlCodecTest, Compress_EmptyWordsAreSkipped)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = std::string(p->single[0].first) + "  " + p->last_word;

        uint8_t out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_compress(p->list, phrase.c_str(), phrase.size(), out, sizeof(out), &out_len), 0);

        std::vector<uint8_t> expected;
        auto idx0 = indexBytes(0);
        auto idxLast = indexBytes(p->last_index);
        expected.insert(expected.end(), idx0.begin(), idx0.end());
        expected.insert(expected.end(), idxLast.begin(), idxLast.end());

        ASSERT_EQ(out_len, expected.size());
        for (size_t i = 0; i < expected.size(); i++)
        {
            EXPECT_EQ(out[i], expected[i]) << "byte " << i;
        }
    }
}

// ===========================================================================
// Golden vectors (deterministic)
// ===========================================================================

TEST_F(WlCodecTest, Golden_Compress)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = goldenPhrase(*p);
        std::vector<uint8_t> expected = goldenBytes(*p);

        uint8_t out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_compress(p->list, phrase.c_str(), phrase.size(), out, sizeof(out), &out_len), 0);

        ASSERT_EQ(out_len, expected.size());
        EXPECT_EQ(std::memcmp(out, expected.data(), out_len), 0);
    }
}

TEST_F(WlCodecTest, Golden_Decompress)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = goldenPhrase(*p);
        std::vector<uint8_t> in = goldenBytes(*p);

        char out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), &out_len), 0);

        EXPECT_STREQ(out, phrase.c_str());
        EXPECT_EQ(out_len, phrase.size());
    }
}

// ===========================================================================
// Decompress
// ===========================================================================

TEST_F(WlCodecTest, Decompress_SingleWord)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        for (const auto &c : p->single)
        {
            std::vector<uint8_t> in = indexBytes(c.second);
            char out[512];
            size_t out_len = 0;
            ASSERT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), &out_len), 0) << c.second;

            EXPECT_STREQ(out, c.first) << c.second;
            EXPECT_EQ(out_len, std::strlen(c.first)) << c.second;
        }
    }
}

TEST_F(WlCodecTest, Decompress_MultiWord)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = multiPhrase(*p);

        std::vector<uint8_t> in;
        auto idx0 = indexBytes(0);
        auto idxLast = indexBytes(p->last_index);
        in.insert(in.end(), idx0.begin(), idx0.end());
        in.insert(in.end(), idxLast.begin(), idxLast.end());
        in.insert(in.end(), idxLast.begin(), idxLast.end());

        char out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), &out_len), 0);

        EXPECT_STREQ(out, phrase.c_str());
        EXPECT_EQ(out_len, phrase.size());
    }
}

// ===========================================================================
// Round-trip
// ===========================================================================

TEST_F(WlCodecTest, RoundTrip)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        for (const char *phrase : p->roundtrip)
        {
            uint8_t compressed[512];
            size_t in_len = std::strlen(phrase);
            size_t compressed_len = 0;
            ASSERT_EQ(wl_codec_compress(p->list, phrase, in_len, compressed, sizeof(compressed), &compressed_len), 0)
                << phrase;

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

            char out[512];
            size_t out_len = 0;
            ASSERT_EQ(wl_codec_decompress(p->list, compressed, compressed_len, out, sizeof(out), &out_len), 0)
                << phrase;

            EXPECT_STREQ(out, phrase) << phrase;
            EXPECT_EQ(out_len, in_len) << phrase;
        }
    }
}

TEST_F(WlCodecTest, RoundTrip_MaxWords)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = repeatWord(p->single[0].first, p->max_words);

        uint8_t compressed[512];
        size_t compressed_len = 0;
        ASSERT_EQ(
            wl_codec_compress(p->list, phrase.c_str(), phrase.size(), compressed, sizeof(compressed), &compressed_len),
            0);

        char out[512];
        size_t out_len = 0;
        ASSERT_EQ(wl_codec_decompress(p->list, compressed, compressed_len, out, sizeof(out), &out_len), 0);

        EXPECT_STREQ(out, phrase.c_str());
        EXPECT_EQ(out_len, phrase.size());
    }
}

// ===========================================================================
// Compress error handling
// ===========================================================================

TEST_F(WlCodecTest, Compress_NullInput)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress(p->list, nullptr, 5, out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_EmptyInput)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress(p->list, "", 0, out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_NullOutput)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        size_t out_len = 0;
        EXPECT_EQ(
            wl_codec_compress(p->list, p->single[0].first, std::strlen(p->single[0].first), nullptr, 512, &out_len),
            -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_NullOutLen)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t out[512];
        EXPECT_EQ(
            wl_codec_compress(p->list, p->single[0].first, std::strlen(p->single[0].first), out, sizeof(out), nullptr),
            -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_UnknownWord)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress(p->list, "notaword", 8, out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_WordTooLong)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress(p->list, "toolongword", 11, out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_TooManyWords)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = repeatWord(p->single[0].first, p->max_words + 1);

        uint8_t out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress(p->list, phrase.c_str(), phrase.size(), out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Compress_OutputTooSmall)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t out[8];
        size_t out_len = 0;
        EXPECT_EQ(
            wl_codec_compress(p->list, p->single[0].first, std::strlen(p->single[0].first), out, sizeof(out), &out_len),
            -EINVAL);
    }
}

// ===========================================================================
// Decompress error handling
// ===========================================================================

TEST_F(WlCodecTest, Decompress_NullPointers)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        char out[512];
        size_t out_len = 0;
        std::vector<uint8_t> in = indexBytes(0);

        EXPECT_EQ(wl_codec_decompress(p->list, nullptr, in.size(), out, sizeof(out), &out_len), -EINVAL);
        EXPECT_EQ(wl_codec_decompress(p->list, in.data(), 0, out, sizeof(out), &out_len), -EINVAL);
        EXPECT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), nullptr, sizeof(out), &out_len), -EINVAL);
        EXPECT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), nullptr), -EINVAL);
    }
}

TEST_F(WlCodecTest, Decompress_OddLength)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        uint8_t in[3] = {0x00, 0x00, 0x00};
        char out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_decompress(p->list, in, sizeof(in), out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Decompress_TooManyWords)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::vector<uint8_t> in((size_t)(p->max_words + 1) * 2, 0);
        char out[512];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Decompress_IndexOutOfRange)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        char out[512];
        size_t out_len = 0;

        std::vector<uint8_t> in = indexBytes(p->out_of_range_index);
        EXPECT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), &out_len), -EINVAL);

        uint8_t inMax[2] = {0xFF, 0xFF};
        EXPECT_EQ(wl_codec_decompress(p->list, inMax, sizeof(inMax), out, sizeof(out), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Decompress_OutputTooSmall)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::vector<uint8_t> in = indexBytes(0);
        char out[4];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_decompress(p->list, in.data(), in.size(), out, sizeof(out), &out_len), -ENOSPC);
    }
}

// ===========================================================================
// Invalid list selector
// ===========================================================================

TEST_F(WlCodecTest, InvalidList)
{
    uint8_t out[512];
    size_t out_len = 0;
    EXPECT_EQ(wl_codec_compress(WORD_LIST_COUNT, "abandon", 7, out, sizeof(out), &out_len), -EINVAL);
    EXPECT_EQ(wl_codec_decompress(WORD_LIST_COUNT, (const uint8_t *)"\x00\x00", 2, (char *)out, sizeof(out), &out_len),
              -EINVAL);
    EXPECT_EQ(wl_codec_compress_passphrase(WORD_LIST_COUNT, "abandon; p", 10, out, sizeof(out), &out_len), -EINVAL);
    EXPECT_EQ(wl_codec_decompress_passphrase(
                  WORD_LIST_COUNT, (const uint8_t *)"\x01\x00\x00", 3, (char *)out, sizeof(out), &out_len),
              -EINVAL);
}

// ===========================================================================
// Passphrase compression / decompression
// ===========================================================================

TEST_F(WlCodecTest, Passphrase_Compress_Format)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = multiPhrase(*p) + "; Hi";
        std::vector<uint8_t> out = compressPassphrase(p->list, phrase);

        std::vector<uint8_t> expected = {0x03};
        auto idx0 = indexBytes(0);
        auto idxLast = indexBytes(p->last_index);
        expected.insert(expected.end(), idx0.begin(), idx0.end());
        expected.insert(expected.end(), idxLast.begin(), idxLast.end());
        expected.insert(expected.end(), idxLast.begin(), idxLast.end());
        expected.push_back(0x48); // 'H'
        expected.push_back(0x69); // 'i'

        ASSERT_EQ(out.size(), expected.size());
        for (size_t i = 0; i < expected.size(); i++)
        {
            EXPECT_EQ(out[i], expected[i]) << "byte " << i;
        }
    }
}

TEST_F(WlCodecTest, Passphrase_RoundTrip)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string base = multiPhrase(*p);
        std::vector<std::pair<std::string, std::string>> cases = {
            {base + "; My Passphrase!", base + " ; My Passphrase!"},
            {base + "; a;b;c", base + " ; a;b;c"},
            {base + ";", base},
            {std::string(p->single[0].first) + "; ", p->single[0].first},
            {" " + std::string(p->single[1].first) + " " + std::string(p->single[2].first) +
                 " ;  leading and trailing  ",
             std::string(p->single[1].first) + " " + std::string(p->single[2].first) + " ; leading and trailing"},
        };

        for (const auto &c : cases)
        {
            std::vector<uint8_t> compressed = compressPassphrase(p->list, c.first);
            ASSERT_FALSE(compressed.empty()) << c.first;
            EXPECT_EQ(decompressPassphrase(p->list, compressed), c.second) << c.first;
        }
    }
}

TEST_F(WlCodecTest, Passphrase_RoundTrip_UTF8)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string base = multiPhrase(*p);
        std::string phrase = base + "; p\u00e4ssphrase \u00e9\u00e8";
        std::vector<uint8_t> compressed = compressPassphrase(p->list, phrase);
        ASSERT_FALSE(compressed.empty());

        std::string recovered = decompressPassphrase(p->list, compressed);
        EXPECT_EQ(recovered, base + " ; p\u00e4ssphrase \u00e9\u00e8");
    }
}

TEST_F(WlCodecTest, Passphrase_RoundTrip_MaxWords)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = repeatWord(p->single[0].first, p->max_words) + "; a passphrase";

        std::vector<uint8_t> compressed = compressPassphrase(p->list, phrase);
        ASSERT_FALSE(compressed.empty());
        EXPECT_EQ(compressed[0], p->max_words);

        std::string recovered = decompressPassphrase(p->list, compressed);
        std::string expected = repeatWord(p->single[0].first, p->max_words) + " ; a passphrase";
        EXPECT_EQ(recovered, expected);
    }
}

TEST_F(WlCodecTest, Passphrase_Compress_NoSeparator)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = multiPhrase(*p);
        std::vector<uint8_t> out(512);
        size_t out_len = 0;
        EXPECT_EQ(
            wl_codec_compress_passphrase(p->list, phrase.c_str(), phrase.size(), out.data(), out.size(), &out_len),
            -EINVAL);
    }
}

TEST_F(WlCodecTest, Passphrase_Compress_EmptySeed)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::vector<uint8_t> out(512);
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress_passphrase(p->list, "; pass", 6, out.data(), out.size(), &out_len), -EINVAL);
    }
}

TEST_F(WlCodecTest, Passphrase_Compress_InvalidSeedWord)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = std::string(p->single[0].first) + " notaword; pass";
        std::vector<uint8_t> out(512);
        size_t out_len = 0;
        EXPECT_EQ(
            wl_codec_compress_passphrase(p->list, phrase.c_str(), phrase.size(), out.data(), out.size(), &out_len),
            -EINVAL);
    }
}

TEST_F(WlCodecTest, Passphrase_Compress_TooManyWords)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = repeatWord(p->single[0].first, p->max_words + 1) + "; pass";
        std::vector<uint8_t> out(512);
        size_t out_len = 0;
        EXPECT_EQ(
            wl_codec_compress_passphrase(p->list, phrase.c_str(), phrase.size(), out.data(), out.size(), &out_len),
            -EINVAL);
    }
}

TEST_F(WlCodecTest, Passphrase_Compress_OutputTooSmall)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string phrase = multiPhrase(*p) + "; Hi";
        uint8_t out[4];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_compress_passphrase(p->list, phrase.c_str(), phrase.size(), out, sizeof(out), &out_len),
                  -ENOSPC);
    }
}

TEST_F(WlCodecTest, Passphrase_Decompress_Errors)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::string out(512, '\0');
        size_t out_len = 0;

        // Null pointers.
        std::vector<uint8_t> ok = {0x01, 0x00, 0x00};
        EXPECT_EQ(wl_codec_decompress_passphrase(p->list, nullptr, ok.size(), &out[0], out.size(), &out_len), -EINVAL);
        EXPECT_EQ(wl_codec_decompress_passphrase(p->list, ok.data(), 0, &out[0], out.size(), &out_len), -EINVAL);
        EXPECT_EQ(wl_codec_decompress_passphrase(p->list, ok.data(), ok.size(), nullptr, out.size(), &out_len),
                  -EINVAL);
        EXPECT_EQ(wl_codec_decompress_passphrase(p->list, ok.data(), ok.size(), &out[0], out.size(), nullptr), -EINVAL);

        // word_count == 0.
        std::vector<uint8_t> zero_words = {0x00};
        EXPECT_EQ(wl_codec_decompress_passphrase(
                      p->list, zero_words.data(), zero_words.size(), &out[0], out.size(), &out_len),
                  -EINVAL);

        // word_count > max.
        std::vector<uint8_t> too_many = {(uint8_t)(p->max_words + 1)};
        EXPECT_EQ(
            wl_codec_decompress_passphrase(p->list, too_many.data(), too_many.size(), &out[0], out.size(), &out_len),
            -EINVAL);

        // Truncated index bytes (word_count=2 needs 5 bytes, only 3 given).
        std::vector<uint8_t> truncated = {0x02, 0x00, 0x00};
        EXPECT_EQ(
            wl_codec_decompress_passphrase(p->list, truncated.data(), truncated.size(), &out[0], out.size(), &out_len),
            -EINVAL);

        // Index out of range.
        std::vector<uint8_t> bad_index = {
            0x01, (uint8_t)(p->out_of_range_index & 0xFF), (uint8_t)(p->out_of_range_index >> 8)};
        EXPECT_EQ(
            wl_codec_decompress_passphrase(p->list, bad_index.data(), bad_index.size(), &out[0], out.size(), &out_len),
            -EINVAL);
    }
}

TEST_F(WlCodecTest, Passphrase_Decompress_OutputTooSmall)
{
    for (const wl_params *p : kParams)
    {
        SCOPED_TRACE(p->name);
        std::vector<uint8_t> in = {0x01, 0x00, 0x00, 'H', 'i'};
        char out[4];
        size_t out_len = 0;
        EXPECT_EQ(wl_codec_decompress_passphrase(p->list, in.data(), in.size(), out, sizeof(out), &out_len), -ENOSPC);
    }
}
