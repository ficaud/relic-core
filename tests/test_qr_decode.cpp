// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for the QR code decoder (qr_decode.c + quirc).
//
// These tests mirror the embedded device path (handler_qr_decode_stream in
// src/access-point/http/src/http_handlers.c):
//
//   1. qr_decode_begin(w, h)      with w, h <= QR_DECODE_MAX_DIM
//   2. qr_decode_buffer(ctx)      -> fill with raw grayscale (1 byte/pixel)
//   3. qr_decode_commit(ctx, out, QR_DECODE_MAX_PAYLOAD, &grids)
//   4. qr_decode_destroy(ctx)
//
// The grayscale images are rasterized from the golden share QR grids (the
// exact "x:base32..." / "x:hex..." payloads the device is expected to scan),
// at 3 px/module with a 4-module quiet zone, so the image side length stays
// within QR_DECODE_MAX_DIM (224). A one-shot qr_decode_gray() (the WASM demo
// path) is also covered with a couple of cases.

#include "qr_decode.h"

#include "golden_shares.h"
#include "golden_base32_qr_grids.h"
#include "golden_hex_qr_grids.h"

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

// Rasterization parameters matching a well-cropped camera frame: 3 pixels per
// module and a 4-module (spec minimum) quiet zone. For the largest golden grid
// (version 10, 57 modules) this yields (57 + 8) * 3 = 195 px <= 224.
constexpr int kScale = 3;
constexpr int kQuiet = 4;

/**
 * @brief Rasterize a packed QR grid into a grayscale image.
 *
 * The grid layout matches getModule() in test_qrcode.cpp: grid[0] holds the
 * side length, the module grid is packed from byte 1, row-major, LSB first.
 * Dark modules become 0x00 and light modules 0xFF, exactly the grayscale
 * format quirc expects.
 *
 * @param grid   Packed QR grid.
 * @param scale  Pixels per module.
 * @param quiet  Quiet-zone modules around the code.
 * @param img    [out] Grayscale buffer (dim * dim bytes).
 * @param dim    [out] Image side length in pixels.
 */
void renderToGray(const uint8_t *grid, int scale, int quiet, std::vector<uint8_t> &img, int &dim)
{
    const int size = grid[0];
    dim = (size + 2 * quiet) * scale;

    img.assign(static_cast<size_t>(dim) * dim, 0xFF);

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            const int index = y * size + x;
            const bool dark = ((grid[(index >> 3) + 1] >> (index & 7)) & 1) != 0;
            if (!dark)
            {
                continue;
            }

            const int py = (y + quiet) * scale;
            const int px = (x + quiet) * scale;
            for (int dy = 0; dy < scale; dy++)
            {
                for (int dx = 0; dx < scale; dx++)
                {
                    img[static_cast<size_t>(py + dy) * dim + (px + dx)] = 0x00;
                }
            }
        }
    }
}

/**
 * @brief Decode a golden QR grid through the same streaming API as the device.
 *
 * @param grid     Packed QR grid.
 * @param payload  [out] Decoded payload (on success).
 * @param grids    [out] Number of QR grids quirc identified.
 * @return The payload length on success, or a negative value on error.
 */
int deviceDecode(const uint8_t *grid, std::string &payload, int &grids)
{
    std::vector<uint8_t> gray;
    int dim = 0;
    renderToGray(grid, kScale, kQuiet, gray, dim);

    struct qr_decode_ctx *ctx = qr_decode_begin(dim, dim);
    if (ctx == nullptr)
    {
        return -1;
    }

    uint8_t *buf = qr_decode_buffer(ctx);
    if (buf == nullptr)
    {
        qr_decode_destroy(ctx);
        return -1;
    }

    memcpy(buf, gray.data(), gray.size());

    char out[QR_DECODE_MAX_PAYLOAD];
    int ret = qr_decode_commit(ctx, out, sizeof(out), &grids);
    qr_decode_destroy(ctx);

    if (ret >= 0)
    {
        payload.assign(out, static_cast<size_t>(ret));
    }
    return ret;
}

} // namespace

// ===========================================================================
// Test fixture
// ===========================================================================

class QRDecodeTest : public ::testing::Test
{
};

// ===========================================================================
// Device path (streaming API) — golden share payloads
// ===========================================================================

// Each base32 golden grid (version 9, 53x53) must decode back to its original
// "x:base32..." share string — the exact payload the device scans.
TEST_F(QRDecodeTest, DevicePathDecodesBase32Shares)
{
    const uint8_t *const grids[] = {
        base32_share1_qr,
        base32_share2_qr,
        base32_share3_qr,
        base32_share4_qr,
        base32_share5_qr,
    };
    const char *const expected[] = {
        base32_share1,
        base32_share2,
        base32_share3,
        base32_share4,
        base32_share5,
    };

    for (int i = 0; i < 5; i++)
    {
        std::string payload;
        int gridCount = -1;
        const int ret = deviceDecode(grids[i], payload, gridCount);

        ASSERT_GE(ret, 0) << "base32 share " << (i + 1) << " should decode";
        EXPECT_EQ(gridCount, 1) << "base32 share " << (i + 1) << " should contain one QR grid";
        EXPECT_EQ(payload, expected[i]) << "base32 share " << (i + 1) << " payload mismatch";
    }
}

