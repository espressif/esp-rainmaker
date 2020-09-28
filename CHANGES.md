# Changes

## 29-Sep-2020 (esp_rmaker_standard_types: Start default names of all standard params with capital letter)

Default parameter names like name, power, etc. have been changed to Name, Power, etc. respectively, so that they look better in the phone app UIs.

With this change, any user configured device name (the name set from phone apps), or any other persistent parameter for which a
default name was used (Eg. power) will be affected, as the values will no more be found in the NVS storage.
Please edit your application code accordingly if you want to stick with the old names.

Eg. If you were using the standard lightbulb device API which internally creates power and name parameters
```
light_device = esp_rmaker_lightbulb_device_create("Light", NULL, DEFAULT_POWER);
```

Please change to below, if you want to stick with old parameter names "name" and "power".

```
light_device = esp_rmaker_device_create("Light", ESP_RMAKER_DEVICE_LIGHTBULB, NULL);
esp_rmaker_device_add_param(light_device, esp_rmaker_name_param_create("name", "Light"));
esp_rmaker_param_t *power_param = esp_rmaker_power_param_create("power", DEFAULT_POWER);
esp_rmaker_device_add_param(light_device, power_param);
esp_rmaker_device_assign_primary_param(light_device, power_param);
```

## 5-Aug-2020 (wifi_provisioning: Use a random pop instead of creating it from MAC address)

Till date, the last 4 bytes of the MAC address were being used to generate the 8 character Proof of Possession (PoP) PIN for Wi-Fi provisioning. This is not secure enough because MAC address is a public information and can also be sniffed easily by devices in vicinity. A minor risk in this is that somebody else in the vicinity can provision your device, but a major risk is a man in the middle attack, wherein someone in vicinity can read the data being exchanged between a phone and the device and get the Wi-Fi credentials.

To prevent this, it is best to use a randomly generated PoP which cannot be guessed. So now, a random stream of bytes is generated and flashed in the fctry partition during claiming and then used as PoP. If your device is already claimed, it is recommended to erase the flash and perform the claiming again. If you erase the flash again, the PoP will change. However, if you just do a reset to factory, it will not.

If for some reason, you want to continue using the earlier mac address based method, please pass `POP_TYPE_MAC` to the `esp_err_t app_wifi_start(app_wifi_pop_type_t pop_type)` function.

## 31-July-2020 (esp\_rainmaker\_core: Code restructure and API changes)

Recently we made some significant changes to most ESP RainMaker APIs to make them even more modular and object oriented. You can check the examples to see what has changed, but here is a guide to help you understand some major changes.

### RainMaker Initialisation

#### Old
```
typedef struct {
    char *name;
    char *type;
    char *fw_version;
    char *model;
} esp_rmaker_node_info_t;

typedef struct {
    esp_rmaker_node_info_t info;
    bool enable_time_sync;
} esp_rmaker_config_t;

esp_err_t esp_rmaker_init(esp_rmaker_config_t *config);
```

#### New
```
typedef struct {
    bool enable_time_sync;
} esp_rmaker_config_t;

esp_rmaker_node_t *esp_rmaker_node_init(const esp_rmaker_config_t *config, const char *name, const char *type);
```

Optional:

```
esp_err_t esp_rmaker_node_add_fw_version(const esp_rmaker_node_t *node, const char *fw_version);
esp_err_t esp_rmaker_node_add_model(const esp_rmaker_node_t *node, const char *model);

```

- `esp_rmaker_init()` changed to `esp_rmaker_node_init()`.
- Init function now returns a node handle instead of error code.
- Node Info is no more part of the config structure. The mandatory fields, "name" and "type" are directly passed as strings during initialization.
- Model and fw version are set internally using the project name and version build variables. They can be overriden using `esp_rmaker_node_add_fw_version/model`.

### Devices

#### Old
```
typedef esp_err_t (*esp_rmaker_param_callback_t)(const char *name, const char *dev_name, esp_rmaker_param_val_t val, void *priv_data);

esp_err_t esp_rmaker_create_device(const char *dev_name, const char *type, esp_rmaker_param_callback_t cb, void *priv_data);
```

#### New
```		
typedef esp_err_t (*esp_rmaker_device_write_cb_t)(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx);
typedef esp_err_t (*esp_rmaker_device_read_cb_t)(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        void *priv_data, esp_rmaker_read_ctx_t *ctx);
        
esp_rmaker_device_t *esp_rmaker_device_create(const char *dev_name, const char *type, void *priv_data);
esp_err_t esp_rmaker_device_add_cb(const esp_rmaker_device_t *device, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb);
esp_err_t esp_rmaker_node_add_device(const esp_rmaker_node_t *node, const esp_rmaker_device_t *device);
esp_err_t esp_rmaker_node_remove_device(const esp_rmaker_node_t *node, const esp_rmaker_device_t *device);
```

- `esp_rmaker_create_device()` changed to `esp_rmaker_device_create()`. It returns a device handle which has to be used for all further operations.
- The callback has changed such that it now gets the device and param handle, rather than the names. A read callback has also been introduced for future use. It can be kept NULL.
- The callbacks have a new "context" which can have additional information related to that callback, populated by the rainmaker core.
- After creating the device, it has to be added to the node explicitly using `esp_rmaker_node_add_device()`.
- Device can be removed using `esp_rmaker_node_remove_device()`. This may be required for bridges.

### Parameters

#### Old
```
esp_err_t esp_rmaker_device_add_param(const char *dev_name, const char *param_name,
        esp_rmaker_param_val_t val, uint8_t properties);
esp_rmaker_param_add_type(const char *dev_name, const char *param_name, const char* type);
esp_err_t esp_rmaker_update_param(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val);
```

#### New
```
esp_rmaker_param_t *esp_rmaker_param_create(const char *param_name, const char *type,
        esp_rmaker_param_val_t val, uint8_t properties);
esp_err_t esp_rmaker_device_add_param(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param);
esp_err_t esp_rmaker_param_update_and_report(const esp_rmaker_param_t *param, esp_rmaker_param_val_t val);


```

- New API `esp_rmaker_param_create()` introduced to create a parameter. It returns a param handle which has to be used for all further operations.
- `esp_rmaker_device_add_param()` modified to accept the device and param handles.
- `esp_rmaker_param_add_type()` removed because the type is now included in `esp_rmaker_param_create()`
- `esp_rmaker_update_param()` changed to `esp_rmaker_param_update_and_report()`. It now accepts param handle, instead of device and parameter names.



