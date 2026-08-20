// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the base32 codec (base32.c / base32.h).

#include "base32.h"
#include "golden_shares.h"

extern "C"
{
}

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
 * @brief Convert a hexadecimal string into bytes.
 *
 * @param hex   Hex string (uppercase or lowercase).
 * @return Decoded bytes.
 */
std::vector<uint8_t> hexToBytes(const std::string &hex)
{
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        char hi = hex[i];
        char lo = hex[i + 1];
        auto nibble = [](char c) -> int
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f')
            {
                return c - 'a' + 10;
            }
            return c - 'A' + 10;
        };
        out.push_back((uint8_t)((nibble(hi) << 4) | nibble(lo)));
    }
    return out;
}

/**
 * @brief Extract the hex payload (after the "N:" prefix) and decode it.
 */
std::vector<uint8_t> shareHexBytes(const char *share)
{
    const char *colon = std::strchr(share, ':');
    return hexToBytes(colon == nullptr ? share : colon + 1);
}

/**
 * @brief Return the share payload (the part after the "N:" prefix).
 */
const char *sharePayload(const char *share)
{
    const char *colon = std::strchr(share, ':');
    return colon == nullptr ? share : colon + 1;
}

/**
 * @brief Golden seed-phrase shares in base32 form (from golden_shares.h),
 *        in share order.
 */
const char *const base32_shares[] = {
    base32_share1,
    base32_share2,
    base32_share3,
    base32_share4,
    base32_share5,
};

const char *const golden_hex[] = {
    seedphrase_share1,
    seedphrase_share2,
    seedphrase_share3,
    seedphrase_share4,
    seedphrase_share5,
};

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class Base32Test : public ::testing::Test
{
  protected:
};

// ===========================================================================
// RFC 4648 test vectors (padding stripped)
// ===========================================================================

TEST_F(Base32Test, Rfc4648Vectors_Encode)
{
    struct
    {
        const char *input;
        const char *expected;
    } cases[] = {
        {"", ""},
        {"f", "MY"},
        {"fo", "MZXQ"},
        {"foo", "MZXW6"},
        {"foob", "MZXW6YQ"},
        {"fooba", "MZXW6YTB"},
        {"foobar", "MZXW6YTBOI"},
    };

    for (const auto &c : cases)
    {
        char out[64];
        size_t in_len = std::strlen(c.input);
        ASSERT_EQ(base32_encode((const uint8_t *)c.input, in_len, out, sizeof(out)), 0);
        EXPECT_STREQ(out, c.expected);
    }
}

TEST_F(Base32Test, Rfc4648Vectors_Decode)
{
    struct
    {
        const char *input;
        const char *expected;
    } cases[] = {
        {"", ""},
        {"MY", "f"},
        {"MZXQ", "fo"},
        {"MZXW6", "foo"},
        {"MZXW6YQ", "foob"},
        {"MZXW6YTB", "fooba"},
        {"MZXW6YTBOI", "foobar"},
    };

    for (const auto &c : cases)
    {
        uint8_t out[64];
        size_t out_len = 0;
        size_t in_len = std::strlen(c.input);
        ASSERT_EQ(base32_decode(c.input, in_len, out, sizeof(out), &out_len), 0);
        ASSERT_EQ(out_len, std::strlen(c.expected));
        EXPECT_EQ(std::memcmp(out, c.expected, out_len), 0);
    }
}

// ===========================================================================
// Round-trip
// ===========================================================================

TEST_F(Base32Test, RoundTrip_Lengths)
{
    const std::vector<size_t> lengths = {0,  1,  2,  3,  4,  5,  6,  7,   8,   9,   10,
                                         15, 16, 31, 32, 33, 63, 64, 127, 128, 129, 187};

    for (size_t len : lengths)
    {
        std::vector<uint8_t> input(len);
        for (size_t i = 0; i < len; i++)
        {
            input[i] = (uint8_t)(i * 7 + 13);
        }

        std::vector<char> encoded(base32_encoded_len(len));
        ASSERT_EQ(base32_encode(input.data(), len, encoded.data(), encoded.size()), 0);

        std::vector<uint8_t> decoded(len > 0 ? len : 1);
        size_t out_len = 0;
        ASSERT_EQ(base32_decode(encoded.data(), std::strlen(encoded.data()), decoded.data(), decoded.size(), &out_len),
                  0);
        ASSERT_EQ(out_len, len);
        EXPECT_EQ(std::memcmp(decoded.data(), input.data(), len), 0);
    }
}

TEST_F(Base32Test, EncodedLen)
{
    EXPECT_EQ(base32_encoded_len(0), 1);
    EXPECT_EQ(base32_encoded_len(1), 3);
    EXPECT_EQ(base32_encoded_len(2), 5);
    EXPECT_EQ(base32_encoded_len(5), 9);
    EXPECT_EQ(base32_encoded_len(187), 301);
}

// ===========================================================================
// Error handling
// ===========================================================================