// Each hex golden grid (version 10, 57x57) must decode back to its original
// uppercase "x:hex..." share string.
TEST_F(QRDecodeTest, DevicePathDecodesHexShares)
{
    const uint8_t *const grids[] = {
        seedphrase_share1_qr,
        seedphrase_share2_qr,
        seedphrase_share3_qr,
        seedphrase_share4_qr,
        seedphrase_share5_qr,
    };
    const char *const expected[] = {
        seedphrase_share1,
        seedphrase_share2,
        seedphrase_share3,
        seedphrase_share4,
        seedphrase_share5,
    };

    for (int i = 0; i < 5; i++)
    {
        std::string payload;
        int gridCount = -1;
        const int ret = deviceDecode(grids[i], payload, gridCount);

        ASSERT_GE(ret, 0) << "hex share " << (i + 1) << " should decode";
        EXPECT_EQ(gridCount, 1) << "hex share " << (i + 1) << " should contain one QR grid";
        EXPECT_EQ(payload, expected[i]) << "hex share " << (i + 1) << " payload mismatch";
    }
}

// ===========================================================================
// Device path (streaming API) — no QR code found
// ===========================================================================

// A blank image (the common "camera saw no QR code" case) must yield a
// negative payload length and report zero grids.
TEST_F(QRDecodeTest, EmptyImageFindsNoGrid)
{
    const int dim = QR_DECODE_MAX_DIM;

    struct qr_decode_ctx *ctx = qr_decode_begin(dim, dim);
    ASSERT_NE(ctx, nullptr);

    uint8_t *buf = qr_decode_buffer(ctx);
    ASSERT_NE(buf, nullptr);
    memset(buf, 0xFF, static_cast<size_t>(dim) * dim);

    char out[QR_DECODE_MAX_PAYLOAD];
    int grids = -1;
    const int ret = qr_decode_commit(ctx, out, sizeof(out), &grids);
    qr_decode_destroy(ctx);

    EXPECT_LT(ret, 0);
    EXPECT_EQ(grids, 0);
}

// ===========================================================================
// Device path (streaming API) — error handling
// ===========================================================================

// qr_decode_begin() rejects non-positive or oversized dimensions, mirroring
// the w/h validation in handler_qr_decode_stream().
TEST_F(QRDecodeTest, BeginRejectsInvalidDimensions)
{
    EXPECT_EQ(qr_decode_begin(0, 10), nullptr);
    EXPECT_EQ(qr_decode_begin(10, 0), nullptr);
    EXPECT_EQ(qr_decode_begin(-1, 10), nullptr);
    EXPECT_EQ(qr_decode_begin(10, -1), nullptr);
    EXPECT_EQ(qr_decode_begin(QR_DECODE_MAX_DIM + 1, 10), nullptr);
    EXPECT_EQ(qr_decode_begin(10, QR_DECODE_MAX_DIM + 1), nullptr);
}

TEST_F(QRDecodeTest, BufferNullCtxReturnsNull)
{
    EXPECT_EQ(qr_decode_buffer(nullptr), nullptr);
}

TEST_F(QRDecodeTest, CommitNullCtxFails)
{
    char out[16];
    int grids = -1;
    EXPECT_LT(qr_decode_commit(nullptr, out, sizeof(out), &grids), 0);
    EXPECT_EQ(grids, 0);
}

TEST_F(QRDecodeTest, CommitZeroOutSizeFails)
{
    struct qr_decode_ctx *ctx = qr_decode_begin(16, 16);
    ASSERT_NE(ctx, nullptr);

    char out[16];
    int grids = -1;
    EXPECT_LT(qr_decode_commit(ctx, out, 0, &grids), 0);
    EXPECT_EQ(grids, 0);

    qr_decode_destroy(ctx);
}

TEST_F(QRDecodeTest, DestroyNullIsNoop)
{
    qr_decode_destroy(nullptr);
}

// ===========================================================================
// One-shot API (WASM demo path)
// ===========================================================================

TEST_F(QRDecodeTest, OneShotDecodesBase32Share)
{
    std::vector<uint8_t> gray;
    int dim = 0;
    renderToGray(base32_share1_qr, kScale, kQuiet, gray, dim);

    char out[QR_DECODE_MAX_PAYLOAD];
    const int ret = qr_decode_gray(gray.data(), dim, dim, out, sizeof(out));

    ASSERT_GE(ret, 0);
    EXPECT_EQ(std::string(out, static_cast<size_t>(ret)), base32_share1);
}

TEST_F(QRDecodeTest, OneShotRejectsInvalidArgs)
{
    static const uint8_t dummy[] = {0};
    char out[16];

    EXPECT_LT(qr_decode_gray(nullptr, 10, 10, out, sizeof(out)), 0);
    EXPECT_LT(qr_decode_gray(dummy, 10, 10, nullptr, sizeof(out)), 0);
    EXPECT_LT(qr_decode_gray(dummy, 10, 10, out, 0), 0);
    EXPECT_LT(qr_decode_gray(dummy, 0, 10, out, sizeof(out)), 0);
    EXPECT_LT(qr_decode_gray(dummy, 10, 0, out, sizeof(out)), 0);
    EXPECT_LT(qr_decode_gray(dummy, QR_DECODE_MAX_DIM + 1, 10, out, sizeof(out)), 0);
    EXPECT_LT(qr_decode_gray(dummy, 10, QR_DECODE_MAX_DIM + 1, out, sizeof(out)), 0);
}
