/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * @file wifi_mgr.c
 *
 * @brief Wi-Fi Access Point management and event handling.
 *
 * @author Julien F.
 * @date 2026-07-12
 *
 * @details This module handles the initialization and lifecycle of the Wi-Fi
 *          Access Point (AP) on the ESP32-S3. It registers a network management
 *          event callback to monitor Wi-Fi state transitions (station connect/
 *          disconnect, AP enable/disable) and coordinates AP startup using a
 *          semaphore-based synchronization pattern. The AP configuration settings
 *          are defined in prj.conf.
 */

#include "wifi_mgr.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

#include "dhcp.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// Zephyr logging module registration
// ===========================================================================
LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_INF);
// ===========================================================================
// Structure and variables definition
// ===========================================================================
//  Assertions of configuration (in prj.conf)
BUILD_ASSERT(sizeof(CONFIG_WIFI_SAMPLE_AP_SSID) > 1,
             "CONFIG_WIFI_SAMPLE_AP_SSID is empty. Please set it in conf file.");

// Semaphore to signal when AP mode is ready
static struct k_sem ap_ready_sem;
// ===========================================================================
// Static function declarations
// ===========================================================================
/**
 * @brief Callback function to handle Wi-Fi events.
 *
 * @param cb[in] : Pointer to the net_mgmt_event_callback structure.
 * @param mgmt_event[in] : The management event that occurred.
 * @param iface[in] : Pointer to the network interface associated with the event.
 *
 * @return void
 */
static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface);

/**
 * @brief Enable Wi-Fi Access Point (AP) mode.
 *
 * @param ap_iface[in] : Pointer to the network interface for the AP.
 *
 * @return 0 on success, negative error code on failure.
 */
static int enable_ap_mode(struct net_if **ap_iface);

// ===========================================================================
// Public function definition
// ===========================================================================

