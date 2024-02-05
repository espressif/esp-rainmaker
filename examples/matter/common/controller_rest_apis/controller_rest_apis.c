// Copyright 2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <controller_rest_apis.h>
#include <esp_check.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_core.h>
#include <json_generator.h>
#include <json_parser.h>
#include <mbedtls/base64.h>
#include <mbedtls/pem.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define TAG "RMAKER_REST"
#define HTTP_API_VERSION "v1"
#define IPK_BYTES_LEN 16

esp_err_t fetch_access_token(const char *endpoint_url,
                             const char *refresh_token, char *access_token,
                             size_t access_token_buf_len) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(refresh_token, ESP_ERR_INVALID_ARG, TAG,
                      "refresh_token cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");

  esp_err_t ret = ESP_OK;
  char url[100];
  snprintf(url, sizeof(url), "%s/%s/%s", endpoint_url, HTTP_API_VERSION,
           "login2");
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  char *http_payload = NULL;
  const size_t http_payload_size = 4096;
  size_t http_payload_len = 0;
  int http_len, http_status_code;
  int access_token_len;
  json_gen_str_t jstr;
  jparse_ctx_t jctx;

  // Initialize http client
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Content-Type", "application/json"),
      cleanup, TAG, "Failed to set HTTP header Content-Type");
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_POST),
                    cleanup, TAG, "Failed to set HTTP method");

  // Prepare the payload for http write and read
  // The http response will include id_token and access_token, so we allocate 4K
  // Bytes buffer for the reponse.
  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to alloc memory for http_payload");
  json_gen_str_start(&jstr, http_payload, http_payload_size - 1, NULL, NULL);
  json_gen_start_object(&jstr);
  json_gen_obj_set_string(&jstr, "refreshtoken", (char *)refresh_token);
  json_gen_end_object(&jstr);
  json_gen_str_end(&jstr);
  ESP_LOGD(TAG, "HTTP write payload: %s", http_payload);

  // Send POST data
  http_payload_len = strnlen(http_payload, http_payload_size - 1);
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, http_payload_len), cleanup,
                    TAG, "Failed to open HTTP connection");
  http_len = esp_http_client_write(client, http_payload, http_payload_len);
  ESP_GOTO_ON_FALSE(http_len == http_payload_len, ESP_FAIL, close, TAG,
                    "Failed to write Payload. Returned len = %d", http_len);

  // Get response data
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = ESP_FAIL;
    goto close;
  }

  // Parse the response payload
  ESP_LOGD(TAG, "HTTP response:%s", http_payload);
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_strlen(&jctx, "accesstoken", &access_token_len) != 0 ||
      access_token_len >= access_token_buf_len ||
      json_obj_get_string(&jctx, "accesstoken", access_token,
                          access_token_buf_len - 1) != 0) {
    ESP_LOGE(TAG,
             "Failed to parse the access token from the HTTP response json");
    ret = ESP_ERR_INVALID_RESPONSE;
  } else {
    access_token[access_token_len] = 0;
  }
  json_parse_end(&jctx);
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

static esp_err_t fetch_rainmaker_group_id_pagination(
    const char *endpoint_url, const char *access_token,
    const uint64_t fabric_id, char *group_id, size_t group_id_buf_len,
    char *start_id, size_t start_id_buf_len, const int num_records,
    int *group_count) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(group_id, ESP_ERR_INVALID_ARG, TAG,
                      "group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(start_id, ESP_ERR_INVALID_ARG, TAG,
                      "start_id cannot be NULL");
  ESP_RETURN_ON_FALSE(group_count, ESP_ERR_INVALID_ARG, TAG,
                      "group_count cannot be NULL");

  esp_err_t ret = ESP_OK;
  char url[256] = {0};
  if (start_id[0] != 0 && strnlen(start_id, start_id_buf_len - 1) > 0) {
    snprintf(url, sizeof(url), "%s/%s/%s?%s&start_id=%s&num_records=%d",
             endpoint_url, HTTP_API_VERSION, "user/node_group",
             "node_list=false&sub_groups=false&node_details=false&is_matter="
             "true&fabric_details=false",
             start_id, num_records);
  } else {
    snprintf(url, sizeof(url), "%s/%s/%s?%s&num_records=%d", endpoint_url,
             HTTP_API_VERSION, "user/node_group",
             "node_list=false&sub_groups=false&node_details=false&is_matter="
             "true&fabric_details=false",
             num_records);
  }

  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 1526,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  int http_len, http_status_code;
  jparse_ctx_t jctx;
  int group_index;
  char fabric_id_str[17];
  int rainmaker_group_id_len = 0;
  int start_id_len = 0;
  char *http_payload = NULL;
  const size_t http_payload_size = 1024;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client.");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");

  // HTTP GET
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set HTTP method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTP connection");

  // Read response
  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, close, TAG,
                    "Failed to alloc memory for http_payload");
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  // Parse the response payload
  ESP_LOGD(TAG, "HTTP response:%s", http_payload);
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_array(&jctx, "groups", group_count) != 0) {
    ESP_LOGE(TAG, "Failed to parse the groups array from the HTTP response");
    json_parse_end(&jctx);
    ret = ESP_FAIL;
    goto close;
  }
  for (group_index = 0; group_index < *group_count; ++group_index) {
    if (json_arr_get_object(&jctx, group_index) == 0) {
      if (json_obj_get_string(&jctx, "fabric_id", fabric_id_str,
                              sizeof(fabric_id_str)) == 0) {
        if (strtoull(fabric_id_str, NULL, 16) == fabric_id) {
          if (json_obj_get_strlen(&jctx, "group_id", &rainmaker_group_id_len) !=
                  0 ||
              json_obj_get_string(&jctx, "group_id", group_id,
                                  group_id_buf_len - 1) != 0) {
            ESP_LOGE(TAG, "Failed to parse the group_id for fabric: 0x%llu",
                     fabric_id);
            ret = ESP_FAIL;
          } else {
            group_id[rainmaker_group_id_len] = 0;
          }
          json_arr_leave_object(&jctx);
          break;
        }
      }
      json_arr_leave_object(&jctx);
    }
  }
  json_obj_leave_array(&jctx);
  // Get the next_id for the next fetch
  if (group_index == *group_count) {
    ret = ESP_ERR_NOT_FOUND;
    if (json_obj_get_strlen(&jctx, "next_id", &start_id_len) == 0 &&
        json_obj_get_string(&jctx, "next_id", start_id, start_id_buf_len - 1) ==
            0) {
      start_id[start_id_len] = 0;
    } else {
      ret = ESP_FAIL;
    }
  }
  json_parse_end(&jctx);
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

