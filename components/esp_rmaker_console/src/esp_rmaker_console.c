// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
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

#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <esp_console.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>

#include <esp_rmaker_console.h>
#include <esp_rmaker_console_internal.h>

static const char *TAG = "esp_rmaker_console";
static int stop;

static void scli_task(void *arg)
{
    int uart_num = CONFIG_ESP_RMAKER_CONSOLE_UART_NUM;
    uint8_t linebuf[256];
    int i, cmd_ret;
    esp_err_t ret;
    QueueHandle_t uart_queue;
    uart_event_t event;
    bool first_done = false;

    ESP_LOGI(TAG, "Initialising UART on port %d", uart_num);
    uart_driver_install(uart_num, 256, 0, 8, &uart_queue, 0);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
    };
    esp_console_init(&console_config);
    esp_console_register_help_command();

    while (!stop) {
        if (first_done) {
            uart_write_bytes(uart_num, "\n>> ", 4);
        } else {
            first_done = true;
        }
        memset(linebuf, 0, sizeof(linebuf));
        i = 0;
        do {
            ret = xQueueReceive(uart_queue, (void * )&event, (portTickType)portMAX_DELAY);
            if (ret != pdPASS) {
                if(stop == 1) {
                    break;
                } else {
                    continue;
                }
            }
            if (event.type == UART_DATA) {
                while (uart_read_bytes(uart_num, (uint8_t *) &linebuf[i], 1, 0)) {
                    if (linebuf[i] == '\r') {
                        uart_write_bytes(uart_num, "\r\n", 2);
                    } else {
                        uart_write_bytes(uart_num, (char *) &linebuf[i], 1);
                    }
                    i++;
                }
            }
        } while ((i < 255) && linebuf[i-1] != '\r');
        if (stop) {
            break;
        }
        /* Remove the truncating \r\n */
        linebuf[strlen((char *)linebuf) - 1] = '\0';
        /* Just to go to the next line */
        printf("\n");
        ret = esp_console_run((char *) linebuf, &cmd_ret);
        if (cmd_ret != 0) {
            printf("%s: Console command failed with error: %d\n", TAG, cmd_ret);
            cmd_ret = 0;
        }
        if (ret < 0) {
            printf("%s: Console dispatcher error\n", TAG);
            break;
        }
    }
    ESP_LOGE(TAG, "Stopped CLI");
    vTaskDelete(NULL);
}

static esp_err_t scli_init()
{
    static bool cli_started = false;
    if (cli_started) {
        return ESP_OK;
    }
#define SCLI_STACK_SIZE 4096
    if (xTaskCreate(&scli_task, "console_task", SCLI_STACK_SIZE, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Couldn't create thead");
        return ESP_ERR_NO_MEM;
    }
    cli_started = true;
    return ESP_OK;
}

esp_err_t esp_rmaker_console_init()
{
    esp_err_t ret = scli_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Couldn't initialise console");
        return ret;
    }

    register_commands();
    return ESP_OK;
}
