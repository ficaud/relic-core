// SPDX-License-Identifier: GPL-3.0-or-later
//
// QR code decoder wrapper around ZXing-cpp (QR-only build).
//
// Implements the same `qr_decode_*` interface as the quirc backend
// (qr_decode.c) so the HTTP handler can switch backends at build time without
// change. The input is a raw grayscale (Lum) buffer, which ZXing accepts
// directly (it does the RGB->luma conversion internally when given color).

#include "qr_decode.h"

#include "ReadBarcode.h"
#include "BarcodeFormat.h"

#include <cstring>
#include <new>

extern "C"
{

// ===========================================================================
// Streaming decode (allocate once, fill the buffer, then commit)
// ===========================================================================
struct qr_decode_ctx
{
    uint8_t *buffer;
    int width;
    int height;
};

struct qr_decode_ctx *qr_decode_begin(int width, int height)
{
    if (width <= 0 || height <= 0 || width > QR_DECODE_MAX_DIM || height > QR_DECODE_MAX_DIM)
    {
        return nullptr;
    }

    qr_decode_ctx *ctx = new (std::nothrow) qr_decode_ctx;
    if (ctx == nullptr)
    {
        return nullptr;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->buffer = new (std::nothrow) uint8_t[(size_t)width * (size_t)height];
    if (ctx->buffer == nullptr)
    {
        delete ctx;
        return nullptr;
    }

    return ctx;
}

uint8_t *qr_decode_buffer(struct qr_decode_ctx *ctx)
{
    return ctx == nullptr ? nullptr : ctx->buffer;
}

int qr_decode_commit(struct qr_decode_ctx *ctx, char *out, size_t out_size, int *grids_out)
{
    if (grids_out != nullptr)
    {
        *grids_out = 0;
    }

    if (ctx == nullptr || ctx->buffer == nullptr || out == nullptr || out_size == 0)
    {
        return -1;
    }

    int len = qr_decode_gray(ctx->buffer, ctx->width, ctx->height, out, out_size);
    if (len < 0)
    {
        return -1;
    }

    if (grids_out != nullptr)
    {
        *grids_out = 1;
    }
    return len;
}

void qr_decode_destroy(struct qr_decode_ctx *ctx)
{
    if (ctx == nullptr)
    {
        return;
    }

    delete[] ctx->buffer;
    delete ctx;
}

// ===========================================================================
// One-shot decode
// ===========================================================================
int qr_decode_gray(const uint8_t *gray, int width, int height, char *out, size_t out_size)
{
    if (gray == nullptr || out == nullptr || out_size == 0 || width <= 0 || height <= 0)
    {
        return -1;
    }

    if (width > QR_DECODE_MAX_DIM || height > QR_DECODE_MAX_DIM)
    {
        return -1;
    }

    try
    {
        ZXing::ImageView iv{gray, width, height, ZXing::ImageFormat::Lum};
        ZXing::ReaderOptions opts;
        opts.setFormats(ZXing::BarcodeFormat::QRCode);

        ZXing::Barcode barcode = ZXing::ReadBarcode(iv, opts);
        if (!barcode.isValid())
        {
            return -1;
        }

        const std::vector<uint8_t> &bytes = barcode.bytes();
        if (bytes.empty() || bytes.size() >= out_size)
        {
            return -1;
        }

        memcpy(out, bytes.data(), bytes.size());
        out[bytes.size()] = '\0';
        return (int)bytes.size();
    }
    catch (...)
    {
        return -1;
    }
}

} // extern "C"
