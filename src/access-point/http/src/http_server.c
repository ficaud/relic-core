/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * HTTP Server — serves the captive portal pages.
 * Delegates routing to the router module and content to the handlers module.
 */

#include "http_server.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "http_handlers.h"
#include "http_router.h"
#include "http_types.h"
#include "page_captive.h"

#include <errno.h>
#include <string.h>

// ===========================================================================
// Zephyr logging module registration
// ===========================================================================
LOG_MODULE_REGISTER(http, LOG_LEVEL_INF);

// ===========================================================================
// Structure and variables definition
// ===========================================================================
K_THREAD_STACK_DEFINE(http_stack, HTTP_STACK_SIZE);
static struct k_thread http_thread;

K_THREAD_STACK_DEFINE(https_stack, TLS_STACK_SIZE);
static struct k_thread https_thread;

// ===========================================================================
// Static function declarations
// ===========================================================================
/**
 * @brief HTTP listener thread function.
 *
 * Listens for HTTP requests on TCP port 80 and serves the captive portal pages.
 *
 * @param arg1 Unused.
 * @param arg2 Unused.
 * @param arg3 Unused.
 *
 * @return void
 */
static void http_thread_fn(void *arg1, void *arg2, void *arg3);

/**
 * @brief TLS listener thread function.
 *
 * Listens for HTTPS requests on TCP port 443 and serves the captive portal pages.
 * This is primarily to catch Android HTTPS probes and trigger the captive portal popup.
 *
 * @param arg1 Unused.
 * @param arg2 Unused.
 * @param arg3 Unused.
 *
 * @return void
 */
static void tls_thread_fn(void *arg1, void *arg2, void *arg3);
// ===========================================================================
// Public function definition
// ===========================================================================
int http_server_start(void)
{
    k_thread_create(
        &http_thread, http_stack, HTTP_STACK_SIZE, http_thread_fn, NULL, NULL, NULL, HTTP_PRIORITY, 0, K_NO_WAIT);

    k_thread_create(
        &https_thread, https_stack, TLS_STACK_SIZE, tls_thread_fn, NULL, NULL, NULL, HTTP_PRIORITY, 0, K_NO_WAIT);

    return 0;
}

int http_server_stop(void)
{
    return 0;
}

// ===========================================================================
// Static function definition
// ===========================================================================
static void http_thread_fn(void *arg1, void *arg2, void *arg3)
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
        LOG_ERR("socket failed (%d)", errno);
        return;
    }

    zsock_setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(HTTP_PORT);

    if (zsock_bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG_ERR("bind failed (%d)", errno);
        zsock_close(server_fd);
        return;
    }

    if (zsock_listen(server_fd, HTTP_BACKLOG) < 0)
    {
        LOG_ERR("listen failed (%d)", errno);
        zsock_close(server_fd);
        return;
    }

    LOG_INF("HTTP server ready on port %d", HTTP_PORT);

    while (1)
    {
        char rx_buf[HTTP_RX_BUF_SIZE];
        struct http_request req;
        const char *response;

        client_fd = zsock_accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            LOG_ERR("accept failed (%d)", errno);
            continue;
        }

        /* Read until \r\n\r\n (end of HTTP headers) */
        int total = 0;
        while (total < (int)sizeof(rx_buf) - 1)
        {
            int n = zsock_recv(client_fd, rx_buf + total, sizeof(rx_buf) - 1 - total, 0);
            if (n <= 0)
                break;
            total += n;
            rx_buf[total] = '\0';
            if (strstr(rx_buf, "\r\n\r\n"))
                break;
        }

        if (total > 0)
        {
            /* Parse + dispatch */
            if (router_parse(&req, rx_buf, total) == 0)
            {
                if (strcmp(req.path, "/qr_decode") == 0)
                {
#if defined(CONFIG_RELIC_QR_DECODE_SERVER)
                    /* Streaming QR decode: the grayscale body is read directly
                     * into quirc's image buffer while it is still arriving. */
                    LOG_INF("POST /qr_decode -> streaming decode");
                    response = handler_qr_decode_stream(&req, rx_buf, client_fd);
#else
                    /* On-device decoding is not compiled in (e.g. the classic
                     * ESP32 lacks the RAM); the web page decodes locally. */
                    response = http_responses_list[HTTP_RESPONSE_NOT_FOUND];
#endif
                }
                else
                {
                    response = router_dispatch(&req);
                }
            }
            else
            {
                response = http_responses_list[HTTP_RESPONSE_BAD_REQUEST];
            }
        }
        else
        {
            response = http_responses_list[HTTP_RESPONSE_BAD_REQUEST];
        }

        /* Send full response (loop: zsock_send may not send all at once) */
        const char *ptr = response;
        int remaining = strlen(response);
        while (remaining > 0)
        {
            int sent = zsock_send(client_fd, ptr, remaining, 0);
            if (sent < 0)
            {
                break;
            }
            ptr += sent;
            remaining -= sent;
        }
        zsock_close(client_fd);
    }
}

static void tls_thread_fn(void *arg1, void *arg2, void *arg3)
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
        LOG_ERR("tls socket failed (%d)", errno);
        return;
    }

    zsock_setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(443);

    if (zsock_bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG_ERR("tls bind failed (%d)", errno);
        zsock_close(server_fd);
        return;
    }

    if (zsock_listen(server_fd, HTTP_BACKLOG) < 0)
    {
        LOG_ERR("tls listen failed (%d)", errno);
        zsock_close(server_fd);
        return;
    }

    LOG_INF("HTTPS server ready on port 443");

    while (1)
    {
        client_fd = zsock_accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            LOG_ERR("tls accept failed (%d)", errno);
            continue;
        }

        /* Discard TLS ClientHello, reply with full captive portal page */
        char discard[256];
        zsock_recv(client_fd, discard, sizeof(discard), 0);

        const char *ptr = PAGE_CAPTIVE;
        int remaining = strlen(PAGE_CAPTIVE);
        while (remaining > 0)
        {
            int sent = zsock_send(client_fd, ptr, remaining, 0);
            if (sent < 0)
            {
                break;
            }
            ptr += sent;
            remaining -= sent;
        }
        zsock_close(client_fd);
    }
}
