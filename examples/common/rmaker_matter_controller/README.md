# ESP RainMaker Matter Controller Component

[![Component Registry](https://components.espressif.com/components/espressif/rmaker_matter_controller/badge.svg)](https://components.espressif.com/components/espressif/rmaker_matter_controller)

A component to setup Matter controller in RainMaker.

## Workflow

The typical application flow is:

1. Create the RainMaker node and services required by the product.
2. Call `app_rmaker_matter_controller_enable()` with controller setup callbacks.
3. Enable RainMaker user-auth service and start RainMaker.
4. The component authorizes with RainMaker, sets up the Matter controller, and reports controller status.
5. Trigger a Matter device-list update when devices are added, removed, or the UI needs to refresh the fabric view.
6. The report layer converts the fetched device list into internal per-node subscription state and starts subscriptions for newly tracked nodes.
7. The application can observe local online/attribute reports and can request subscription retries for already tracked nodes.

## Public Headers

### `app_rmaker_matter_controller.h`

Provides the main component entry point and controller setup callbacks.

- `app_rmaker_matter_controller_enable()` adds the Matter controller setup service to the RainMaker node and starts the controller integration.
- `matter_controller_config_t` provides controller setup, optional NOC update, and optional device-list update callbacks.
- Controller credential helper APIs are also declared here for products that need to fetch/store Matter controller credentials explicitly.

### `app_rmaker_matter_device_list.h`

Provides the cloud device-list update API and the `matter_device_t` list type.

- `app_rmaker_matter_device_list_updatable()` checks whether RainMaker auth, group, and controller setup state are ready for a device-list update.
- `app_rmaker_matter_device_list_update()` fetches the current Matter device list from RainMaker and feeds it into the report/subscription layer.
- `app_rmaker_device_list_copy_create()` and `app_rmaker_device_list_copy_destroy()` let the application keep a copy of the callback-lifetime list.

### `app_rmaker_matter_report.h`

Provides local report callbacks and subscription retry helpers.

- `app_rmaker_matter_report_set_callback()` registers an observer for local Matter online and attribute reports.
- `app_rmaker_matter_report_retry_subscription()` retries one tracked offline node.
- `app_rmaker_matter_report_retry_subscriptions()` retries all tracked offline nodes.

### `app_rmaker_matter_controller_api.h`

Provides lower-level RainMaker Matter Fabric REST API helpers used by the component.

Most applications do not need to call these directly unless they are implementing custom controller setup, credential, or device-list flows.

## Usage

> [!NOTE]
> Refer to the [Matter controller example](https://github.com/espressif/esp-rainmaker/tree/master/examples/matter/matter_controller) for more details.

### Initialize the Component

Enable the component after the RainMaker node is created and before enabling the RainMaker user-auth service. The application provides callbacks for controller setup, optional NOC update, and optional device-list updates.

```c
#include <app_rmaker_matter_controller.h>

static esp_err_t matter_controller_setup(uint8_t *ipk, size_t ipk_len, uint64_t fabric_id)
{
    /* Set up the Matter controller for the selected RainMaker group/fabric. */
    return ESP_OK;
}

static esp_err_t matter_controller_update_noc(uint64_t fabric_id)
{
    /* Refresh controller operational credentials if the product needs it. */
    return ESP_OK;
}

static void matter_device_list_update(esp_err_t err, const matter_device_t *dev_list)
{
    /* dev_list is callback-lifetime only. Copy it if it must be retained. */
}

void app_init(void)
{
    matter_controller_config_t config = {
        .setup_callback = matter_controller_setup,
        .update_noc_callback = matter_controller_update_noc,
        .device_list_update_callback = matter_device_list_update,
    };

    ESP_ERROR_CHECK(app_rmaker_matter_controller_enable(&config));
}
```

The setup callback is where the application connects the RainMaker group/fabric information to the Matter controller stack. See the Matter controller examples for the controller-specific setup implementation.

### Observe Matter Attribute and Online Reports

Register a report callback if the application needs local notification when the controller receives Matter attribute reports or online/offline state changes.

```c
#include <app_rmaker_matter_report.h>

static void matter_report_cb(const app_rmaker_matter_report_t *report, void *priv_data)
{
    if (!report) {
        return;
    }
    switch (report->type) {
    case APP_RMAKER_MATTER_REPORT_ONLINE:
        /* report->node_id identifies the Matter node. report->data is {"online": true|false}. */
        break;
    case APP_RMAKER_MATTER_REPORT_ATTR:
        /* report->data contains the coalesced attribute batch. */
        break;
    default:
        break;
    }
}

void app_set_report_callback(void)
{
    ESP_ERROR_CHECK(app_rmaker_matter_report_set_callback(matter_report_cb, NULL));
}
```

The callback runs from the report task. The `report->data` object is read-only and valid only during the callback. Duplicate the cJSON object before passing it to another task or storing it.

### Refresh Matter Subscriptions

If the application needs fresh Matter state, such as after a user opens a device page or presses a refresh button, it can ask the component to retry subscriptions for tracked offline nodes.

```c
#include <app_rmaker_matter_report.h>

void refresh_all_matter_reports(void)
{
    ESP_ERROR_CHECK(app_rmaker_matter_report_retry_subscriptions());
}

void refresh_one_matter_node(uint64_t matter_node_id)
{
    ESP_ERROR_CHECK(app_rmaker_matter_report_retry_subscription(matter_node_id));
}
```

These APIs do not create new device-list entries. They retry report subscriptions for nodes already known to the report layer.

### Refresh the Matter Device List

To refresh the controller's Matter device list from RainMaker, use `app_rmaker_matter_device_list_update()` when `app_rmaker_matter_device_list_updatable()` returns true. Newly discovered nodes are added to the report layer and their initial subscriptions are started by the update flow.

```c
#include <app_rmaker_matter_device_list.h>

void refresh_matter_device_list(void)
{
    if (app_rmaker_matter_device_list_updatable()) {
        ESP_ERROR_CHECK(app_rmaker_matter_device_list_update());
    }
}
```

The configured `device_list_update_callback` is invoked when the update completes. The provided list is callback-lifetime only; copy it with `app_rmaker_device_list_copy_create()` if the application needs to retain it.

## Notes

### Rate Limit

Matter attribute reports are coalesced and rate-limited before being published to RainMaker. This avoids excessive cloud updates when a device sends many attribute changes in a short time.

The publish rate is controlled by a token bucket:

```text
CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY=3
CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS=10000
```

`CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_CAPACITY` sets how many attribute report batches may be published quickly after an idle period. `CONFIG_RMAKER_MTCTL_REPORT_ATTR_BUCKET_REFILL_MS` sets how often one token is refilled, bounding the sustained publish rate.

### Memory Usage

PSRAM is **strongly recommended**. The controller can hold Matter device lists, command responses, attribute reports, subscription state, and JSON publish buffers at the same time. Enable PSRAM-preferred allocation in menuconfig or sdkconfig defaults:

```text
CONFIG_RMAKER_MTCTL_MEMORY_ALLOCATION_PREFER_SPIRAM=y
CONFIG_RMAKER_MTCTL_MEMORY_REPORT_TASK_CREATE_FROM_SPIRAM=y
```

Configuring cJSON hooks is also recommended during startup, so cJSON allocations can utilize PSRAM too. Do this before creating any cJSON object:

```c
#include <cJSON.h>
#include <esp_heap_caps.h>
#include <stddef.h>

static void *cjson_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}

void app_main(void)
{
    cJSON_Hooks hooks = {
        .malloc_fn = cjson_malloc,
        .free_fn = free,
    };
    cJSON_InitHooks(&hooks);

    /* Continue application initialization. */
}
```
