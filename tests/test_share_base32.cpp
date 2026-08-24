// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the share base32 codec (share_base32.c / share_base32.h).

#include "share_base32.h"
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

std::string bytesToHex(const std::vector<uint8_t> &bytes)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes)
    {
        out.push_back(hex[(b >> 4) & 0x0F]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class ShareBase32Test : public ::testing::Test
{
  protected:
    char b32_buf_[512];
    char hex_buf_[1024];
};

// ===========================================================================
// Round-trip
// ===========================================================================

TEST_F(ShareBase32Test, RoundTrip)
{
    const std::vector<size_t> lengths = {1, 2, 3, 5, 16, 31, 32, 127, 128, 256};

    for (size_t len : lengths)
    {
        std::vector<uint8_t> data(len);
        for (size_t i = 0; i < len; i++)
        {
            data[i] = (uint8_t)(i * 7 + 13);
        }
        std::string hex_share = "1:" + bytesToHex(data);

        ASSERT_EQ(share_to_base32(hex_share.c_str(), b32_buf_, sizeof(b32_buf_)), 0) << "len " << len;
        std::string b32_share = b32_buf_;

        ASSERT_EQ(share_from_base32(b32_share.c_str(), b32_share.size(), hex_buf_, sizeof(hex_buf_)), 0)
            << "len " << len;
        EXPECT_EQ(std::string(hex_buf_), hex_share) << "len " << len;
    }
}

// ===========================================================================
// Golden seed-phrase shares
// ===========================================================================

TEST_F(ShareBase32Test, GoldenEncode)
{
    const char *const hex_shares[] = {
        seedphrase_share1, seedphrase_share2, seedphrase_share3, seedphrase_share4, seedphrase_share5,
    };
    const char *const b32_shares[] = {
        base32_share1, base32_share2, base32_share3, base32_share4, base32_share5,
    };

    for (int i = 0; i < 5; i++)
    {
        ASSERT_EQ(share_to_base32(hex_shares[i], b32_buf_, sizeof(b32_buf_)), 0);
        EXPECT_STREQ(b32_buf_, b32_shares[i]) << "share " << (i + 1);
    }
}

TEST_F(ShareBase32Test, GoldenDecode)
{
    const char *const hex_shares[] = {
        seedphrase_share1, seedphrase_share2, seedphrase_share3, seedphrase_share4, seedphrase_share5,
    };
    const char *const b32_shares[] = {
        base32_share1, base32_share2, base32_share3, base32_share4, base32_share5,
    };

    for (int i = 0; i < 5; i++)
    {
        ASSERT_EQ(share_from_base32(b32_shares[i], std::strlen(b32_shares[i]), hex_buf_, sizeof(hex_buf_)), 0);
        EXPECT_STREQ(hex_buf_, hex_shares[i]) << "share " << (i + 1);
    }
}

// ===========================================================================
// Case-insensitivity
// ===========================================================================

TEST_F(ShareBase32Test, LowercaseHexAccepted)
{
    char a[512];
    char b[512];

    ASSERT_EQ(share_to_base32("1:ab01cd23ef", a, sizeof(a)), 0);
    ASSERT_EQ(share_to_base32("1:AB01CD23EF", b, sizeof(b)), 0);
    EXPECT_STREQ(a, b);
}

TEST_F(ShareBase32Test, LowercaseBase32Accepted)
{
    char a[1024];
    char b[1024];
    const char *upper = "1:MZXW6YTB";
    const char *lower = "1:mzxw6ytb";

    ASSERT_EQ(share_from_base32(upper, std::strlen(upper), a, sizeof(a)), 0);
    ASSERT_EQ(share_from_base32(lower, std::strlen(lower), b, sizeof(b)), 0);
    EXPECT_STREQ(a, b);
}

// ===========================================================================
// Error handling
// ===========================================================================

TEST_F(ShareBase32Test, EncodeErrors)
{
    EXPECT_NE(share_to_base32(nullptr, b32_buf_, sizeof(b32_buf_)), 0);
    EXPECT_NE(share_to_base32("1:ABCD", nullptr, sizeof(b32_buf_)), 0);
    EXPECT_NE(share_to_base32("ABCD", b32_buf_, sizeof(b32_buf_)), 0);     // no colon
    EXPECT_NE(share_to_base32(":ABCD", b32_buf_, sizeof(b32_buf_)), 0);    // empty x
    EXPECT_NE(share_to_base32("0:ABCD", b32_buf_, sizeof(b32_buf_)), 0);   // x < 1
    EXPECT_NE(share_to_base32("256:ABCD", b32_buf_, sizeof(b32_buf_)), 0); // x > 255
    EXPECT_NE(share_to_base32("x:ABCD", b32_buf_, sizeof(b32_buf_)), 0);   // non-numeric x
    EXPECT_NE(share_to_base32("1:A", b32_buf_, sizeof(b32_buf_)), 0);      // odd hex length
    EXPECT_NE(share_to_base32("1:ZZ", b32_buf_, sizeof(b32_buf_)), 0);     // bad hex char
}

TEST_F(ShareBase32Test, DecodeErrors)
{
    EXPECT_NE(share_from_base32(nullptr, 4, hex_buf_, sizeof(hex_buf_)), 0);
    EXPECT_NE(share_from_base32("1:MY", 4, nullptr, sizeof(hex_buf_)), 0);
    EXPECT_NE(share_from_base32("ABCD", 4, hex_buf_, sizeof(hex_buf_)), 0);      // no colon
    EXPECT_NE(share_from_base32(":MY", 3, hex_buf_, sizeof(hex_buf_)), 0);       // empty x
    EXPECT_NE(share_from_base32("0:MY", 4, hex_buf_, sizeof(hex_buf_)), 0);      // x < 1
    EXPECT_NE(share_from_base32("256:MY", 6, hex_buf_, sizeof(hex_buf_)), 0);    // x > 255
    EXPECT_NE(share_from_base32("1:M", 3, hex_buf_, sizeof(hex_buf_)), 0);       // invalid length (mod 8 == 1)
    EXPECT_NE(share_from_base32("1:MYA0", 6, hex_buf_, sizeof(hex_buf_)), 0);    // forbidden char '0'
}

TEST_F(ShareBase32Test, OutputTooSmall)
{
    char tiny[2];
    EXPECT_NE(share_to_base32("1:ABCD", tiny, sizeof(tiny)), 0);

    char tiny_hex[4];
    EXPECT_NE(share_from_base32("1:MZXW6", 7, tiny_hex, sizeof(tiny_hex)), 0);
}
