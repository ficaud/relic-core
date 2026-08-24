// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the QR Code generator (qr_encode.c / qr_encode.h).

#include "qr_encode.h"
#include "golden_shares.h"
#include "golden_base32_qr_grids.h"
#include "golden_hex_qr_grids.h"

extern "C"
{
}

#include <cstring>
#include <gtest/gtest.h>
#include <string>

// ===========================================================================
// Test helpers
// ===========================================================================

namespace
{

// Buffer large enough for any QR Code (version 40).
constexpr size_t kBufLen = qrcodegen_BUFFER_LEN_MAX;

/**
 * @brief Read a single module (pixel) of the QR Code grid.
 *
 * Mirrors the internal getModuleBounded() layout: qrcode[0] holds the side
 * length, the module grid is packed from byte 1, row-major, LSB first.
 *
 * @param qrcode  QR Code buffer produced by qrcodegen_encodeText().
 * @param x       Column (0 = left).
 * @param y       Row (0 = top).
 * @return true if the module is dark, false if light or out of bounds.
 */
bool getModule(const uint8_t qrcode[], int x, int y)
{
    int size = qrcode[0];
    if (x < 0 || y < 0 || x >= size || y >= size)
    {
        return false;
    }
    int index = y * size + x;
    return ((qrcode[(index >> 3) + 1] >> (index & 7)) & 1) != 0;
}

/**
 * @brief Encode text and return the QR Code buffer.
 *
 * @param text  UTF-8 text to encode.
 * @return true on success, false on failure.
 */
bool encodeText(const std::string &text, uint8_t qrcode[kBufLen])
{
    uint8_t temp[kBufLen];
    return qrcodegen_encodeText(text.c_str(), temp, qrcode);
}

/**
 * @brief Check that the three finder patterns (position detection squares)
 *        are present at the expected corners of the QR Code.
 *
 * Each finder pattern is a 7x7 dark square with a 3x3 dark center and a
 * 1-module light ring. We verify the four corners of the outer square and
 * the center module.
 *
 * @param qrcode  QR Code buffer.
 * @param size    Side length in modules.
 */
void expectFinderPattern(const uint8_t qrcode[], int size, int ox, int oy)
{
    // Outer 7x7 square corners are dark.
    EXPECT_TRUE(getModule(qrcode, ox, oy));
    EXPECT_TRUE(getModule(qrcode, ox + 6, oy));
    EXPECT_TRUE(getModule(qrcode, ox, oy + 6));
    EXPECT_TRUE(getModule(qrcode, ox + 6, oy + 6));

    // Center 3x3 square is dark.
    EXPECT_TRUE(getModule(qrcode, ox + 3, oy + 3));

    // The ring just inside the outer square is light (separator).
    EXPECT_FALSE(getModule(qrcode, ox + 1, oy + 1));
    EXPECT_FALSE(getModule(qrcode, ox + 5, oy + 1));
    EXPECT_FALSE(getModule(qrcode, ox + 1, oy + 5));
    EXPECT_FALSE(getModule(qrcode, ox + 5, oy + 5));
}

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class QRCodeTest : public ::testing::Test
{
  protected:
    uint8_t qrcode_[kBufLen];
    uint8_t temp_[kBufLen];
};

// ===========================================================================
// Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// Basic encoding
// ---------------------------------------------------------------------------

TEST_F(QRCodeTest, EncodeShortText)
{
    ASSERT_TRUE(qrcodegen_encodeText("HELLO WORLD", temp_, qrcode_));
    int size = qrcodegen_getSize(qrcode_);
    EXPECT_GE(size, 21); // version 1 minimum
    EXPECT_LE(size, 177); // version 40 maximum
    EXPECT_EQ(size % 4, 1); // size = 4*version + 17
}

TEST_F(QRCodeTest, EncodeEmptyString)
{
    ASSERT_TRUE(qrcodegen_encodeText("", temp_, qrcode_));
    EXPECT_GE(qrcodegen_getSize(qrcode_), 21);
}

TEST_F(QRCodeTest, EncodeDigitsText)
{
    // Digits are valid alphanumeric characters (encoded in alphanumeric mode).
    ASSERT_TRUE(qrcodegen_encodeText("0123456789", temp_, qrcode_));
    EXPECT_GE(qrcodegen_getSize(qrcode_), 21);
}

TEST_F(QRCodeTest, EncodeAlphanumericText)
{
    // Alphanumeric mode: uppercase letters, digits and a few symbols.
    ASSERT_TRUE(qrcodegen_encodeText("HELLO WORLD 123", temp_, qrcode_));
    EXPECT_GE(qrcodegen_getSize(qrcode_), 21);
}

// ---------------------------------------------------------------------------
// Size / version selection
// ---------------------------------------------------------------------------

TEST_F(QRCodeTest, SizeIncreasesWithContentLength)
{
    std::string shortText = "A";
    std::string longText(200, 'A');

    uint8_t shortQr[kBufLen];
    uint8_t longQr[kBufLen];

    ASSERT_TRUE(encodeText(shortText, shortQr));
    ASSERT_TRUE(encodeText(longText, longQr));

    int shortSize = qrcodegen_getSize(shortQr);
    int longSize = qrcodegen_getSize(longQr);

    EXPECT_GE(longSize, shortSize) << "Longer content should not produce a smaller QR Code";
}

TEST_F(QRCodeTest, SizeIsMultipleOfFourPlusOne)
{
    ASSERT_TRUE(qrcodegen_encodeText("RELIC CORE", temp_, qrcode_));
    int size = qrcodegen_getSize(qrcode_);
    EXPECT_EQ((size - 17) % 4, 0);
    EXPECT_GE(size, 21);
    EXPECT_LE(size, 177);
}

// ---------------------------------------------------------------------------
// Determinism
// ---------------------------------------------------------------------------

TEST_F(QRCodeTest, SameInputProducesSameOutput)
{
    uint8_t qr1[kBufLen];
    uint8_t qr2[kBufLen];

    ASSERT_TRUE(encodeText("DETERMINISTIC", qr1));
    ASSERT_TRUE(encodeText("DETERMINISTIC", qr2));

    int size = qrcodegen_getSize(qr1);
    ASSERT_EQ(size, qrcodegen_getSize(qr2));

    // Compare the full grid (byte 0 = size, then the module bytes).
    size_t bytes = (size * size + 7) / 8 + 1;
    EXPECT_EQ(std::memcmp(qr1, qr2, bytes), 0) << "Same input must produce identical QR Codes";
}

TEST_F(QRCodeTest, DifferentInputProducesDifferentOutput)
{
    uint8_t qr1[kBufLen];
    uint8_t qr2[kBufLen];

    ASSERT_TRUE(encodeText("FIRST", qr1));
    ASSERT_TRUE(encodeText("SECOND", qr2));

    int size = qrcodegen_getSize(qr1);
    ASSERT_EQ(size, qrcodegen_getSize(qr2));

    size_t bytes = (size * size + 7) / 8 + 1;
    EXPECT_NE(std::memcmp(qr1, qr2, bytes), 0) << "Different inputs should produce different QR Codes";
}

// ---------------------------------------------------------------------------
// Structure: finder patterns
// ---------------------------------------------------------------------------

TEST_F(QRCodeTest, FinderPatternsPresent)
{
    ASSERT_TRUE(qrcodegen_encodeText("RELIC CORE", temp_, qrcode_));
    int size = qrcodegen_getSize(qrcode_);

    // Top-left, top-right and bottom-left finder patterns.
    expectFinderPattern(qrcode_, size, 0, 0);
    expectFinderPattern(qrcode_, size, size - 7, 0);
    expectFinderPattern(qrcode_, size, 0, size - 7);
}

TEST_F(QRCodeTest, NotUniformGrid)
{
    ASSERT_TRUE(qrcodegen_encodeText("NOT UNIFORM TEST", temp_, qrcode_));
    int size = qrcodegen_getSize(qrcode_);

    // A valid QR Code is never a solid block: it must contain both dark and
    // light modules. Count the dark modules and ensure it is neither empty
    // nor the whole grid.
    int darkCount = 0;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            if (getModule(qrcode_, x, y))
            {
                darkCount++;
            }
        }
    }

    EXPECT_GT(darkCount, 0) << "QR Code should contain at least one dark module";
    EXPECT_LT(darkCount, size * size) << "QR Code should not be entirely dark";
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST_F(QRCodeTest, TextTooLongFails)
{
    // A very long text that cannot fit in any version (1..40) at ECC LOW.
    std::string tooLong(5000, 'X');
    EXPECT_FALSE(qrcodegen_encodeText(tooLong.c_str(), temp_, qrcode_));
    EXPECT_EQ(qrcode_[0], 0) << "qrcode[0] should be set to 0 (invalid size sentinel) on failure";
}