esp_err_t fetch_rainmaker_group_id(const char *endpoint_url,
                                   const char *access_token,
                                   const uint64_t fabric_id, char *group_id,
                                   size_t group_id_buf_len) {
  esp_err_t err = ESP_OK;
  char next_group_id[24] = {0};
  int group_count = 0;
  const int num_records = 4;

  do {
    err = fetch_rainmaker_group_id_pagination(
        endpoint_url, access_token, fabric_id, group_id, group_id_buf_len,
        next_group_id, sizeof(next_group_id), num_records, &group_count);
  } while (err == ESP_ERR_NOT_FOUND && group_count == num_records);

  return err;
}

esp_err_t fetch_matter_fabric_id(const char *endpoint_url,
                                 const char *access_token, const char *group_id,
                                 uint64_t *fabric_id) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(group_id, ESP_ERR_INVALID_ARG, TAG,
                      "group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(fabric_id, ESP_ERR_INVALID_ARG, TAG,
                      "fabric_id cannot be NULL");

  esp_err_t ret = ESP_OK;
  char url[256];
  snprintf(url, sizeof(url),
           "%s/%s/"
           "%s?group_id=%s&node_list=false&sub_groups=false&node_details=false&"
           "is_matter=true&"
           "fabric_details=false",
           endpoint_url, HTTP_API_VERSION, "user/node_group", group_id);
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 1024,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  char *http_payload = NULL;
  const size_t http_payload_size = 1024;
  char fabric_id_str[17] = {0};
  int group_size = 0, fabric_id_str_len = 0;
  int http_len, http_status_code;
  jparse_ctx_t jctx;

  // Initialize http client
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Content-Type", "application/json"),
      cleanup, TAG, "Failed to set HTTP header Content-Type");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");

  // HTTP GET
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set http method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTP connection");

  // Read response
  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, close, TAG,
                    "Failed to alloc memory for http_payload");
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  // Parse the response payload
  ESP_LOGI(TAG, "HTTP response:%s", http_payload);
  ret = ESP_FAIL;
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_array(&jctx, "groups", &group_size) == 0 &&
      group_size == 1) {
    if (json_arr_get_object(&jctx, 0) == 0) {
      if (json_obj_get_strlen(&jctx, "fabric_id", &fabric_id_str_len) == 0 &&
          fabric_id_str_len == 16 &&
          json_obj_get_string(&jctx, "fabric_id", fabric_id_str,
                              sizeof(fabric_id_str)) == 0) {
        fabric_id_str[fabric_id_str_len] = 0;
        *fabric_id = strtoull(fabric_id_str, NULL, 16);
        if (*fabric_id != 0) {
          ret = ESP_OK;
        }
      }
      json_arr_leave_object(&jctx);
    }
    json_obj_leave_array(&jctx);
  }
  json_parse_end(&jctx);
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

#define PEM_BEGIN_CSR "-----BEGIN CERTIFICATE REQUEST-----\n"
#define PEM_END_CSR "-----END CERTIFICATE REQUEST-----\n"

static size_t format_csr(const char *input, char *output,
                         size_t output_buf_size) {
  size_t output_len = 0;
  // replace '\n' to '\\'+'n' so it can be the JSON input of http client
  while (*input) {
    if (*input != '\n') {
      if (output_len >= output_buf_size - 1) {
        return 0;
      }
      output[output_len] = *input;
      output_len++;
    } else {
      if (output_len >= output_buf_size - 2) {
        return 0;
      }
      output[output_len] = '\\';
      output[output_len + 1] = 'n';
      output_len += 2;
    }
    input++;
  }
  output[output_len] = 0;
  return output_len;
}

static size_t deformat_cert(const char *input, char *output,
                            size_t output_buf_size) {
  size_t output_len = 0;
  // replace '\\'+'n' to '\n' so it can be converted to DER-Encoded certificate
  while (*input) {
    if (*input == '\\' && *(input + 1) == 'n') {
      output[output_len] = '\n';
      input += 2;
    } else {
      output[output_len] = *input;
      input++;
    }
    output_len++;
    if (output_len >= output_buf_size) {
      return 0;
    }
  }
  output[output_len] = 0;
  return output_len;
}

