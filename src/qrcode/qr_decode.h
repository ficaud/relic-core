// SPDX-License-Identifier: GPL-3.0-or-later
//
// QR code decoder interface. Implemented by the quirc backend
// (quirc/qr_decode_quirc.c) and the ZXing-cpp backend
// (zxing/qr_decode_zxing.cpp); the HTTP handler uses this interface unchanged
// regardless of which backend is compiled in.

#ifndef QRCODE_QR_DECODE_H
#define QRCODE_QR_DECODE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ===========================================================================
// Definitions
// ===========================================================================
/** Maximum image dimension (px) accepted for decoding. Kept small so the
 *  image buffer plus the decoder's internal state fit in the device's RAM
 *  budget. The firmware build overrides this with the per-board Kconfig value
 *  RELIC_QR_DECODE_MAX_DIM (224 on ESP32-S3, 640 on the XIAO ESP32S3 with
 *  PSRAM, 192 on the classic ESP32).
 *  WASM demo and native tests keep the 224 default. */
#ifndef QR_DECODE_MAX_DIM
#define QR_DECODE_MAX_DIM (224)
#endif

/** Maximum number of pixels (width * height) for a decodable image. */
#define QR_DECODE_MAX_PIXELS (QR_DECODE_MAX_DIM * QR_DECODE_MAX_DIM)

/** Maximum size of a decoded payload (mirrors QUIRC_MAX_PAYLOAD). */
#define QR_DECODE_MAX_PAYLOAD (2048)

/** Bytes per pixel accepted by qr_decode_begin(). */
#define QR_DECODE_BPP_GRAY (1) /**< Grayscale / luminance */
#define QR_DECODE_BPP_RGBA (4) /**< RGBA (ZXing backend only) */

/** Opaque streaming decoder context. */
struct qr_decode_ctx;

// ===========================================================================
// One-shot decode
// ===========================================================================
/**
 * @brief Decode a QR code from a grayscale image.
 *
 * The caller provides a raw grayscale buffer (one byte per pixel) along
 * with its dimensions. This is exactly the format expected by quirc.
 *
 * @param gray[in]     Grayscale pixel buffer (width * height bytes).
 * @param width[in]    Image width in pixels.
 * @param height[in]   Image height in pixels.
 * @param out[out]     Buffer for the decoded payload (null-terminated).
 * @param out_size[in] Size of the output buffer.
 *
 * @return The payload length on success (>= 0), or a negative value on error.
 */
int qr_decode_gray(const uint8_t *gray, int width, int height, char *out, size_t out_size);

// ===========================================================================
// Streaming decode (allocate once, fill the buffer, then commit)
// ===========================================================================
/**
 * @brief Create a decoder context and allocate the image buffer.
 *
 * Use this when the pixel data arrives as a stream (e.g. an HTTP body) so the
 * image is written directly into the decoder's buffer — avoiding a second full
 * copy of the image in RAM (important on memory-constrained targets).
 *
 * @param width[in]  Image width in pixels.
 * @param height[in] Image height in pixels.
 * @param bpp[in]    Bytes per pixel (QR_DECODE_BPP_GRAY = grayscale/Lum,
 *                   QR_DECODE_BPP_RGBA = RGBA). Only the ZXing-cpp backend
 *                   accepts QR_DECODE_BPP_RGBA; quirc only accepts
 *                   QR_DECODE_BPP_GRAY.
 *
 * @return A decoder context, or NULL on error (invalid dims / bpp / OOM).
 */
struct qr_decode_ctx *qr_decode_begin(int width, int height, int bpp);

/**
 * @brief Return a pointer to the image buffer to fill.
 *
 * The buffer holds exactly width * height * bpp bytes (bpp being the value
 * passed to qr_decode_begin()). The caller writes the pixel data here before
 * calling qr_decode_commit().
 *
 * @param ctx[in] Decoder context from qr_decode_begin().
 *
 * @return Pointer to the image buffer, or NULL on error.
 */
uint8_t *qr_decode_buffer(struct qr_decode_ctx *ctx);

/**
 * @brief Finish decoding the filled buffer.
 *
 * @param ctx[in]      Decoder context from qr_decode_begin().
 * @param out[out]     Buffer for the decoded payload (null-terminated).
 * @param out_size[in] Size of the output buffer.
 * @param grids_out[out] Optional. Receives the number of QR grids identified
 *                      (0 means no QR code was found at all). May be NULL.
 *
 * @return The payload length on success (>= 0), or a negative value on error.
 */
int qr_decode_commit(struct qr_decode_ctx *ctx, char *out, size_t out_size, int *grids_out);

/**
 * @brief Release a decoder context created by qr_decode_begin().
 *
 * @param ctx[in] Decoder context to release (may be NULL).
 */
void qr_decode_destroy(struct qr_decode_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* QRCODE_QR_DECODE_H */