TEST_F(QRCodeTest, NullTextFails)
{
    EXPECT_FALSE(qrcodegen_encodeText(nullptr, temp_, qrcode_));
}

TEST_F(QRCodeTest, NullBuffersFail)
{
    EXPECT_FALSE(qrcodegen_encodeText("TEST", nullptr, qrcode_));
    EXPECT_FALSE(qrcodegen_encodeText("TEST", temp_, nullptr));
}

TEST_F(QRCodeTest, GetSizeNullReturnsMinusOne)
{
    EXPECT_EQ(qrcodegen_getSize(nullptr), -1);
}

// ---------------------------------------------------------------------------
// Capacity / boundary
// ---------------------------------------------------------------------------

TEST_F(QRCodeTest, DigitsCapacityFits)
{
    // Digits are valid alphanumeric characters; a 100-digit string should fit easily.
    std::string digits(100, '7');
    ASSERT_TRUE(encodeText(digits, qrcode_));
    EXPECT_GE(qrcodegen_getSize(qrcode_), 21);
}

TEST_F(QRCodeTest, AlphanumericCapacityBoundary)
{
    // Alphanumeric mode: ~4296 chars fit in version 40 at ECC LOW. Use a value
    // well within range but large enough to require a high version.
    std::string text(1000, 'A');
    ASSERT_TRUE(encodeText(text, qrcode_));
    int size = qrcodegen_getSize(qrcode_);
    EXPECT_GT(size, 21) << "1000 chars should require a version larger than 1";
}