static esp_err_t convert_der_to_pem(const uint8_t *der, size_t der_len,
                                    char *pem_buf, size_t pem_buf_size) {
  ESP_RETURN_ON_FALSE(der && der_len > 0, ESP_ERR_INVALID_ARG, TAG,
                      "der cannot be NULL");
  ESP_RETURN_ON_FALSE(pem_buf && pem_buf_size > der_len, ESP_ERR_INVALID_ARG,
                      TAG, "pem_buf cannot be NULL");
  size_t pem_len = pem_buf_size;
  // copy the csr_der to the end of the csr_pem buffer
  memcpy(pem_buf + pem_buf_size - der_len, der, der_len);
  ESP_RETURN_ON_FALSE(mbedtls_pem_write_buffer(
                          PEM_BEGIN_CSR, PEM_END_CSR,
                          (uint8_t *)(pem_buf + pem_buf_size - der_len),
                          der_len, (uint8_t *)(pem_buf), 1024, &pem_len) == 0,
                      ESP_FAIL, TAG,
                      "Failed to convert DER-Encoded CSR to PEM-Encoded CSR");
  return ESP_OK;
}

static esp_err_t convert_pem_to_der(const char *pem, uint8_t *der_buf,
                                    size_t *der_len) {
  ESP_RETURN_ON_FALSE(pem && strlen(pem) > 0, ESP_ERR_INVALID_ARG, TAG,
                      "pem cannot be NULL");
  ESP_RETURN_ON_FALSE(der_buf && der_len, ESP_ERR_INVALID_ARG, TAG,
                      "der_buf cannot be NULL");
  size_t pem_len = strlen(pem);
  size_t len = 0;
  const char *s1, *s2, *end = pem + pem_len;
  s1 = (char *)strstr(pem, "-----BEGIN");
  if (s1 == NULL) {
    return ESP_FAIL;
  }
  s2 = (char *)strstr(pem, "-----END");
  if (s2 == NULL) {
    return ESP_FAIL;
  }
  s1 += 10;
  while (s1 < end && *s1 != '-') {
    s1++;
  }
  while (s1 < end && *s1 == '-') {
    s1++;
  }
  if (*s1 == '\r') {
    s1++;
  }
  if (*s1 == '\n') {
    s1++;
  }
  int ret =
      mbedtls_base64_decode(NULL, 0, &len, (const unsigned char *)s1, s2 - s1);
  if (ret == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
    return ESP_FAIL;
  }
  if (len > *der_len) {
    return ESP_FAIL;
  }
  if ((ret = mbedtls_base64_decode(der_buf, len, &len,
                                   (const unsigned char *)s1, s2 - s1)) != 0) {
    return ESP_FAIL;
  }
  *der_len = len;
  return ESP_OK;
}

static int convert_char_to_digit(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

static bool convert_string_to_bytes(const char *str, uint8_t *bytes,
                                    size_t bytes_len) {
  if (strlen(str) != bytes_len * 2) {
    return false;
  }
  for (size_t i = 0; i < bytes_len; ++i) {
    int byte_h = convert_char_to_digit(str[2 * i]);
    int byte_l = convert_char_to_digit(str[2 * i + 1]);
    if (byte_h < 0 || byte_l < 0) {
      return false;
    }
    bytes[i] = (((uint8_t)byte_h) << 4) + (uint8_t)byte_l;
  }
  return true;
}

static esp_err_t fetch_fabric_rcac_pem(const char *endpoint_url,
                                       const char *access_token,
                                       const char *group_id, char *rcac_pem,
                                       size_t rcac_pem_buf_size) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(group_id, ESP_ERR_INVALID_ARG, TAG,
                      "group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(rcac_pem, ESP_ERR_INVALID_ARG, TAG,
                      "rcac_pem cannot be NULL");

  esp_err_t ret = ESP_OK;
  char url[256];
  snprintf(url, sizeof(url),
           "%s/%s/"
           "%s?group_id=%s&node_list=false&sub_groups=false&node_details=false&"
           "is_matter=true&"
           "fabric_details=true",
           endpoint_url, HTTP_API_VERSION, "user/node_group", group_id);
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 1024,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  char *http_payload = NULL;
  const size_t http_payload_size = 1536;
  char *rcac_pem_formatted = NULL;
  const size_t rcac_pem_formatted_size = 1024;
  int group_size = 0, rcac_pem_len = 0;
  int http_len, http_status_code;
  jparse_ctx_t jctx;
  bool rcac_fetched = false;

  // Initialize http client
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Content-Type", "application/json"),
      cleanup, TAG, "Failed to set HTTP header Content-Type");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");

  // HTTP GET
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set http method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTP connection");

  // Read response
  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, close, TAG,
                    "Failed to alloc memory for http_payload");
  rcac_pem_formatted =
      (char *)MEM_CALLOC_EXTRAM(rcac_pem_formatted_size, sizeof(char));
  ESP_GOTO_ON_FALSE(rcac_pem_formatted, ESP_ERR_NO_MEM, close, TAG,
                    "Failed to alloc memory for rcac_pem_formatted");
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  // Parse the response payload
  ESP_LOGD(TAG, "HTTP response:%s", http_payload);
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_array(&jctx, "groups", &group_size) == 0 &&
      group_size == 1) {
    if (json_arr_get_object(&jctx, 0) == 0) {
      if (json_obj_get_object(&jctx, "fabric_details") == 0) {
        if (json_obj_get_strlen(&jctx, "root_ca", &rcac_pem_len) == 0 &&
            rcac_pem_len < rcac_pem_formatted_size) {
          if (json_obj_get_string(&jctx, "root_ca", rcac_pem_formatted,
                                  rcac_pem_formatted_size) == 0) {
            deformat_cert(rcac_pem_formatted, rcac_pem, rcac_pem_buf_size);
            rcac_fetched = true;
          }
        }
        json_obj_leave_object(&jctx);
      }
      json_arr_leave_object(&jctx);
    }
    json_obj_leave_array(&jctx);
  }
  json_parse_end(&jctx);
  ret = rcac_fetched ? ESP_OK : ESP_FAIL;
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  if (rcac_pem_formatted) {
    free(rcac_pem_formatted);
  }
  return ret;
}

