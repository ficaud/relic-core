// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the QR Code → SVG conversion (qrcode_to_svg.c / qrcode_to_svg.h).

#include "qr_encode.h"
#include "qrcode_to_svg.h"
#include "golden_shares.h"

extern "C"
{
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

// ===========================================================================
// Test helpers
// ===========================================================================

namespace
{

// Buffer large enough for any QR Code (version 40).
constexpr size_t kBufLen = qrcodegen_BUFFER_LEN_MAX;

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
 * @brief Read an entire file into a string.
 *
 * @param path  Filesystem path of the file to read.
 * @return The file contents on success, or an empty string on failure (the
 *         caller must also check the @p ok flag).
 */
std::string readFile(const std::string &path, bool *ok)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        if (ok != nullptr)
        {
            *ok = false;
        }
        return std::string();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (ok != nullptr)
    {
        *ok = true;
    }
    return ss.str();
}

/**
 * @brief Encode a share string payload and render the resulting QR grid as SVG.
 *
 * @param share  The share string (e.g. "1:KXGM6J7...") to encode.
 * @return The generated SVG document on success, or an empty string on failure.
 */
std::string renderShareSvg(const std::string &share)
{
    // The QR encoder supports alphanumeric mode only. The base32 share payload
    // (A-Z 2-7) is already uppercase and alphanumeric-safe, so it is encoded
    // as-is. Uppercasing is kept defensively for any lowercased transcription.
    std::string upper = share;
    for (char &c : upper)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }

    uint8_t qrcode[kBufLen];
    if (!encodeText(upper, qrcode))
    {
        return std::string();
    }
    const int size = qrcode[0];
    if (size <= 0)
    {
        return std::string();
    }

    char buf[QR_SVG_BUF_SIZE];
    int len = qrcode_to_svg(qrcode, size, buf, sizeof(buf));
    if (len < 0)
    {
        return std::string();
    }
    return std::string(buf, static_cast<size_t>(len));
}

/**
 * @brief Absolute path of the golden reference SVG file for a given share.
 *
 * @param shareIndex  Share number (1..5).
 * @param format      "base32" (file `seed-phrase-shareN.svg`) or "hex"
 *                    (file `seed-phrase-shareN-hex.svg`).
 * @return The path to the reference file under the directory configured at
 *         build time (REFERENCE_SVG_DIR).
 */
std::string referencePath(int shareIndex, const std::string &format)
{
    std::string suffix = (format == "hex") ? "-hex" : "";
    return std::string(REFERENCE_SVG_DIR) + "/seed-phrase-share" + std::to_string(shareIndex) + suffix + ".svg";
}

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class SVGTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Encode a fixed, non-trivial payload so every test has a valid QR grid.
        ASSERT_TRUE(encodeText("RELIC CORE", qrcode_));
        size_ = qrcode_[0];
        ASSERT_GT(size_, 0);
    }

    uint8_t qrcode_[kBufLen];
    int size_ = 0;
};

// ===========================================================================
// Tests
// ===========================================================================

// ---------------------------------------------------------------------------
// Basic structure
// ---------------------------------------------------------------------------

TEST_F(SVGTest, ReturnsPositiveLength)
{
    char buf[QR_SVG_BUF_SIZE];
    int len = qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));
    EXPECT_GT(len, 0) << "SVG generation should succeed with a large-enough buffer";
}

TEST_F(SVGTest, StartsWithXmlDeclaration)
{
    char buf[QR_SVG_BUF_SIZE];
    int len = qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));
    ASSERT_GT(len, 0);
    EXPECT_STREQ(std::string(buf, strlen("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")).c_str(),
                 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
}

