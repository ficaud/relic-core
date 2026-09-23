// SPDX-License-Identifier: Apache-2.0
//
// Static replacement for zxing-cpp's generated Version.h (normally produced by
// its CMake configure_file from Version.h.in). The Relic Core firmware builds
// a QR-only reader subset, so we hard-code those flags here instead of pulling
// in zxing-cpp's full CMake machinery.

#pragma once

#define ZXING_READERS
/* #undef ZXING_WRITERS */

#define ZXING_ENABLE_1D 0
#define ZXING_ENABLE_AZTEC 0
#define ZXING_ENABLE_DATAMATRIX 0
#define ZXING_ENABLE_MAXICODE 0
#define ZXING_ENABLE_PDF417 0
#define ZXING_ENABLE_QRCODE 1

/* #undef ZXING_EXPERIMENTAL_API */
/* #undef ZXING_USE_ZINT */

#define ZXING_VERSION_MAJOR 3
#define ZXING_VERSION_MINOR 1
#define ZXING_VERSION_PATCH 1
#define ZXING_VERSION_SUFFIX ""

#define ZXING_VERSION_STR "3.1.1"