static esp_err_t fetch_device_noc(const char *endpoint_url,
                                  const char *access_token, csr_type_t csr_type,
                                  const char *csr_pem_formatted,
                                  uint64_t *matter_node_id,
                                  const char *rainmaker_group_id, char *noc_pem,
                                  size_t noc_pem_buf_size) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(csr_pem_formatted, ESP_ERR_INVALID_ARG, TAG,
                      "csr_pem_formatted cannot be NULL");
  ESP_RETURN_ON_FALSE(rainmaker_group_id, ESP_ERR_INVALID_ARG, TAG,
                      "rainmaker_group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(noc_pem, ESP_ERR_INVALID_ARG, TAG,
                      "noc_pem cannot be NULL");
  ESP_RETURN_ON_FALSE(matter_node_id, ESP_ERR_INVALID_ARG, TAG,
                      "matter_node_id cannot be NULL");

  esp_err_t ret = ESP_OK;
  char url[256];
  snprintf(url, sizeof(url), "%s/%s/%s", endpoint_url, HTTP_API_VERSION,
           "user/node_group");
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 2048,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  char matter_node_id_str[17] = {0};
  snprintf(matter_node_id_str, sizeof(matter_node_id_str), "%016llX",
           *matter_node_id);
  char noc_user_id[17] = {0};
  json_gen_str_t jstr;
  jparse_ctx_t jctx;
  int http_len, http_status_code, cert_count, noc_pem_formatted_len;
  char *http_payload = NULL;
  const size_t http_payload_size = 2048;
  size_t http_payload_len = 0;
  char *noc_pem_formatted = NULL;
  const size_t noc_pem_formatted_size = 1024;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client.");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Content-Type", "application/json"),
      cleanup, TAG, "Failed to set HTTP header Content-Type");
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_PUT),
                    cleanup, TAG, "Failed to set HTTP method");

  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to alloc memory for http_payload");
  noc_pem_formatted =
      (char *)MEM_CALLOC_EXTRAM(noc_pem_formatted_size, sizeof(char));
  ESP_GOTO_ON_FALSE(noc_pem_formatted, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to alloc memory for noc_pem_formatted");

  // Generate the JSON payload
  json_gen_str_start(&jstr, http_payload, http_payload_size - 1, NULL, NULL);
  json_gen_start_object(&jstr);
  json_gen_obj_set_string(&jstr, "operation", "add");
  json_gen_push_array(&jstr, "csr_requests");
  json_gen_start_object(&jstr);
  if (csr_type == CSR_TYPE_CONTROLLER) {
    json_gen_obj_set_string(&jstr, "role", "secondary");
    json_gen_obj_set_string(&jstr, "matter_node_id", matter_node_id_str);
  }
  json_gen_obj_set_string(&jstr, "group_id", (char *)rainmaker_group_id);
  json_gen_obj_set_string(&jstr, "csr", (char *)csr_pem_formatted);
  json_gen_end_object(&jstr);
  json_gen_pop_array(&jstr);
  if (csr_type == CSR_TYPE_CONTROLLER) {
    json_gen_obj_set_string(&jstr, "csr_type", "controller");
  } else if (csr_type == CSR_TYPE_USER) {
    json_gen_obj_set_string(&jstr, "csr_type", "user");
  }
  json_gen_end_object(&jstr);
  json_gen_str_end(&jstr);
  ESP_LOGD(TAG, "HTTP write payload: %s", http_payload);

  // Send POST data
  http_payload_len = strnlen(http_payload, http_payload_size - 1);
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, http_payload_len), cleanup,
                    TAG, "Failed to open HTTP connection");
  http_len = esp_http_client_write(client, http_payload, http_payload_len);
  ESP_GOTO_ON_FALSE(http_len == http_payload_len, ESP_FAIL, close, TAG,
                    "Failed to write Payload. Returned len = %d.", http_len);

  // Read response
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  // Parse http response
  ESP_LOGD(TAG, "http_response %s", http_payload);
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_array(&jctx, "certificates", &cert_count) == 0 &&
      cert_count == 1) {
    if (json_arr_get_object(&jctx, 0) == 0) {
      if (json_obj_get_strlen(&jctx, "user_noc", &noc_pem_formatted_len) == 0 &&
          json_obj_get_string(&jctx, "user_noc", noc_pem_formatted,
                              noc_pem_formatted_size) == 0) {
        noc_pem_formatted[noc_pem_formatted_len] = 0;
        if (csr_type == CSR_TYPE_USER) {
          if (json_obj_get_string(&jctx, "matter_user_id", noc_user_id, 17) ==
              0) {
            ESP_LOGI(TAG, "New NOC user id : 0x%s", noc_user_id);
          }
          *matter_node_id = strtoull(noc_user_id, NULL, 16);
        }
      }
      json_arr_leave_object(&jctx);
    }
    json_obj_leave_array(&jctx);
  }
  json_parse_end(&jctx);

  // De-format the noc_pem
  ESP_GOTO_ON_FALSE(noc_pem_formatted_len > 0, ESP_FAIL, close, TAG,
                    "Failed to get formatted NOC from HTTP response");
  ESP_GOTO_ON_FALSE(
      deformat_cert(noc_pem_formatted, noc_pem, noc_pem_buf_size) > 0, ESP_FAIL,
      close, TAG, "Failed to de-formatted NOC");
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  if (noc_pem_formatted) {
    free(noc_pem_formatted);
  }
  return ret;
}