TEST_F(SVGTest, ContainsSvgRootAndViewBox)
{
    char buf[QR_SVG_BUF_SIZE];
    qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));

    int img_size = size_ * SVG_SCALE + 2 * SVG_MARGIN;
    EXPECT_NE(std::strstr(buf, "<svg"), nullptr);
    EXPECT_NE(std::strstr(buf, "xmlns=\"http://www.w3.org/2000/svg\""), nullptr);
    EXPECT_NE(std::strstr(buf, "viewBox=\"0 0 "), nullptr);

    // The viewBox must match the computed image size.
    char expected[64];
    snprintf(expected, sizeof(expected), "viewBox=\"0 0 %d %d\"", img_size, img_size);
    EXPECT_NE(std::strstr(buf, expected), nullptr);
}

TEST_F(SVGTest, EndsWithClosingSvgTag)
{
    char buf[QR_SVG_BUF_SIZE];
    int len = qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));
    ASSERT_GT(len, 0);
    // The document must end with the closing </svg> tag (optionally followed
    // by a trailing newline).
    const char *closing = "</svg>";
    EXPECT_GT(len, (int)strlen(closing));
    // Look at the tail of the document, stripping an optional trailing newline.
    std::string tail(buf + len - strlen(closing) - 1);
    EXPECT_EQ(tail, "</svg>\n") << "Document must end with the closing </svg> tag";
}

// ---------------------------------------------------------------------------
// Content
// ---------------------------------------------------------------------------

TEST_F(SVGTest, ContainsWhiteBackgroundRect)
{
    char buf[QR_SVG_BUF_SIZE];
    qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));
    EXPECT_NE(std::strstr(buf, "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>"), nullptr);
}

TEST_F(SVGTest, ContainsPathForDarkModules)
{
    char buf[QR_SVG_BUF_SIZE];
    qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));
    EXPECT_NE(std::strstr(buf, "<path d=\""), nullptr);
}

TEST_F(SVGTest, DarkModulesAreRenderedAsPathCommands)
{
    char buf[QR_SVG_BUF_SIZE];
    qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));

    // Every dark module must appear as a path command M{x},{y}h{scale}.
    // Scan the grid and verify each dark module yields a matching command.
    for (int y = 0; y < size_; y++)
    {
        for (int x = 0; x < size_; x++)
        {
            if (getModule(qrcode_, x, y))
            {
                char cmd[64];
                snprintf(cmd,
                         sizeof(cmd),
                         "M%d,%dh%d",
                         SVG_MARGIN + x * SVG_SCALE,
                         SVG_MARGIN + y * SVG_SCALE + SVG_SCALE / 2,
                         SVG_SCALE);
                EXPECT_NE(std::strstr(buf, cmd), nullptr)
                    << "Dark module at (" << x << ", " << y << ") missing from SVG";
            }
        }
    }
}

TEST_F(SVGTest, LightModulesAreNotRendered)
{
    char buf[QR_SVG_BUF_SIZE];
    qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));

    // Verify that a module known to be light (e.g. the separator ring of the
    // top-left finder pattern) does NOT produce a path command.
    // (1,1) is part of the light separator ring of the top-left finder pattern.
    ASSERT_FALSE(getModule(qrcode_, 1, 1));
    char cmd[64];
    snprintf(cmd,
             sizeof(cmd),
             "M%d,%dh%d",
             SVG_MARGIN + 1 * SVG_SCALE,
             SVG_MARGIN + 1 * SVG_SCALE + SVG_SCALE / 2,
             SVG_SCALE);
    EXPECT_EQ(std::strstr(buf, cmd), nullptr) << "Light module at (1, 1) should not appear in the SVG";
}

// ---------------------------------------------------------------------------
// Return value / length
// ---------------------------------------------------------------------------

TEST_F(SVGTest, ReturnLengthMatchesStringLength)
{
    char buf[QR_SVG_BUF_SIZE];
    int len = qrcode_to_svg(qrcode_, size_, buf, sizeof(buf));
    ASSERT_GT(len, 0);
    EXPECT_EQ(len, (int)strlen(buf)) << "Returned length must match the actual string length";
}

