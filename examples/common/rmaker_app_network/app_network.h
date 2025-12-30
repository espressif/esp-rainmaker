/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFG_DATA_HEADER                     0xe5, 0x02
#define MGF_DATA_APP_ID                     'N', 'o', 'v'
#define MFG_DATA_VERSION                    'a'
#define MFG_DATA_CUSTOMER_ID                0x00, 0x01

#define MGF_DATA_DEVICE_TYPE_LIGHT              0x0005
#define MGF_DATA_DEVICE_TYPE_SWITCH             0x0080
#define MGF_DATA_DEVICE_TYPE_USER_AUTH          0x0101
#define MGF_DATA_DEVICE_TYPE_MATTER_CONTROLLER  0xFFF1

#define MFG_DATA_DEVICE_SUBTYPE_SWITCH              0x01
#define MFG_DATA_DEVICE_SUBTYPE_LIGHT               0x01
#define MFG_DATA_DEVICE_SUBTYPE_AI_AGENT            0x01
#define MFG_DATA_DEVICE_SUBTYPE_MATTER_CONTROLLER   0x01

#define MFG_DATA_DEVICE_EXTRA_CODE          0x00

/** ESP RainMaker Event Base */
ESP_EVENT_DECLARE_BASE(APP_NETWORK_EVENT);

/** App Network Events */
typedef enum {
    /** QR code available for display. Associated data is the NULL terminated QR payload. */
    APP_NETWORK_EVENT_QR_DISPLAY = 1,
    /** Provisioning timed out */
    APP_NETWORK_EVENT_PROV_TIMEOUT,
    /** Provisioning has restarted due to failures (Invalid SSID/Passphrase) */
    APP_NETWORK_EVENT_PROV_RESTART,
    /** Provisioning closed due to invalid credentials */
    APP_NETWORK_EVENT_PROV_CRED_MISMATCH,
} app_network_event_t;

/** Types of Proof of Possession */
typedef enum {
    /** Use MAC address to generate PoP */
    POP_TYPE_MAC,
    /** Use random stream generated and stored in fctry partition during claiming process as PoP */
    POP_TYPE_RANDOM,
    /** Do not use any PoP.
     * Use this option with caution. Consider using `CONFIG_APP_NETWORK_PROV_TIMEOUT_PERIOD` with this.
     */
    POP_TYPE_NONE,
    /** Use a custom PoP.
     * Set a custom PoP using app_network_set_custom_pop() first.
     */
    POP_TYPE_CUSTOM
} app_network_pop_type_t;

/** Initialize Wi-Fi/Thread
 *
 * This initializes Wi-Fi/Thread stack and the network provisioning manager
 */
void app_network_init();

/** Start Wi-Fi/Thread
 *
 * This will start provisioning if the node is not provisioned and will connect to any network
 * if node is provisioned. Function will return successfully only after network is connected
 *
 * @param[in] pop_type The type for Proof of Possession (PoP) pin
 *
 * @return ESP_OK on success (Network connected).
 * @return error in case of failure.
 */
esp_err_t app_network_start(app_network_pop_type_t pop_type);

/** Set custom manufacturing data
 *
 * This can be used to add some custom manufacturing data in BLE advertisements during
 * provisioning. This can be used by apps to filter the scanned BLE devices and show
 * only the relevant one. Supported by Nova Home app for light and switch
 *
 * @param[in] device_type Type of the device, like light or switch
 * @param[in] device_subtype Sub Type of the device (application specific)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_network_set_custom_mfg_data(uint16_t device_type, uint8_t device_subtype);

/** Set custom PoP
 *
 * This can be used to set a custom Proof of Possession (PoP) pin for provisioning.
 * Applicable only if POP_TYPE_CUSTOM is used for app_network_start().
 *
 * @param[in] pop A NULL terminated PoP string (typically 8 characters alphanumeric)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_network_set_custom_pop(const char *pop);

/** Get device PoP string
 *
 * Generate and return the Proof of Possession (PoP) string based on the specified type.
 * The caller is responsible for freeing the returned string.
 *
 * @param[in] pop_type The type of PoP to generate
 *
 * @return Pointer to the allocated PoP string on success (caller must free).
 * @return NULL on failure or if pop_type is POP_TYPE_NONE.
 */
char *app_network_get_device_pop(app_network_pop_type_t pop_type);

/** Get device PoP string using cached pop_type
 *
 * Returns the PoP string using the pop_type that was used in the last call to app_network_start().
 * The caller is responsible for freeing the returned string.
 *
 * @note This function should only be called after app_network_start() has been called.
 *
 * @return Pointer to the allocated PoP string on success (caller must free).
 * @return NULL if app_network_start() hasn't been called yet or on failure.
 */
char *app_network_get_device_pop_default(void);

/** Get device service name
 *
 * Returns the service name used for provisioning (e.g., BLE device name or SoftAP SSID).
 * The caller is responsible for freeing the returned string.
 *
 * @return Pointer to the allocated service name string on success (caller must free).
 * @return NULL on failure.
 */
char *app_network_get_device_service_name(void);

#if CONFIG_APP_WIFI_PROV_COMPAT
#define APP_WIFI_EVENT APP_NETWORK_EVENT
typedef app_network_event_t app_wifi_event_t;
#define APP_WIFI_EVENT_QR_DISPLAY APP_NETWORK_EVENT_QR_DISPLAY
#define APP_WIFI_EVENT_PROV_TIMEOUT APP_NETWORK_EVENT_PROV_TIMEOUT
#define APP_WIFI_EVENT_PROV_RESTART APP_NETWORK_EVENT_PROV_RESTART
#define APP_WIFI_EVENT_PROV_CRED_MISMATCH APP_NETWORK_EVENT_PROV_CRED_MISMATCH
typedef app_network_pop_type_t app_wifi_pop_type_t;
#define app_wifi_init() app_network_init()
#define app_wifi_start(pop_type) app_network_start(pop_type)
#define app_wifi_set_custom_mfg_data(device_type, device_subtype) app_network_set_custom_mfg_data(device_type, device_subtype)
#define app_wifi_set_custom_pop(pop) app_network_set_custom_pop(pop)
#endif /* !CONFIG_APP_WIFI_PROV_COMPAT */

#ifdef __cplusplus
}
#endif