esp_err_t fetch_fabric_ipk(const char *endpoint_url, const char *access_token,
                           const char *group_id, uint8_t *ipk_buf,
                           size_t ipk_buf_size) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(group_id, ESP_ERR_INVALID_ARG, TAG,
                      "group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(ipk_buf && ipk_buf_size >= IPK_BYTES_LEN,
                      ESP_ERR_INVALID_ARG, TAG, "ipk_buf is not enough");

  esp_err_t ret = ESP_OK;
  char url[256];
  snprintf(url, sizeof(url),
           "%s/%s/"
           "%s?group_id=%s&node_list=false&sub_groups=false&node_details=false&"
           "is_matter=true&"
           "fabric_details=true",
           endpoint_url, HTTP_API_VERSION, "user/node_group", group_id);
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 1024,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  char *http_payload = NULL;
  const size_t http_payload_size = 1536;
  char ipk_str[2 * IPK_BYTES_LEN + 1] = {0};
  int group_size = 0, ipk_str_len = 0;
  int http_len, http_status_code;
  jparse_ctx_t jctx;
  bool ipk_fetched = false;

  // Initialize http client
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Content-Type", "application/json"),
      cleanup, TAG, "Failed to set HTTP header Content-Type");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");

  // HTTP GET
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set http method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTPconnection");

  // Read response
  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, close, TAG,
                    "Failed to alloc memory for http_payload");
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  // Parse the response payload
  ESP_LOGD(TAG, "HTTP response:%s", http_payload);
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_array(&jctx, "groups", &group_size) == 0 &&
      group_size == 1) {
    if (json_arr_get_object(&jctx, 0) == 0) {
      if (json_obj_get_object(&jctx, "fabric_details") == 0) {
        if (json_obj_get_strlen(&jctx, "ipk", &ipk_str_len) == 0 &&
            ipk_str_len == IPK_BYTES_LEN * 2 &&
            json_obj_get_string(&jctx, "ipk", ipk_str, sizeof(ipk_str)) == 0) {
          if (convert_string_to_bytes(ipk_str, ipk_buf, IPK_BYTES_LEN)) {
            ipk_fetched = true;
          }
        }
        json_obj_leave_object(&jctx);
      }
      json_arr_leave_object(&jctx);
    }
    json_obj_leave_array(&jctx);
  }
  json_parse_end(&jctx);
  ret = ipk_fetched ? ESP_OK : ESP_FAIL;
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

esp_err_t fetch_fabric_rcac_der(const char *endpoint_url,
                                const char *access_token,
                                const char *rainmaker_group_id,
                                unsigned char *rcac_der, size_t *rcac_der_len) {
  esp_err_t ret = ESP_OK;
  char *rcac_pem = NULL;
  const size_t rcac_pem_size = 1024;
  rcac_pem = (char *)MEM_CALLOC_EXTRAM(rcac_pem_size, sizeof(char));
  ESP_GOTO_ON_FALSE(rcac_pem, ESP_ERR_NO_MEM, exit, TAG,
                    "Failed to allocate memory for rcac_pem");
  ESP_GOTO_ON_ERROR(fetch_fabric_rcac_pem(endpoint_url, access_token,
                                          rainmaker_group_id, rcac_pem,
                                          rcac_pem_size),
                    exit, TAG, "Failed to fetch RCAC pem");
  ESP_GOTO_ON_ERROR(convert_pem_to_der(rcac_pem, rcac_der, rcac_der_len), exit,
                    TAG, "Failed to convert rcac_pem to rcac_der");
exit:
  if (rcac_pem) {
    free(rcac_pem);
  }
  return ret;
}

esp_err_t issue_noc_with_csr(const char *endpoint_url, const char *access_token,
                             csr_type_t csr_type, const uint8_t *csr_der,
                             const size_t csr_der_len,
                             const char *rainmaker_group_id,
                             uint64_t *matter_node_id, unsigned char *noc_der,
                             size_t *noc_der_len) {
  esp_err_t ret = ESP_OK;
  char *csr_pem = NULL;
  char *csr_pem_formatted = NULL;
  char *noc_pem = NULL;
  const size_t pem_buffer_size = 1024;

  csr_pem = (char *)MEM_CALLOC_EXTRAM(pem_buffer_size, sizeof(char));
  ESP_GOTO_ON_FALSE(csr_pem, ESP_ERR_NO_MEM, exit, TAG,
                    "Failed to allocate memory for csr_pem");
  ESP_GOTO_ON_ERROR(
      convert_der_to_pem(csr_der, csr_der_len, csr_pem, pem_buffer_size), exit,
      TAG, "Failed to convert csr_der to csr_pem");
  csr_pem_formatted = (char *)MEM_CALLOC_EXTRAM(pem_buffer_size, sizeof(char));
  ESP_GOTO_ON_FALSE(csr_pem_formatted, ESP_ERR_NO_MEM, exit, TAG,
                    "Failed to allocate memory for csr_pem_formatted");
  ESP_GOTO_ON_FALSE(format_csr(csr_pem, csr_pem_formatted, pem_buffer_size) > 0,
                    ESP_FAIL, exit, TAG, "Failed to format CSR");
  noc_pem = (char *)MEM_CALLOC_EXTRAM(pem_buffer_size, sizeof(char));
  ;
  ESP_GOTO_ON_FALSE(noc_pem, ESP_ERR_NO_MEM, exit, TAG,
                    "Failed to allocate memory for noc_pem");
  ESP_GOTO_ON_ERROR(fetch_device_noc(endpoint_url, access_token, csr_type,
                                     csr_pem_formatted, matter_node_id,
                                     rainmaker_group_id, noc_pem,
                                     pem_buffer_size),
                    exit, TAG, "Failed to fetch user noc");
  ESP_GOTO_ON_ERROR(convert_pem_to_der(noc_pem, noc_der, noc_der_len), exit,
                    TAG, "Failed to convert noc_pem to noc_der");
exit:
  if (csr_pem) {
    free(csr_pem);
  }
  if (csr_pem_formatted) {
    free(csr_pem_formatted);
  }
  if (noc_pem) {
    free(noc_pem);
  }
  return ret;
}