// ---------------------------------------------------------------------------
// Real cases deterministic tests from pre-generated QR codes that we know are true
// ---------------------------------------------------------------------------
//
// These tests are generated from an edge case scenario: a 24-word seed phrase
//
//   "abandon abandon abandon abandon abandon abandon abandon abandon abandon
//    abandon abandon abandon abandon abandon abandon abandon abandon abandon
//    abandon abandon abandon abandon abandon art"
//
// The shares have been generated from this phrase and encoded as QR codes.
// The payload uses the base32-compressed share form "x:base32..." (version 9,
// 53x53 modules), which is what the /qr-share.svg endpoint produces. The
// golden QR grid byte arrays are stored in golden_base32_qr_grids.h so that
// re-encoding the same base32 share string must reproduce the exact same grid
// byte-for-byte. This guards against any regression that would silently change
// the generated QR codes.

namespace
{

// The golden grids (base32 payloads) in the same order as the base32_shareN
// string constants.
static const uint8_t *const kShareGoldenGrids[] = {
    base32_share1_qr,
    base32_share2_qr,
    base32_share3_qr,
    base32_share4_qr,
    base32_share5_qr,
};

} // namespace

// ---------------------------------------------------------------------------
// Real-case deterministic tests (golden grids)
// ---------------------------------------------------------------------------

