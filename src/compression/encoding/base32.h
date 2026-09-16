/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file base32.h
 *
 * @brief Base32 codec (RFC 4648) for compressing share payloads.
 *
 * @author Julien F.
 * @date 2026-08-20
 *
 * @details Encodes arbitrary bytes as base32 using the RFC 4648 alphabet
 *          (A-Z 2-7, uppercase) without '=' padding. The shorter, purely
 *          alphanumeric output produces a smaller QR code than the hex
 *          representation it replaces.
 *
 *          Decoding is case-insensitive to tolerate manual transcription.
 */

#ifndef BASE32_H
#define BASE32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// ===========================================================================
// Public function declaration
// ===========================================================================
/**
 * @brief Compute the required output buffer size for base32 encoding.
 *
 * The result includes the terminating NUL character, so it is suitable as a
 * buffer size for base32_encode().
 *
 * @param[in] in_len  Number of input bytes.
 *
 * @return Required output size in bytes (>= 1; empty input yields 1 for the NUL).
 */
size_t base32_encoded_len(size_t in_len);

/**
 * @brief Encode bytes as unpadded base32 (RFC 4648 alphabet, uppercase).
 *
 * @param[in]  in        Input bytes.
 * @param[in]  in_len    Number of input bytes.
 * @param[out] out       Output buffer, NUL-terminated.
 * @param[in]  out_size  Size of the output buffer (use base32_encoded_len()).
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int base32_encode(const uint8_t *in, size_t in_len, char *out, size_t out_size);

/**
 * @brief Decode an unpadded base32 string back into bytes.
 *
 * Decoding is case-insensitive. Any character outside the RFC 4648 alphabet
 * (A-Z 2-7, case-insensitive) is rejected.
 *
 * @param[in]  in        Input base32 string (may be NUL-terminated).
 * @param[in]  in_len    Number of input characters (excluding any NUL).
 * @param[out] out       Output buffer for decoded bytes.
 * @param[in]  out_size  Size of the output buffer.
 * @param[out] out_len   Receives the number of decoded bytes written.
 *
 * @return 0 on success, negative on error (-EINVAL, -ENOSPC).
 */
int base32_decode(const char *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* BASE32_H */
