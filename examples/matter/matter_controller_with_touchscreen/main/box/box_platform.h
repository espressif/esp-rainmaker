#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t box_platform_init(void);
const char *box_platform_get_name(void);

#ifdef __cplusplus
}
#endif
