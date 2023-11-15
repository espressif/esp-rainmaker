
#pragma once

#include <stdint.h>
#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_services.h>
#include <esp_rmaker_ota.h>


#include "esp_rmaker_mqtt.h"


esp_err_t esp_rmaker_controller_service_enable();
esp_err_t esp_rmaker_controller_report_status_using_params(char *additional_info);