// Each base32 share string must reproduce its golden QR grid byte-for-byte when
// re-encoded. The grid covers qrcode[0] (side length) plus the packed module
// bytes, so the full buffer is compared.
TEST_F(QRCodeTest, ShareBase32GoldenGridMatches)
{
    // The five base32 share strings in the same order as the golden grids.
    const char *const kShareStrings[] = {
        base32_share1,
        base32_share2,
        base32_share3,
        base32_share4,
        base32_share5,
    };

    for (int i = 0; i < 5; i++)
    {
        uint8_t qr[kBufLen];
        ASSERT_TRUE(encodeText(kShareStrings[i], qr)) << "Share " << (i + 1) << " should encode successfully";

        const uint8_t *golden = kShareGoldenGrids[i];
        const int size = qrcodegen_getSize(qr);
        ASSERT_EQ(size, (int)golden[0]) << "Share " << (i + 1) << " QR size mismatch";
        const size_t bytes = (size * size + 7) / 8 + 1;

        EXPECT_EQ(std::memcmp(qr, golden, bytes), 0)
            << "Share " << (i + 1) << " QR grid does not match the golden reference";
    }
}

// The five base32 golden grids must all be mutually distinct, since each share
// encodes different payload data.
TEST_F(QRCodeTest, Base32GoldenGridsAreDistinct)
{
    const size_t bytes = (53 * 53 + 7) / 8 + 1; /* version 9 grid */
    for (int i = 0; i < 5; i++)
    {
        for (int j = i + 1; j < 5; j++)
        {
            EXPECT_NE(std::memcmp(kShareGoldenGrids[i], kShareGoldenGrids[j], bytes), 0)
                << "Shares " << (i + 1) << " and " << (j + 1) << " must have distinct QR grids";
        }
    }
}

// The golden grids (hex payloads) in the same order as the seedphrase_shareN
// string constants.
static const uint8_t *const kSeedPhraseGoldenGrids[] = {
    seedphrase_share1_qr,
    seedphrase_share2_qr,
    seedphrase_share3_qr,
    seedphrase_share4_qr,
    seedphrase_share5_qr,
};

// Each hex share string must reproduce its golden QR grid byte-for-byte when
// re-encoded. The grid covers qrcode[0] (side length) plus the packed module
// bytes, so the full 408-byte buffer is compared.
TEST_F(QRCodeTest, ShareHexGoldenGridMatches)
{
    // The five hex share strings in the same order as the golden grids.
    const char *const kShareStrings[] = {
        seedphrase_share1,
        seedphrase_share2,
        seedphrase_share3,
        seedphrase_share4,
        seedphrase_share5,
    };

    for (int i = 0; i < 5; i++)
    {
        // Uppercase the share string (the front-end uppercases the hex payload
        // before requesting the QR code, see split.js).
        std::string upper = kShareStrings[i];
        for (char &c : upper)
        {
            if (c >= 'a' && c <= 'z')
            {
                c = static_cast<char>(c - 'a' + 'A');
            }
        }

        uint8_t qr[kBufLen];
        ASSERT_TRUE(encodeText(upper, qr)) << "Share " << (i + 1) << " should encode successfully";

        const uint8_t *golden = kSeedPhraseGoldenGrids[i];
        const int size = qrcodegen_getSize(qr);
        ASSERT_EQ(size, (int)golden[0]) << "Share " << (i + 1) << " QR size mismatch";
        const size_t bytes = (size * size + 7) / 8 + 1;

        EXPECT_EQ(std::memcmp(qr, golden, bytes), 0)
            << "Share " << (i + 1) << " QR grid does not match the golden reference";
    }
}

// The five hex golden grids must all be mutually distinct, since each share
// encodes different payload data.
TEST_F(QRCodeTest, HexGoldenGridsAreDistinct)
{
    const size_t bytes = (57 * 57 + 7) / 8 + 1; /* version 10 grid */
    for (int i = 0; i < 5; i++)
    {
        for (int j = i + 1; j < 5; j++)
        {
            EXPECT_NE(std::memcmp(kSeedPhraseGoldenGrids[i], kSeedPhraseGoldenGrids[j], bytes), 0)
                << "Shares " << (i + 1) << " and " << (j + 1) << " must have distinct QR grids";
        }
    }
}
