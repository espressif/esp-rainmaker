/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <sdkconfig.h>

/* The BLE session outlives provisioning, so the provisioning manager runs in
 * NETWORK_PROV_MODE_SESSION_ONLY and the endpoints other modules added stay
 * reachable. True for both modes that keep the session up. */
#if defined(CONFIG_APP_NETWORK_BLE_MODE_ALWAYS) || \
    defined(CONFIG_APP_NETWORK_BLE_MODE_SESSION_ONLY)
#define APP_NETWORK_BLE_PERSISTENT_SESSION  1
#else
#define APP_NETWORK_BLE_PERSISTENT_SESSION  0
#endif

/* Provisioning is offered by this build, so the network stack is initialised.
 * Both are skipped together, because without provisioning there is no path to
 * credentials and the stack has no use. */
#if defined(CONFIG_APP_NETWORK_BLE_MODE_SESSION_ONLY)
#define APP_NETWORK_PROV_ENABLED            0
#else
#define APP_NETWORK_PROV_ENABLED            1
#endif