esp_err_t create_matter_controller(const char *endpoint_url, const char *access_token,
                                   const char *rainmaker_node_id, const char *rainmaker_group_id,
                                   uint64_t *matter_node_id)
{
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG, "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG, "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(rainmaker_node_id, ESP_ERR_INVALID_ARG, TAG, "rainmaker_node_id cannot be NULL");
  ESP_RETURN_ON_FALSE(rainmaker_group_id, ESP_ERR_INVALID_ARG, TAG, "rainmaker_group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(matter_node_id, ESP_ERR_INVALID_ARG, TAG, "rainmaker_group_id cannot be NULL");
  esp_err_t ret = ESP_OK;
  char url[256];
  snprintf(url, sizeof(url), "%s/%s/%s?matter_controller=true", endpoint_url, HTTP_API_VERSION,
           "user/node_group");
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 2048,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  json_gen_str_t jstr;
  jparse_ctx_t jctx;
  int http_len, http_status_code;
  char *http_payload = NULL;
  const size_t http_payload_size = 512;
  char status_str[10];
  char matter_node_id_str[17];
  size_t http_payload_len = 0;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client.");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Content-Type", "application/json"),
      cleanup, TAG, "Failed to set HTTP header Content-Type");
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_PUT),
                    cleanup, TAG, "Failed to set HTTP method");

  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to alloc memory for http_payload");

  // Generate the JSON payload
  json_gen_str_start(&jstr, http_payload, http_payload_size - 1, NULL, NULL);
  json_gen_start_object(&jstr);
  json_gen_obj_set_string(&jstr, "node_id", rainmaker_node_id);
  json_gen_obj_set_string(&jstr, "group_id", rainmaker_group_id);
  json_gen_end_object(&jstr);
  json_gen_str_end(&jstr);
  ESP_LOGI(TAG, "HTTP write payload: %s", http_payload);

  // Send POST data
  http_payload_len = strnlen(http_payload, http_payload_size - 1);
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, http_payload_len), cleanup,
                    TAG, "Failed to open HTTP connection");
  http_len = esp_http_client_write(client, http_payload, http_payload_len);
  ESP_GOTO_ON_FALSE(http_len == http_payload_len, ESP_FAIL, close, TAG,
                    "Failed to write Payload. Returned len = %d.", http_len);

  // Read response
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  // Parse http response
  ESP_LOGI(TAG, "http_response %s", http_payload);
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_string(&jctx, "status", status_str, sizeof(status_str)) == 0 &&
      strcmp(status_str, "success") == 0) {
    if (json_obj_get_string(&jctx, "matter_node_id", matter_node_id_str, sizeof(matter_node_id_str)) == 0) {
      *matter_node_id = strtoull(matter_node_id_str, NULL, 16);
    }
  }
  json_parse_end(&jctx);
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

const char *controller_node_type = "Controller";

static int fetch_matter_node_list_size(const char *endpoint_url, const char *access_token, const char *rainmaker_group_id)
{
  int response_buffer_size = 0;

  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(rainmaker_group_id, ESP_ERR_INVALID_ARG, TAG,
                      "rainmaker_group_id cannot be NULL");

  ESP_LOGD(TAG,"Access Token: %s",access_token);
  esp_err_t ret = ESP_OK;
  char url[200];
  int http_len, http_status_code;
  char *http_payload = NULL;
  const size_t http_payload_size = 512;

  snprintf(url, sizeof(url), "%s/%s/%s=%s&%s", endpoint_url, HTTP_API_VERSION,
           "user/node_group?group_id", rainmaker_group_id,
           "node_details=false&sub_groups=false&node_list=true&is_matter=true&matter_node_list=true");
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 1024,
      .buffer_size_tx = 1536,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  ESP_LOGD(TAG,"URL: %s",url);
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client.");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");
  // HTTP GET Method
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set HTTP method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTP connection");

  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to allocate memory for http_payload");

  // Read Response
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {

    response_buffer_size = http_len;
  }
  else {
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return response_buffer_size;
}

