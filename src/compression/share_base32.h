/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file share_base32.h
 *
 * @brief Share text conversion between the "x:hex..." and "x:base32..." forms.
 *
 * @author Julien F.
 * @date 2026-08-24
 *
 * @details A Shamir share is carried around the device as "x:hex..." (the
 *          canonical, copy/paste and reconstruct wire format). To shrink the
 *          QR code, the QR payload uses a base32-compressed form "x:base32...".
 *
 *          This module converts between the two textual representations, using
 *          the base32 codec (RFC 4648) from base32.h.
 */

#ifndef SHARE_BASE32_H
#define SHARE_BASE32_H

#include <stddef.h>
#include <stdint.h>

#include "sss.h"

#ifdef __cplusplus
extern "C"
{
#endif

// ===========================================================================
// Definitions
// ===========================================================================
/* Worst-case base32 size of a single share payload (ceil(256*8/5) = 410 chars)
 * plus the terminating NUL. */
#define SHARE_B32_BUF_SIZE ((SSS_MAX_SECRET_LEN * 8 + 4) / 5 + 1)

// ===========================================================================
// Public function declaration
// ===========================================================================
/**
 * @brief Convert a share in "x:hex..." form to "x:base32..." (QR payload).
 *
 * @param text[in]      Input share text ("x:hex..."), NUL-terminated.
 * @param out[out]      Output buffer ("x:base32...", NUL-terminated).
 * @param out_size[in]  Size of the output buffer.
 *
 * @return 0 on success, -1 on error.
 */
int share_to_base32(const char *text, char *out, size_t out_size);

/**
 * @brief Convert a share in "x:base32..." form back to "x:hex...".
 *
 * @param text[in]      Input share text ("x:base32...").
 * @param text_len[in]  Number of input characters (excluding any NUL).
 * @param out[out]      Output buffer ("x:hex...", NUL-terminated).
 * @param out_size[in]  Size of the output buffer.
 *
 * @return 0 on success, -1 on error.
 */
int share_from_base32(const char *text, size_t text_len, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* SHARE_BASE32_H */