TEST_F(Base32Test, Encode_OutputTooSmall)
{
    const uint8_t input[] = "hello";
    char out[4];
    EXPECT_NE(base32_encode(input, 5, out, sizeof(out)), 0);
}

TEST_F(Base32Test, Encode_NullOutput)
{
    const uint8_t input[] = "hello";
    EXPECT_NE(base32_encode(input, 5, nullptr, 32), 0);
}

TEST_F(Base32Test, Encode_NullInput)
{
    char out[32];
    EXPECT_NE(base32_encode(nullptr, 5, out, sizeof(out)), 0);
}

TEST_F(Base32Test, Decode_InvalidLength)
{
    uint8_t out[32];
    size_t out_len = 0;

    // Lengths mod 8 == 1, 3, 6 are invalid for unpadded base32.
    EXPECT_NE(base32_decode("A", 1, out, sizeof(out), &out_len), 0);
    EXPECT_NE(base32_decode("ABC", 3, out, sizeof(out), &out_len), 0);
    EXPECT_NE(base32_decode("ABCDEF", 6, out, sizeof(out), &out_len), 0);
}

TEST_F(Base32Test, Decode_ForbiddenChars)
{
    uint8_t out[32];
    size_t out_len = 0;

    const char *bad[] = {"0", "1", "8", "9", "-", "_", "=", " ", "MY=", "MZXW6!", nullptr};
    for (int i = 0; bad[i] != nullptr; i++)
    {
        EXPECT_NE(base32_decode(bad[i], std::strlen(bad[i]), out, sizeof(out), &out_len), 0) << "char: " << bad[i];
    }
}

TEST_F(Base32Test, Decode_OutputTooSmall)
{
    const char *input = "MZXW6YTBOI"; // decodes to 7 bytes
    uint8_t out[4];
    size_t out_len = 0;
    EXPECT_NE(base32_decode(input, std::strlen(input), out, sizeof(out), &out_len), 0);
}

TEST_F(Base32Test, Decode_NullPointers)
{
    uint8_t out[32];
    size_t out_len = 0;
    EXPECT_NE(base32_decode("MZXW6", 5, nullptr, sizeof(out), &out_len), 0);
    EXPECT_NE(base32_decode("MZXW6", 5, out, sizeof(out), nullptr), 0);
    EXPECT_NE(base32_decode(nullptr, 5, out, sizeof(out), &out_len), 0);
}

// ===========================================================================
// Case-insensitive decoding
// ===========================================================================

TEST_F(Base32Test, Decode_Lowercase)
{
    uint8_t upper[32];
    uint8_t lower[32];
    size_t upper_len = 0;
    size_t lower_len = 0;

    ASSERT_EQ(base32_decode("MZXW6YTB", 8, upper, sizeof(upper), &upper_len), 0);
    ASSERT_EQ(base32_decode("mzxw6ytb", 8, lower, sizeof(lower), &lower_len), 0);

    ASSERT_EQ(upper_len, lower_len);
    EXPECT_EQ(std::memcmp(upper, lower, upper_len), 0);
}

// ===========================================================================
// Output charset constraint (QR alphanumeric-safe)
// ===========================================================================

TEST_F(Base32Test, OutputIsAlphanumericSafe)
{
    std::vector<uint8_t> input(187);
    for (size_t i = 0; i < input.size(); i++)
    {
        input[i] = (uint8_t)i;
    }

    std::vector<char> encoded(base32_encoded_len(input.size()));
    ASSERT_EQ(base32_encode(input.data(), input.size(), encoded.data(), encoded.size()), 0);

    for (size_t i = 0; i < std::strlen(encoded.data()); i++)
    {
        char c = encoded[i];
        bool valid = (c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7');
        EXPECT_TRUE(valid) << "unexpected char '" << c << "' at index " << i;
    }
}

// ===========================================================================
// Golden seed-phrase shares
// ===========================================================================

TEST_F(Base32Test, GoldenShares_Encode)
{
    for (int i = 0; i < 5; i++)
    {
        std::vector<uint8_t> bytes = shareHexBytes(golden_hex[i]);

        std::vector<char> encoded(base32_encoded_len(bytes.size()));
        ASSERT_EQ(base32_encode(bytes.data(), bytes.size(), encoded.data(), encoded.size()), 0);

        EXPECT_STREQ(encoded.data(), sharePayload(base32_shares[i])) << "share " << (i + 1);
    }
}

TEST_F(Base32Test, GoldenShares_Decode)
{
    for (int i = 0; i < 5; i++)
    {
        std::vector<uint8_t> expected = shareHexBytes(golden_hex[i]);

        const char *payload = sharePayload(base32_shares[i]);
        std::vector<uint8_t> decoded(expected.size());
        size_t out_len = 0;
        size_t in_len = std::strlen(payload);
        ASSERT_EQ(base32_decode(payload, in_len, decoded.data(), decoded.size(), &out_len), 0);

        ASSERT_EQ(out_len, expected.size());
        EXPECT_EQ(std::memcmp(decoded.data(), expected.data(), out_len), 0) << "share " << (i + 1);
    }
}
