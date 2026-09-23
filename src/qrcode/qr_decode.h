// SPDX-License-Identifier: GPL-3.0-or-later
//
// QR code decoder interface. Implemented by the quirc backend (qr_decode.c)
// and the ZXing-cpp backend (qr_decode_zxing.cpp); the HTTP handler uses this
// interface unchanged regardless of which backend is compiled in.

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
/** Maximum supported image dimension (pixels) per side. */
/** Maximum image dimension (px) accepted for decoding. Kept small so the
 *  image buffer plus quirc's internal state fit in the device's RAM budget.
 *  The firmware build overrides this with the per-board Kconfig value
 *  RELIC_QR_DECODE_MAX_DIM (224 on ESP32-S3, 192 on the classic ESP32).
 *  WASM demo and native tests keep the 224 default. */
#ifndef QR_DECODE_MAX_DIM
#define QR_DECODE_MAX_DIM (224)
#endif

/** Maximum number of pixels (width * height) for a decodable image. */
#define QR_DECODE_MAX_PIXELS (QR_DECODE_MAX_DIM * QR_DECODE_MAX_DIM)

/** Maximum size of a decoded payload (mirrors QUIRC_MAX_PAYLOAD). */
#define QR_DECODE_MAX_PAYLOAD (2048)

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
 * @brief Create a decoder context and allocate the grayscale image buffer.
 *
 * Use this when the pixel data arrives as a stream (e.g. an HTTP body) so the
 * image is written directly into quirc's buffer — avoiding a second full copy
 * of the image in RAM (important on memory-constrained targets).
 *
 * @param width[in]  Image width in pixels.
 * @param height[in] Image height in pixels.
 *
 * @return A decoder context, or NULL on error (invalid dims / out of memory).
 */
struct qr_decode_ctx *qr_decode_begin(int width, int height);

/**
 * @brief Return a pointer to the grayscale buffer to fill.
 *
 * The buffer holds exactly width * height bytes. The caller writes the pixel
 * data here before calling qr_decode_commit().
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
 * @param grids_out[out] Optional. Receives the number of QR grids quirc
 *                      identified (0 means no QR code was found at all).
 *                      May be NULL.
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