static esp_err_t parse_node_list(char* http_payload, int buffer_size, matter_device_t **device_list)
{
  jparse_ctx_t jctx;

  // Parse the http response
  if(json_parse_start(&jctx, http_payload, buffer_size ) != 0)
  {
    ESP_LOGE(TAG, "Failed to parse the HTTP response json on json_parse_start");
    return ESP_FAIL;
  }

  int num_groups;
  if(json_obj_get_array(&jctx, "groups", &num_groups)==0)
  {
    if(json_arr_get_object(&jctx,0)==0)
    {
      int num_nodes;
      if(json_obj_get_array(&jctx, "node_details", &num_nodes)==0)
      {
        ESP_LOGD(TAG,"Got %d node details",num_nodes);

        char* self_node_id = esp_rmaker_get_node_id();
        for(int node_index=0; node_index<num_nodes; node_index++)
        {
            if(json_arr_get_object(&jctx, node_index)==0)
            {
              char rainmaker_node_id_str[ESP_RAINMAKER_NODE_ID_MAX_LEN];
              int str_len;
              if (json_obj_get_strlen(&jctx, "id", &str_len) == 0)
              {
                json_obj_get_string(&jctx, "id", rainmaker_node_id_str,str_len + 1);

                rainmaker_node_id_str[str_len] = '\0';


                if(strncmp(rainmaker_node_id_str,self_node_id,strlen(self_node_id))!=0)
                {
                  char matter_node_id_str[17];
                  if (json_obj_get_strlen(&jctx, "matter_node_id", &str_len) == 0 &&
                      json_obj_get_string(&jctx, "matter_node_id", matter_node_id_str,
                                          str_len + 1) == 0)
                  {
                    matter_node_id_str[str_len] = '\0';
                    matter_device_t *device_entry = NULL;
                    device_entry = (matter_device_t *)calloc(1, sizeof(matter_device_t));
                    if (!device_entry)
                    {
                      ESP_LOGE(TAG, "Failed to alloc memory for device element");
                      json_parse_end(&jctx);
                      return ESP_FAIL;
                    }
                    else
                    {
                      device_entry->is_metadata_fetched = false;
                      device_entry->next = *device_list;
                      *device_list = device_entry;
                    }
                    device_entry->node_id = strtoull(matter_node_id_str, NULL, 16);
                    strcpy(device_entry->rainmaker_node_id,rainmaker_node_id_str);

                  }
                  else
                  {
                    ESP_LOGE(TAG,"Error parsing matter_node_id.");
                    return ESP_FAIL;
                  }
                }
                else
                {
                  ESP_LOGD(TAG,"Skipping self from Node list.");
                }

              }
              else
              {
                return ESP_FAIL;
              }

              json_arr_leave_object(&jctx);
            }
            else
            {
              return ESP_FAIL;
            }
        }
        json_obj_leave_array(&jctx);

      }
      else
      {
        return ESP_FAIL;
      }
      json_arr_leave_object(&jctx);
    }
    else
    {
      ESP_LOGE(TAG,"Error fetching object groups array");
      return ESP_FAIL;
    }
    json_obj_leave_array(&jctx);
  }
  else
  {
    ESP_LOGE(TAG,"Error parsing groups array");
    return ESP_FAIL;
  }
  json_parse_end(&jctx);

  return ESP_OK;
}

static esp_err_t fetch_matter_node_list(const char *endpoint_url,
                                        const char *access_token,
                                        const char *rainmaker_group_id,
                                        matter_device_t **matter_dev_list) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(rainmaker_group_id, ESP_ERR_INVALID_ARG, TAG,
                      "rainmaker_group_id cannot be NULL");
  ESP_RETURN_ON_FALSE(matter_dev_list && *matter_dev_list == NULL,
                      ESP_ERR_INVALID_ARG, TAG,
                      "matter_dev_list cannot be NULL and *matter_dev_list "
                      "should be an empty list");

  ESP_LOGD(TAG,"Access Token: %s",access_token);
  esp_err_t ret = ESP_OK;
  char url[200];
  int http_len, http_status_code;
  char *http_payload = NULL;
  matter_device_t *new_device_list = NULL;


  int response_buffer_size = fetch_matter_node_list_size(endpoint_url,access_token,rainmaker_group_id);
  if(response_buffer_size==0)
  {
    ESP_LOGE(TAG,"Error getting Node list response Buffer size.");
    return ESP_FAIL;
  }

  response_buffer_size++; //to accomodate null termination

  ESP_LOGI(TAG,"%d Bytes Buffer required for fetching Node list.",response_buffer_size);

  snprintf(url, sizeof(url), "%s/%s/%s=%s&%s", endpoint_url, HTTP_API_VERSION,
           "user/node_group?group_id", rainmaker_group_id,
           "node_details=false&sub_groups=false&node_list=true&is_matter=true&matter_node_list=true");
  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = response_buffer_size,
      .buffer_size_tx = 1536,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  ESP_LOGD(TAG,"URL: %s",url);
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client.");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");
  // HTTP GET Method
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set HTTP method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTP connection");

  http_payload = (char *)MEM_CALLOC_EXTRAM(response_buffer_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to allocate memory for http_payload");

  // Read Response
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             response_buffer_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             response_buffer_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }

  ESP_LOGD(TAG, "HTTP response payload: %s", http_payload);

  // Read the node list from the http response
  if (parse_node_list(http_payload, response_buffer_size, &new_device_list) == ESP_OK) {
    *matter_dev_list = new_device_list;
  } else {
    free_matter_device_list(new_device_list);
  }

close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

