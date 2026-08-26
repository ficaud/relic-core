#ifndef ACCESS_POINT_HANDLERS_H
#define ACCESS_POINT_HANDLERS_H

#include "http_router.h"
/// ===========================================================================
// Definitions
// ===========================================================================
#define BIP_MODE_CLASSIC    ("1") // if the bip checkbox is checked without the passphrase
#define BIP_MODE_PASSPHRASE ("2") // if the bip checkbox is checked with the passphrase
// ===========================================================================
// Public function declaration
// ===========================================================================
/**
 * @brief Root handler — landing page with two buttons (split / unsplit).
 */
const char *handler_root(const struct http_request *req);

/**
 * @brief Captive portal handler — used for all probes
 *        (Android, Apple, Windows) and any unknown route.
 *        Returns the HTML page that triggers the popup.
 */
const char *handler_captive_portal(const struct http_request *req);

/**
 * @brief Split handler — serves the Shamir's Secret Encryption form.
 */
const char *handler_split(const struct http_request *req);

/**
 * @brief Unsplit handler — serves the reconstruct-secret demo page.
 */
const char *handler_unsplit(const struct http_request *req);

/**
 * @brief Encrypt handler — receives a message to encrypt.
 *
 * Expects a query parameter "msg" with the message to encrypt.
 * Returns a 200 OK with JSON containing the shares.
 *
 * @param req[in] Request parsed by router_parse().
 *
 * @return Complete HTTP response string (static, do not free).
 */
const char *handler_divide(const struct http_request *req);

/**
 * @brief Reconstruct handler — receives shares and reconstructs the secret.
 *
 * Expects query parameters:
 *   s   — number of shares
 *   d   — comma-separated hex data (e.g. "5a02,77d0,79b7")
 *   x   — comma-separated x values  (e.g. "1,2,3")
 *
 * Returns a 200 OK with the reconstructed secret.
 *
 * @param req[in] Request parsed by router_parse().
 *
 * @return Complete HTTP response string (static, do not free).
 */
const char *handler_reconstruct(const struct http_request *req);

/**
 * @brief QR Code SVG handler — generates a QR Code as an SVG image.
 *
 * Encodes the text from query parameter "text"
 * and returns it as an SVG image with Content-Type: image/svg+xml.
 *
 * @param req[in] Request parsed by router_parse().
 *
 * @return Complete HTTP response string (static, do not free).
 */
const char *handler_qr_svg(const struct http_request *req);

/**
 * @brief Share QR Code SVG handler — generates a QR Code as an SVG image.
 *
 * Encodes the share from query parameter "text" (form "x:hex..."), compresses
 * its payload to base32 ("x:base32...") to shrink the QR code, and returns the
 * result as an SVG image with Content-Type: image/svg+xml.
 *
 * @param req[in] Request parsed by router_parse().
 *
 * @return Complete HTTP response string (static, do not free).
 */
const char *handler_qr_share_svg(const struct http_request *req);

/**
 * @brief QR decode handler — decodes a QR code from an uploaded grayscale image.
 *
 * Expects a POST whose query carries the image dimensions:
 *   w   — image width in pixels (1..QR_DECODE_MAX_DIM)
 *   h   — image height in pixels (1..QR_DECODE_MAX_DIM)
 * and whose body holds exactly w*h raw grayscale bytes
 * (Content-Type: application/octet-stream).
 *
 * The body is streamed directly into quirc's image buffer (no intermediate
 * copy) while it is still arriving on the socket, so it must be invoked from
 * the HTTP thread that owns the client connection.
 *
 * @param req[in]      Request parsed by router_parse().
 * @param raw[in]      Raw received buffer (headers + any already-received body).
 * @param client_fd[in] Client socket, used to read the remainder of the body.
 *
 * @return Complete HTTP response string (static, do not free).
 */
const char *handler_qr_decode_stream(const struct http_request *req, const char *raw, int client_fd);

#endif /* ACCESS_POINT_HANDLERS_H */
