/*
 * QR Code generator library (C) — minimal build tailored for Relic Core.
 *
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/qr-code-generator-library
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 *
 * This is a reduced build of the upstream library tailored to this application.
 * Only text encoding with ECC LOW, mask AUTO and versions 1–40 is supported;
 * see qr_encode.c for the list of removed features.
 */
#ifndef QR_ENCODE_H
#define QR_ENCODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ===========================================================================
 * Macro constants
 * =========================================================================== */
#define qrcodegen_VERSION_MIN 1 /* Minimum version of the QR Code Model 2 standard */
#define qrcodegen_VERSION_MAX 40 /* Maximum version of the QR Code Model 2 standard */

/* Bytes needed to store any QR Code up to and including the given version.
 * Compile-time constant: e.g. uint8_t buffer[qrcodegen_BUFFER_LEN_FOR_VERSION(25)]; */
#define qrcodegen_BUFFER_LEN_FOR_VERSION(n) ((((n) * 4 + 17) * ((n) * 4 + 17) + 7) / 8 + 1)

/* Worst-case number of bytes needed for one QR Code (version 40) = 3918. */
#define qrcodegen_BUFFER_LEN_MAX qrcodegen_BUFFER_LEN_FOR_VERSION(qrcodegen_VERSION_MAX)

/* mode alphanumeric is fixed because it is the only mode supported */
#define MODE_ALPHANUMERIC 0x2
/* ===========================================================================
 * Public functions
 * =========================================================================== */
/*
 * Encodes the given UTF-8 text string into a QR Code, returning true on success.
 *
 * Uses ECC LOW, automatic mask selection and versions 1–40. Returns false if the
 * text is too long to fit in any version.
 *
 * tempBuffer[] and qrcode[] must each have a length of at least
 * qrcodegen_BUFFER_LEN_MAX and must not overlap (aliasing). tempBuffer is used
 * as scratch space and contains no useful data afterwards. On success, qrcode
 * can be passed to qrcodegen_getSize() and read bit-by-bit: byte 0 holds the
 * side length, the module grid is packed from byte 1, row-major, LSB first.
 */
bool qrcodegen_encodeText(const char *text, uint8_t tempBuffer[], uint8_t qrcode[]);

/*
 * Returns the side length of the given QR Code in modules (range [21, 177]).
 */
int qrcodegen_getSize(const uint8_t qrcode[]);

#ifdef __cplusplus
}
#endif

#endif /* QR_ENCODE_H */