// ---------------------------------------------------------------------------
// Determinism
// ---------------------------------------------------------------------------

TEST_F(SVGTest, SameInputProducesSameOutput)
{
    char buf1[QR_SVG_BUF_SIZE];
    char buf2[QR_SVG_BUF_SIZE];
    int len1 = qrcode_to_svg(qrcode_, size_, buf1, sizeof(buf1));
    int len2 = qrcode_to_svg(qrcode_, size_, buf2, sizeof(buf2));
    ASSERT_EQ(len1, len2);
    EXPECT_EQ(std::memcmp(buf1, buf2, len1), 0) << "Same QR grid must produce identical SVG";
}

TEST_F(SVGTest, DifferentInputProducesDifferentOutput)
{
    uint8_t other[kBufLen];
    ASSERT_TRUE(encodeText("A DIFFERENT PAYLOAD", other));
    int otherSize = other[0];
    ASSERT_GT(otherSize, 0);

    char buf1[QR_SVG_BUF_SIZE];
    char buf2[QR_SVG_BUF_SIZE];
    int len1 = qrcode_to_svg(qrcode_, size_, buf1, sizeof(buf1));
    int len2 = qrcode_to_svg(other, otherSize, buf2, sizeof(buf2));
    ASSERT_GT(len1, 0);
    ASSERT_GT(len2, 0);
    EXPECT_NE(std::memcmp(buf1, buf2, std::min(len1, len2)), 0)
        << "Different QR grids should produce different SVG documents";
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST_F(SVGTest, BufferTooSmallReturnsMinusOne)
{
    // A buffer too small for even the SVG header.
    char tiny[8];
    EXPECT_LT(qrcode_to_svg(qrcode_, size_, tiny, sizeof(tiny)), 0);
}

TEST_F(SVGTest, NullQrCodeFails)
{
    char buf[QR_SVG_BUF_SIZE];
    EXPECT_LT(qrcode_to_svg(nullptr, size_, buf, sizeof(buf)), 0);
}

TEST_F(SVGTest, NullBufferFails)
{
    EXPECT_LT(qrcode_to_svg(qrcode_, size_, nullptr, QR_SVG_BUF_SIZE), 0);
}

TEST_F(SVGTest, ZeroSizeFails)
{
    char buf[QR_SVG_BUF_SIZE];
    EXPECT_LT(qrcode_to_svg(qrcode_, 0, buf, sizeof(buf)), 0);
}

TEST_F(SVGTest, ZeroLengthBufferFails)
{
    char buf[QR_SVG_BUF_SIZE];
    EXPECT_LT(qrcode_to_svg(qrcode_, size_, buf, 0), 0);
}


// ---------------------------------------------------------------------------
// Real cases tests from svg references
// ---------------------------------------------------------------------------
/*
 * Theses tests are generated from an edge case scenario which is: A 24-words seed phrase
 *
 * Here is the seed phrase:
 * "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon
 * abandon abandon abandon abandon abandon abandon abandon abandon abandon art
 *
 * Shares have been generated under the forms of qrcode using Relic Core v1.2.3 on a ESP32-DevkitV1
 * shares are available under reference/svg/seed-phrase-shareX.svg
 */

// ---------------------------------------------------------------------------
// Golden-file comparison tests
// ---------------------------------------------------------------------------
//
// These tests re-encode each base32 seed-phrase share string with the current
// QR code encoder + SVG renderer and compare the resulting document byte-for-byte
// with the golden reference SVG files checked in under tests/reference/svg/.
// The base32 payload form is what the /qr-share.svg endpoint emits.

/**
 * @brief Encode a share string, render it to SVG and compare against its
 *        golden reference file.
 *
 * @param share       The share string (e.g. "1:KXGM6J7...").
 * @param shareIndex  Share number (1..5), used to locate the reference file.
 * @param format      "base32" or "hex" — selects the reference file variant.
 */
void expectMatchesReference(const std::string &share, int shareIndex, const std::string &format)
{
    // Render the current SVG from the share string.
    const std::string generated = renderShareSvg(share);
    ASSERT_FALSE(generated.empty()) << "Failed to render SVG for share " << shareIndex;

    // Read the golden reference file.
    bool ok = false;
    const std::string reference = readFile(referencePath(shareIndex, format), &ok);
    ASSERT_TRUE(ok) << "Reference SVG file not found: " << referencePath(shareIndex, format);

    // The generated document must match the reference byte-for-byte.
    EXPECT_EQ(generated, reference) << "Share " << shareIndex << " SVG does not match the reference";
}

TEST_F(SVGTest, ReferenceTestBase32Share1)
{
    expectMatchesReference(base32_share1, 1, "base32");
}

TEST_F(SVGTest, ReferenceTestBase32Share3)
{
    expectMatchesReference(base32_share3, 3, "base32");
}

TEST_F(SVGTest, ReferenceTestBase32Share4)
{
    expectMatchesReference(base32_share4, 4, "base32");
}

TEST_F(SVGTest, ReferenceTestBase32Share5)
{
    expectMatchesReference(base32_share5, 5, "base32");
}

TEST_F(SVGTest, ReferenceTestHexShare1)
{
    expectMatchesReference(seedphrase_share1, 1, "hex");
}

TEST_F(SVGTest, ReferenceTestHexShare3)
{
    expectMatchesReference(seedphrase_share3, 3, "hex");
}

TEST_F(SVGTest, ReferenceTestHexShare4)
{
    expectMatchesReference(seedphrase_share4, 4, "hex");
}

TEST_F(SVGTest, ReferenceTestHexShare5)
{
    expectMatchesReference(seedphrase_share5, 5, "hex");
}

// ---------------------------------------------------------------------------
// Share 2 has no golden reference file, so only structural checks are done.
// ---------------------------------------------------------------------------

TEST_F(SVGTest, ReferenceTestBase32Share2Structure)
{
    const std::string generated = renderShareSvg(base32_share2);
    ASSERT_FALSE(generated.empty()) << "Failed to render SVG for share 2";

    // With the alphanumeric-only encoder, the base32 share 2 payload is
    // encoded as a version-9 QR code: 53 modules, so the image is 53*8 + 64
    // = 488 px, matching the golden reference SVGs for shares 1, 3, 4 and 5.
    EXPECT_NE(generated.find("viewBox=\"0 0 488 488\""), std::string::npos);
    EXPECT_NE(generated.find("width=\"488\" height=\"488\""), std::string::npos);
    EXPECT_NE(generated.find("<rect width=\"100%\" height=\"100%\" fill=\"white\"/>"), std::string::npos);
    EXPECT_NE(generated.find("<path d=\""), std::string::npos);
    EXPECT_NE(generated.find("stroke=\"black\" stroke-width=\"8\"/>"), std::string::npos);
}

TEST_F(SVGTest, ReferenceTestHexShare2Structure)
{
    const std::string generated = renderShareSvg(seedphrase_share2);
    ASSERT_FALSE(generated.empty()) << "Failed to render SVG for share 2";

    // With the alphanumeric-only encoder, the uppercased hex share 2 payload
    // is encoded as a version-10 QR code: 57 modules, so the image is 57*8 + 64
    // = 520 px, matching the historical golden reference SVGs.
    EXPECT_NE(generated.find("viewBox=\"0 0 520 520\""), std::string::npos);
    EXPECT_NE(generated.find("width=\"520\" height=\"520\""), std::string::npos);
    EXPECT_NE(generated.find("<rect width=\"100%\" height=\"100%\" fill=\"white\"/>"), std::string::npos);
    EXPECT_NE(generated.find("<path d=\""), std::string::npos);
    EXPECT_NE(generated.find("stroke=\"black\" stroke-width=\"8\"/>"), std::string::npos);
}
