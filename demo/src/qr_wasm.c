/**
 * @file qr_wasm.c
 *
 * @brief WASM entry point — exposes QR Code SVG generation to JavaScript.
 *
 * @author Julien F.
 * @date 2026-07-31
 *
 * @details Uses the reference qr_encode.c from relic-core directly.
 *          Compile with Emscripten alongside qr_encode.c.
 */

#include "qr_encode.h"
#include "qrcode_to_svg.h"

#include <stdbool.h>
#include <stdlib.h>

/* ── Public WASM API ── */

/**
 * @brief Generate a QR Code SVG for the given text.
 *
 * @param text  Null-terminated UTF-8 string to encode.
 * @return      Malloc'd SVG string (caller must free with wasm_qr_free()),
 *              or NULL on error.
 */
char *wasm_qr_generate(const char *text)
{
    char *ret = NULL;
    uint8_t qr_temp[qrcodegen_BUFFER_LEN_MAX];
    uint8_t qr_code[qrcodegen_BUFFER_LEN_MAX];

    /* Generate the QR Code (ECC LOW, mask AUTO, versions 1-40) */
    bool ok = qrcodegen_encodeText(text, qr_temp, qr_code);
    if (!ok)
    {
        goto exit;
    }

    /* Allocate output buffer */
    char *svg = malloc(QR_SVG_BUF_SIZE);
    if (!svg)
    {
        goto exit;
    }

    /* Render the QR code as a pure SVG document (reuses src/svg/qrcode_to_svg) */
    if (qrcode_to_svg(qr_code, qrcodegen_getSize(qr_code), svg, QR_SVG_BUF_SIZE) < 0)
    {
        free(svg);
        goto exit;
    }

    // no error, return the svg
    ret = svg;

exit:
    return ret;
}

/**
 * @brief Free a string returned by wasm_qr_generate().
 *
 * @param ptr  Pointer to free.
 */
void wasm_qr_free(char *ptr)
{
    free(ptr);
}
