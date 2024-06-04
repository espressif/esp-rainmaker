/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "sdkconfig.h"
#include <esp_zigbee_core.h>

/* Zigbee Configuration */
#define MAX_CHILDREN                    10          /* the max amount of connected devices */
#if CONFIG_ZIGBEE_INSTALLCODE_ENABLED
#define INSTALLCODE_POLICY_ENABLE       true /* enable the install code policy for security */
#else
#define INSTALLCODE_POLICY_ENABLE       false /* disable the install code policy for security */
#endif
#define ESP_ZB_PRIMARY_CHANNEL_MASK     (1l << 13)  /* Zigbee primary channel mask use in the example */
#define ESP_GW_ENDPOINT                 1           /* esp gateway endpoint */

#define RCP_VERSION_MAX_SIZE            80
#define HOST_RESET_PIN_TO_RCP_RESET     CONFIG_PIN_TO_RCP_RESET
#define HOST_BOOT_PIN_TO_RCP_BOOT       CONFIG_PIN_TO_RCP_BOOT
#define HOST_RX_PIN_TO_RCP_TX           CONFIG_PIN_TO_RCP_TX
#define HOST_TX_PIN_TO_RCP_RX           CONFIG_PIN_TO_RCP_RX

#define ESP_ZB_ZC_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,                                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#if CONFIG_ZB_RADIO_NATIVE
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }
#else
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_UART_RCP,                   \
            .radio_uart_config = {                              \
            .port = 1,                                          \
            .uart_config =                                      \
                {                                               \
                    .baud_rate = 460800,                        \
                    .data_bits = UART_DATA_8_BITS,              \
                    .parity = UART_PARITY_DISABLE,              \
                    .stop_bits = UART_STOP_BITS_1,              \
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      \
                    .rx_flow_ctrl_thresh = 0,                   \
                    .source_clk = UART_SCLK_DEFAULT,            \
                },                                              \
            .rx_pin = HOST_RX_PIN_TO_RCP_TX,                    \
            .tx_pin = HOST_TX_PIN_TO_RCP_RX,                    \
        },                                                      \
    }
#endif

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

#define ESP_ZB_RCP_UPDATE_CONFIG()                                                                                                  \
    {                                                                                                                               \
        .rcp_type = RCP_TYPE_ESP32H2_UART, .uart_rx_pin = HOST_RX_PIN_TO_RCP_TX, .uart_tx_pin = HOST_TX_PIN_TO_RCP_RX,              \
        .uart_port = 1, .uart_baudrate = 115200, .reset_pin = HOST_RESET_PIN_TO_RCP_RESET, .boot_pin = HOST_BOOT_PIN_TO_RCP_BOOT,   \
        .update_baudrate = 460800, .firmware_dir = "/rcp_fw/ot_rcp", .target_chip = ESP32H2_CHIP,                                   \
    }