int wifi_mgr_init(void)
{
    int ret = -1; // error by default
    static struct net_if *ap_iface;
    static struct net_mgmt_event_callback cb;
    // Delay to allow the Wi-Fi driver to initialize
    k_sleep(K_SECONDS(5));

    // Semaphore initialization for AP ready signal
    ret = k_sem_init(&ap_ready_sem, 0, 1);
    if (ret)
    {
        LOG_ERR("Failed to initialize semaphore, err: %d", ret);
        goto exit;
    }

    // Register Wi-Fi event callback
    net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
    net_mgmt_add_event_callback(&cb);

    // Get wifi AP interface
    ap_iface = net_if_get_wifi_sap();

    if (!ap_iface)
    {
        LOG_ERR("Failed to get Wi-Fi AP interface");
        goto exit;
    }

    // Enable AP mode
    ret = enable_ap_mode(&ap_iface);

    if (ret)
    {
        LOG_ERR("enable_ap_mode failed (%d)", ret);
        goto exit;
    }

    // Wait for AP ready signal to be given by the event handler
    ret = k_sem_take(&ap_ready_sem, K_FOREVER);

exit:
    return (ret);
}
// ===========================================================================
// Static function definition
// ===========================================================================
static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event)
    {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            LOG_INF("Connected to %s", CONFIG_WIFI_SAMPLE_SSID);
            break;

        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            LOG_INF("Disconnected from %s", CONFIG_WIFI_SAMPLE_SSID);
            break;

        case NET_EVENT_WIFI_AP_ENABLE_RESULT:
            struct wifi_status *status = (struct wifi_status *)cb->info;
            if (status->status)
            {
                LOG_ERR("AP enable failed (%d)", status->status);
            }
            else
            {
                LOG_INF("AP Mode enabled");
                k_sem_give(&ap_ready_sem);
            }
            break;

        case NET_EVENT_WIFI_AP_DISABLE_RESULT:
            LOG_INF("AP Mode disabled");
            break;

        case NET_EVENT_WIFI_AP_STA_CONNECTED:
        {
            struct wifi_ap_sta_info *sta = (struct wifi_ap_sta_info *)cb->info;
            LOG_INF("station: " MACSTR " joined",
                    sta->mac[0],
                    sta->mac[1],
                    sta->mac[2],
                    sta->mac[3],
                    sta->mac[4],
                    sta->mac[5]);
            break;
        }

        case NET_EVENT_WIFI_AP_STA_DISCONNECTED:
        {
            struct wifi_ap_sta_info *sta = (struct wifi_ap_sta_info *)cb->info;
            LOG_INF("station: " MACSTR " left",
                    sta->mac[0],
                    sta->mac[1],
                    sta->mac[2],
                    sta->mac[3],
                    sta->mac[4],
                    sta->mac[5]);
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Derive the Wi-Fi AP password from the device MAC address.
 *
 * @details Computes an FNV-1a 64-bit hash of the given MAC bytes and converts
 *          it to a fixed-length (WIFI_PSK_LEN), upper-case base-36 password.
 *          This is fully deterministic: the same MAC always yields the same
 *          password, and it requires no external crypto/hash library. The demo
 *          flasher (demo/flash.html) mirrors this exact algorithm in
 *          JavaScript so the QR code shown after flashing matches the password
 *          actually used by the freshly-flashed firmware.
 *
 * @param mac[in]      : Pointer to the MAC bytes.
 * @param mac_len[in]  : Number of MAC bytes.
 * @param out[out]     : Destination buffer, must hold WIFI_PSK_LEN + 1 bytes.
 *
 * @return void (the derived password is written to @p out, always NUL-terminated).
 */
static void derive_wifi_psk(const uint8_t *mac, size_t mac_len, char *out)
{
    /* FNV-1a 64-bit hash of the MAC bytes */
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < mac_len; i++)
    {
        hash ^= mac[i];
        hash *= 0x100000001b3ULL;
    }

    /* Upper-case base-36 alphabet (0-9A-Z) */
    static const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    /* Convert the hash to a fixed-length password, least-significant digit first.
     * This order must match the JavaScript implementation in demo/flash.html. */
    uint64_t h = hash;
    for (int i = 0; i < WIFI_PSK_LEN; i++)
    {
        out[i] = alphabet[h % 36];
        h /= 36;
    }
    out[WIFI_PSK_LEN] = '\0';
}

static int enable_ap_mode(struct net_if **ap_iface)
{
    struct wifi_connect_req_params ap_config;
    /* SSID buffer: base SSID + "-" + 4 hex digits of MAC = max 32 chars */
    // Relic-XXXX (10 so we define it to 16 to be safe)
    char ssid_buf[16];
    // Get the link-layer address of the AP interface to generate a unique SSID
    struct net_linkaddr *link_addr = net_if_get_link_addr(*ap_iface);

    if (!ap_iface || !*ap_iface)
    {
        LOG_ERR("AP interface not initialized");
        return -EIO;
    }

    /* Build dynamic SSID by appending the AP's own MAC address */
    if (!link_addr || link_addr->len < 6)
    {
        LOG_ERR("Failed to get link address for AP interface");
        return -EIO;
    }

    // Format the SSID with the base SSID and the last 4 hex digits of the MAC
    int written = snprintf(
        ssid_buf, sizeof(ssid_buf), "%s-%02X%02X", CONFIG_WIFI_SAMPLE_AP_SSID, link_addr->addr[4], link_addr->addr[5]);

    if (written < 0 || written >= (int)sizeof(ssid_buf))
    {
        LOG_ERR("SSID too long (%d chars, max %zu)", written, sizeof(ssid_buf) - 1);
        return -ENOBUFS;
    }

    LOG_INF("AP SSID: %s", ssid_buf);

    LOG_INF("Turning on AP Mode");

    /* Derive a deterministic WPA2 password from the device MAC address, so that
     * every device broadcasts a unique access point with a unique password. */
    char psk_buf[WIFI_PSK_LEN + 1];
    derive_wifi_psk(link_addr->addr, link_addr->len, psk_buf);

    LOG_INF("AP PSK: %s", psk_buf);

    ap_config.ssid = (const uint8_t *)ssid_buf;
    ap_config.ssid_length = (uint8_t)written;
    ap_config.psk = (const uint8_t *)psk_buf;
    ap_config.psk_length = WIFI_PSK_LEN;
    ap_config.channel = WIFI_CHANNEL_ANY;
    ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;
    ap_config.security = WIFI_SECURITY_TYPE_PSK;

    int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, *ap_iface, &ap_config, sizeof(struct wifi_connect_req_params));
    if (ret)
    {
        LOG_ERR("NET_REQUEST_WIFI_AP_ENABLE failed, err: %d", ret);
        return ret;
    }

#if CONFIG_WIFI_SAMPLE_DHCPV4_START
    /* Start DHCP server only after AP is fully enabled */
    dhcp_server_start(*ap_iface);
#endif

    return 0;
}
