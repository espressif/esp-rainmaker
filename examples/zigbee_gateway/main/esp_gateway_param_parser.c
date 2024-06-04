/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_app_rainmaker.h>
#include <esp_gateway_zigbee_type.h>
#include <json_parser.h>

static const char *TAG = "esp_gateway_param_parser";
/**@brief Address type.
 *
 * @ref ADDR_SHORT and @ref ADDR_LONG correspond to APS addressing mode constants
 * and must not be changed.
 */
typedef enum {
    ADDR_INVALID = 0,
    ADDR_ANY     = 1,
    ADDR_SHORT   = 2, // ZB_APS_ADDR_MODE_16_ENDP_PRESENT
    ADDR_LONG    = 3, // ZB_APS_ADDR_MODE_64_ENDP_PRESENT
} addr_type_t;

static uint8_t parse_hex_digit(const char c)
{
    uint8_t result = 0xff;

    if ((c >= '0') && (c <= '9')) {
        result = c - '0';
    } else if ((c >= 'a') && (c <= 'f')) {
        result = c - 'a' + 10;
    } else if ((c >= 'A') && (c <= 'F')) {
        result = c - 'A' + 10;
    }

    return result;
}

static bool parse_hex_str(char const *p_in_str, uint8_t in_str_len,
                   uint8_t *p_out_buff, uint8_t out_buff_size,
                   bool reverse)
{
    uint8_t i     = 0;
    int8_t  delta = 1;

    /* Skip 0x suffix if present. */
    if ((in_str_len > 2) && (p_in_str[0] == '0') && ((p_in_str[1]) == 'x')) {
        in_str_len -= 2;
        p_in_str += 2;
    }

    if (reverse) {
        p_in_str = p_in_str + in_str_len - 1;
        delta    = -1;
    }

    /* Check if we have enough output space */
    if (in_str_len > 2 * out_buff_size) {
        return false;
    }

    memset(p_out_buff, 0, out_buff_size);

    while (i < in_str_len) {
        uint8_t nibble = parse_hex_digit(*p_in_str);
        if (nibble > 0x0f) {
            break;
        }

        if (i & 0x01) {
            *p_out_buff |= reverse ? nibble << 4 : nibble;
            p_out_buff++;
        } else {
            *p_out_buff = reverse ? nibble : nibble << 4;
        }

        i += 1;
        p_in_str += delta;
    }

    return (i == in_str_len);
}

static addr_type_t parse_address(const char *input, esp_zb_ieee_addr_t *addr, addr_type_t addr_type)
{
    addr_type_t result = ADDR_INVALID;
    size_t      len    = strlen(input);

    if (!input || !addr || !strlen(input)) {
        return ADDR_INVALID;
    }

    /* Skip 0x suffix if present. */
    if ((input[0] == '0') && ((input[1]) == 'x')) {
        input += 2;
        len   -= 2;
    }

    if ((len == 2 * sizeof(esp_zb_ieee_addr_t)) &&
            (addr_type == ADDR_ANY || addr_type == ADDR_LONG)) {
        result = ADDR_LONG;
    } else if ((len == 2 * sizeof(uint16_t)) &&
               (addr_type == ADDR_ANY || addr_type == ADDR_SHORT)) {
        result = ADDR_SHORT;
    } else {
        return ADDR_INVALID;
    }

    return parse_hex_str(input, len, (uint8_t *)addr, len / 2, true) ? result : ADDR_INVALID;
}

static inline bool parse_long_address(const char *input, esp_zb_ieee_addr_t addr)
{
    return (parse_address(input, (esp_zb_ieee_addr_t *)addr, ADDR_LONG) != ADDR_INVALID);
}

static void remove_escape_char(char *json_string_data)
{
    char *target_json_string_data = json_string_data;
    while (*json_string_data)
    {
        if (*json_string_data == '\\')
        {
            json_string_data++;
        }
        *target_json_string_data++ = *json_string_data++;
    }
    *target_json_string_data = '\0';
}

esp_err_t esp_zb_prase_ic_obj(char *payload, esp_zigbee_ic_mac_address_t *ic_mac_value)
{
    int payload_len = 0;
    payload_len = strlen(payload);
    remove_escape_char(payload);
    /*
    {
    "install_code": "<install code>",
    "MAC_address": "<zigbee device mac address>",
    }
    */
    jparse_ctx_t jctx;
    char *install_code = NULL, *mac_address = NULL;
    int ret = json_parse_start(&jctx, (char *)payload, (int)payload_len);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Invalid JSON received: %s", (char *)payload);
        return -1;
    }

    /* json prase install code string*/
    int len = 0;
    ret = json_obj_get_strlen(&jctx, "install_code", &len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Aborted. zigbee joining install code found in JSON");
        return -1;
    }
    len++; /* Increment for NULL character */
    install_code = calloc(1, len);
    if (!install_code)
    {
        ESP_LOGE(TAG, "Aborted. install_code memory allocation failed");
        return -1;
    }

    json_obj_get_string(&jctx, "install_code", install_code, len);

    /* json prase mac address string */
    len = 0;
    ret = json_obj_get_strlen(&jctx, "MAC_address", &len);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Aborted. zigbee joining  MAC_address not found in JSON");
        return -1;
    }
    len++; /* Increment for NULL character */
    mac_address = calloc(1, len);
    if (!mac_address)
    {
        ESP_LOGE(TAG, "Aborted. zigbee joining MAC address memory allocation failed");
        return -1;
    }
    json_obj_get_string(&jctx, "MAC_address", mac_address, len);
    json_parse_end(&jctx);

    /* ic and mac address string convert into hex */
    if (!parse_hex_str(install_code, strlen(install_code), ic_mac_value->ic, sizeof(ic_mac_value->ic), false)) {
        ESP_LOGE(TAG, "Failed to parse IC string");
        return -1;
    }
    if (!parse_long_address(mac_address, ic_mac_value->addr)) {
        ESP_LOGE(TAG, "Failed to parse eui64");
        return -1;
    }
    return ESP_OK;
}
