# Changes

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