static esp_err_t get_node_reachable(jparse_ctx_t *jctx, bool *value) {
  bool value_got = false;
  if (json_obj_get_object(jctx, "status") == 0) {
    if (json_obj_get_object(jctx, "connectivity") == 0) {
      if (json_obj_get_bool(jctx, "connected", value) == 0) {
        value_got = true;
      }
      json_obj_leave_object(jctx);
    }
    json_obj_leave_object(jctx);
  }
  return value_got ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t get_node_metadata(jparse_ctx_t *jctx, matter_device_t *dev) {
  if (json_obj_get_object(jctx, "metadata") == 0) {
      if (json_obj_get_object(jctx, "Matter") == 0) {
        int device_type = 0;
        if (json_obj_get_int(jctx, "deviceType", &device_type) == 0) {
          dev->endpoints[0].device_type_id = device_type;
          int ep_count = 0;
          int ep_id = 1;
          if (json_obj_get_array(jctx, "endpointsData", &ep_count) == 0) {
            json_arr_get_int(jctx, 1, &ep_id);
            dev->endpoints[0].endpoint_id = ep_id;
            json_obj_leave_array(jctx);
          }
          dev->endpoint_count = 1;
        }
        int device_name_len = 32;
        if (json_obj_get_strlen(jctx, "deviceName", &device_name_len) == 0 &&
            device_name_len < ESP_MATTER_DEVICE_NAME_MAX_LEN) {
          json_obj_get_string(jctx, "deviceName", dev->endpoints[0].device_name,
                              ESP_MATTER_DEVICE_NAME_MAX_LEN);
        }
        json_obj_get_bool(jctx, "isRainmaker", &(dev->is_rainmaker_device));
        json_obj_leave_object(jctx);
    }
    json_obj_leave_object(jctx);
  }
  return ESP_OK;
}

static esp_err_t fetch_matter_node_metadata(const char *endpoint_url,
                                            const char *access_token,
                                            matter_device_t *matter_dev) {
  ESP_RETURN_ON_FALSE(endpoint_url, ESP_ERR_INVALID_ARG, TAG,
                      "endpoint_url cannot be NULL");
  ESP_RETURN_ON_FALSE(access_token, ESP_ERR_INVALID_ARG, TAG,
                      "access_token cannot be NULL");
  ESP_RETURN_ON_FALSE(
      matter_dev && strnlen(matter_dev->rainmaker_node_id,
                            sizeof(matter_dev->rainmaker_node_id)) > 0,
      ESP_ERR_INVALID_ARG, TAG,
      "matter_dev cannot be NULL and it should have the rainmaker_node_id info");

  esp_err_t ret = ESP_OK;
  char url[200];
  snprintf(url, sizeof(url), "%s/%s/%s?node_id=%s&%s", endpoint_url,
           HTTP_API_VERSION, "user/nodes", matter_dev->rainmaker_node_id,
           "node_details=true&is_matter=true&params=false");
  int http_len, http_status_code;
  int node_count;
  char id_str[40] = {0};
  int id_str_len = 0;
  jparse_ctx_t jctx;
  char *http_payload = NULL;
  const size_t http_payload_size = 4096;

  esp_http_client_config_t config = {
      .url = url,
      .transport_type = HTTP_TRANSPORT_OVER_SSL,
      .buffer_size = 4096,
      .buffer_size_tx = 2048,
      .skip_cert_common_name_check = false,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG,
                      "Failed to initialise HTTP Client.");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "accept", "application/json"), cleanup,
      TAG, "Failed to set HTTP header accept");
  ESP_GOTO_ON_ERROR(
      esp_http_client_set_header(client, "Authorization", access_token),
      cleanup, TAG, "Failed to set HTTP header Authorization");
  // HTTP GET Method
  ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_GET),
                    cleanup, TAG, "Failed to set http method");
  ESP_GOTO_ON_ERROR(esp_http_client_open(client, 0), cleanup, TAG,
                    "Failed to open HTTP connection");
  http_payload = (char *)MEM_CALLOC_EXTRAM(http_payload_size, sizeof(char));
  ESP_GOTO_ON_FALSE(http_payload, ESP_ERR_NO_MEM, cleanup, TAG,
                    "Failed to allocate memory for http_payload");

  // Read Response
  http_len = esp_http_client_fetch_headers(client);
  http_status_code = esp_http_client_get_status_code(client);
  if ((http_len > 0) && (http_status_code == 200)) {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
  } else {
    http_len = esp_http_client_read_response(client, http_payload,
                                             http_payload_size - 1);
    http_payload[http_len] = 0;
    ESP_LOGE(TAG, "Invalid response for %s", url);
    ESP_LOGE(TAG, "Status = %d, Data = %s", http_status_code,
             http_len > 0 ? http_payload : "None");
    ret = http_status_code == 401 ? ESP_ERR_INVALID_STATE : ESP_FAIL;
    goto close;
  }
  ESP_LOGD(TAG, "HTTP response payload: %s", http_payload);

  // Parse the http response
  ESP_GOTO_ON_FALSE(
      json_parse_start(&jctx, http_payload, http_len) == 0, ESP_FAIL, close,
      TAG, "Failed to parse the HTTP response json on json_parse_start");
  if (json_obj_get_array(&jctx, "node_details", &node_count) == 0 &&
      node_count == 1) {
    if (json_arr_get_object(&jctx, 0) == 0) {
      if (json_obj_get_strlen(&jctx, "id", &id_str_len) == 0 &&
          id_str_len < sizeof(id_str) &&
          json_obj_get_string(&jctx, "id", id_str, sizeof(id_str)) == 0) {
        id_str[id_str_len] = 0;
        if (strncmp(matter_dev->rainmaker_node_id, id_str, id_str_len) != 0) {
          ESP_LOGE(TAG, "rainmaker_node_id does not match");
          ret = ESP_FAIL;
        } else {
          get_node_reachable(&jctx, &(matter_dev->reachable));
          get_node_metadata(&jctx, matter_dev);
          matter_dev->is_metadata_fetched = true;
        }
      }
      json_arr_leave_object(&jctx);
    }
    json_obj_leave_array(&jctx);
  }
  json_parse_end(&jctx);
close:
  esp_http_client_close(client);
cleanup:
  esp_http_client_cleanup(client);
  if (http_payload) {
    free(http_payload);
  }
  return ret;
}

esp_err_t fetch_matter_device_list(const char *endpoint_url,
                                   const char *access_token,
                                   const char *rainmaker_group_id,
                                   matter_device_t **matter_dev_list) {
  esp_err_t err = ESP_OK;
  ESP_RETURN_ON_ERROR(fetch_matter_node_list(endpoint_url, access_token,
                                             rainmaker_group_id,
                                             matter_dev_list),
                      TAG, "Failed to fetch matter node list");
  if (*matter_dev_list) {
    matter_device_t *dev = *matter_dev_list;
    while (dev) {
      if ((err = fetch_matter_node_metadata(endpoint_url, access_token, dev)) !=
          ESP_OK) {
        ESP_LOGE(TAG, "Failed to fetch metadata for Matter Node 0x%llX",
                 dev->node_id);
        free_matter_device_list(*matter_dev_list);
        *matter_dev_list = NULL;
        return err;
      }
      dev = dev->next;
    }
  }
  return ESP_OK;
}
