#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

#include "i2c_device.h"

#define TAG "I2C-DEVICE"

#ifdef I2C_DEVICE_DEBUG_INFO
#define log_i(format...) ESP_LOGI(TAG, format)
#else
#define log_i(format...)
#endif

#ifdef I2C_DEVICE_DEBUG_ERROR
#define log_e(format...) ESP_LOGE(TAG, format)
#else
#define log_e(format...)
#endif

#ifdef I2C_DEVICE_DEBUG_REG
#define log_reg(buffer, buffer_len) ESP_LOG_BUFFER_HEX(TAG, buffer, buffer_len)
#else
#define log_reg(buffer, buffer_len)
#endif

#define I2C_TIMEOUT_MS (100) // 1000ms
#define MAX_DEVICE_NUMBER 24

typedef struct _i2c_port_obj_t {
    i2c_port_t port;
    gpio_num_t scl;
    gpio_num_t sda;
    uint32_t freq;
} i2c_port_obj_t;

typedef struct _i2c_device_t {
    i2c_port_obj_t* i2c_port;
    uint8_t addr;
} i2c_device_t;

static SemaphoreHandle_t i2c_mutex[I2C_NUM_MAX];
static i2c_port_obj_t *i2c_port_used[2] = { NULL, NULL };

I2CDevice_t i2c_malloc_device(i2c_port_t i2c_num, gpio_num_t sda, gpio_num_t scl, uint32_t freq, uint8_t device_addr) {
    if (i2c_num > I2C_NUM_MAX) {
        i2c_num = I2C_NUM_MAX;
    }

    if (i2c_mutex[0] == NULL) {
        i2c_mutex[0] = xSemaphoreCreateRecursiveMutex();
    }

    if (i2c_mutex[1] == NULL) {
        i2c_mutex[1] = xSemaphoreCreateRecursiveMutex(); 
    }

    i2c_port_obj_t* new_device_port = (i2c_port_obj_t *)malloc(sizeof(i2c_port_obj_t));
    if (new_device_port == NULL) {
        return NULL;
    }

    new_device_port->sda = sda;
    new_device_port->scl = scl;
    new_device_port->freq = freq;
    new_device_port->port = i2c_num;

    i2c_device_t* device = (i2c_device_t *)malloc(sizeof(i2c_device_t));
    if (device == NULL) {
        return NULL;
    }

    device->i2c_port = new_device_port;
    device->addr = device_addr;
    log_i("New device malloc, scl: %d, sda: %d, freq: %d HZ",
        device->i2c_port->scl, device->i2c_port->sda, device->i2c_port->freq);

    return (I2CDevice_t)device;
}

void i2c_free_device(I2CDevice_t i2c_device) {
    if (i2c_device == NULL) {
        return ;
    }
    free(((i2c_device_t *)i2c_device)->i2c_port);
    free(i2c_device);
}

BaseType_t i2c_take_port(i2c_port_t i2c_num, uint32_t timeout) {
    if (i2c_mutex[i2c_num] == NULL) {
        return pdFAIL;
    }

    return xSemaphoreTakeRecursive(i2c_mutex[i2c_num], timeout);
}

BaseType_t i2c_free_port(i2c_port_t i2c_num) {
    if (i2c_mutex[i2c_num] == NULL) {
        return pdFAIL;
    }

    return xSemaphoreGiveRecursive(i2c_mutex[i2c_num]);
}

esp_err_t i2c_apply_bus(I2CDevice_t i2c_device) {
    if (i2c_device == NULL) {
        return ESP_FAIL;
    }

    i2c_device_t* device = (i2c_device_t *)i2c_device;
    xSemaphoreTakeRecursive(i2c_mutex[device->i2c_port->port], portMAX_DELAY);
    i2c_port_obj_t* used_port = i2c_port_used[device->i2c_port->port];
    
    if (used_port == device->i2c_port) {
        return ESP_OK;
    }

    if ((used_port != NULL) && 
        (device->i2c_port->sda == used_port->sda) && 
        (device->i2c_port->scl == used_port->scl) && 
        ((device->i2c_port->freq == used_port->freq))) {
            i2c_port_used[device->i2c_port->port] = device->i2c_port;
            return ESP_OK;    
    }

    if (used_port != NULL) {
        i2c_driver_delete(device->i2c_port->port);
        if ((device->i2c_port->sda != used_port->sda) || (device->i2c_port->scl != used_port->scl)) {
            gpio_reset_pin(used_port->sda);
            gpio_reset_pin(used_port->scl);
        }
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = device->i2c_port->sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = device->i2c_port->scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = device->i2c_port->freq,
    };

    i2c_param_config(device->i2c_port->port, &conf);
    i2c_driver_install(device->i2c_port->port, I2C_MODE_MASTER, 0, 0, 0);

    i2c_port_used[device->i2c_port->port] = device->i2c_port;
    log_i("I2C config update, scl: %d, sda: %d, freq: %d HZ",
            device->i2c_port->scl, device->i2c_port->sda, device->i2c_port->freq);
    return ESP_OK;
}

void i2c_free_bus(I2CDevice_t i2c_device) {
    if (i2c_device == NULL) {
        return ;
    }
    i2c_device_t* device = (i2c_device_t *)i2c_device;
    xSemaphoreGiveRecursive(i2c_mutex[device->i2c_port->port]);
}

esp_err_t i2c_read_bytes(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint16_t length) {
    if (i2c_device == NULL || (length > 0 && data == NULL)) {
        return ESP_FAIL;
    }

    i2c_device_t* device = (i2c_device_t *)i2c_device;

    i2c_cmd_handle_t write_cmd = i2c_cmd_link_create();
    i2c_master_start(write_cmd);
    i2c_master_write_byte(write_cmd, (device->addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(write_cmd, reg_addr, 1);
    i2c_master_stop(write_cmd);

    i2c_cmd_handle_t read_cmd = i2c_cmd_link_create();
    i2c_master_start(read_cmd);
    i2c_master_write_byte(read_cmd, (device->addr << 1) | I2C_MASTER_READ, 1);
    if (length > 1) {
        i2c_master_read(read_cmd, data, length - 1, I2C_MASTER_ACK);
    }
    if (length > 0) {
        i2c_master_read_byte(read_cmd, &data[length-1], I2C_MASTER_NACK);
    }
    i2c_master_stop(read_cmd);


    i2c_apply_bus(i2c_device);
    esp_err_t err = ESP_FAIL;
    err = i2c_master_cmd_begin(device->i2c_port->port, write_cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err == ESP_OK && length > 0) {
        err = i2c_master_cmd_begin(device->i2c_port->port, read_cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    }
    i2c_free_bus(i2c_device);

    i2c_cmd_link_delete(write_cmd);
    i2c_cmd_link_delete(read_cmd);

    if (err != ESP_OK) {
        log_e("I2C Read Error: 0x%02x, reg: 0x%02x, length: %d, Code: 0x%x", device->addr, reg_addr, length, err);
    } else {
        log_i("I2C Read Success: 0x%02x, reg: 0x%02x, length: %d", device->addr, reg_addr, length);
        log_reg(data, length);
    }

    return err;
}

esp_err_t i2c_read_byte(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t* data) {
    return i2c_read_bytes(i2c_device, reg_addr, data, 1);
}

esp_err_t i2c_read_bit(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint8_t bit_pos) {
    if (data == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_FAIL;
    uint8_t bit_data = 0x00;
    err = i2c_read_byte(i2c_device, reg_addr, &bit_data);
    if (err != ESP_OK) {
        return err;
    }

    *data = (bit_data >> bit_pos) & 0x01;
    return ESP_OK; 
}

esp_err_t i2c_read_bits(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint8_t bit_pos, uint8_t bit_length) {
    if ((bit_pos + bit_length > 8) || data == NULL) {
        return ESP_FAIL;
    }

    uint8_t bit_data = 0x00;
    esp_err_t err = ESP_FAIL;
    err = i2c_read_byte(i2c_device, reg_addr, &bit_data);
    if (err != ESP_OK) {
        return err;
    }

    bit_data = bit_data >> bit_pos;
    bit_data &= (1 << bit_length) - 1;
    *data = bit_data;
    return ESP_OK;
}

esp_err_t i2c_write_bytes(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint16_t length) {
    if (i2c_device == NULL || (length > 0 && data == NULL)) {
        return ESP_FAIL;
    }

    i2c_device_t* device = (i2c_device_t *)i2c_device;

    i2c_cmd_handle_t write_cmd = i2c_cmd_link_create();
    i2c_master_start(write_cmd);
    i2c_master_write_byte(write_cmd, (device->addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(write_cmd, reg_addr, 1);
    if (length > 0) {
        i2c_master_write(write_cmd, data, length, 1);
    }
    i2c_master_stop(write_cmd);

    esp_err_t err = ESP_FAIL;

    i2c_apply_bus(i2c_device);
    err = i2c_master_cmd_begin(device->i2c_port->port, write_cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_free_bus(i2c_device);

    i2c_cmd_link_delete(write_cmd);

    if (err != ESP_OK) {
        log_e("I2C Write Error, addr: 0x%02x, reg: 0x%02x, length: %d, Code: 0x%x", device->addr, reg_addr, length, err);
    } else {
        log_i("I2C Write Success, addr: 0x%02x, reg: 0x%02x, length: %d", device->addr, reg_addr, length);
        log_reg(data, length);
    }

    return err;
}

esp_err_t i2c_write_byte(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t data) {
    return i2c_write_bytes(i2c_device, reg_addr, &data, 1);
}

esp_err_t i2c_write_bit(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t data, uint8_t bit_pos) {
    uint8_t value = 0x00;
    esp_err_t err = ESP_FAIL;
    err = i2c_read_byte(i2c_device, reg_addr, &value);
    if (err != ESP_OK) {
        return err;
    }

    value &= ~(1 << bit_pos);
    value |= (data & 0x01) << bit_pos;
    return i2c_write_byte(i2c_device, reg_addr, value);
}

esp_err_t i2c_write_bits(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t data, uint8_t bit_pos, uint8_t bit_length) {
    if ((bit_pos + bit_length) > 8) {
        return ESP_FAIL;
    }

    uint8_t value = 0x00;
    esp_err_t err = ESP_FAIL;
    err = i2c_read_byte(i2c_device, reg_addr, &value);
    if (err != ESP_OK) {
        return err;
    }

    value &= ~(((1 << bit_length) - 1) << bit_pos);
    data &= (1 << bit_length) - 1;
    value |= data << bit_pos;

    return i2c_write_byte(i2c_device, reg_addr, value);
}

esp_err_t i2c_device_change_freq(I2CDevice_t i2c_device, uint32_t freq) {
    if (i2c_device == NULL) {
        return ESP_FAIL;
    }
    i2c_device_t* device = (i2c_device_t *)i2c_device;
    xSemaphoreTakeRecursive(i2c_mutex[device->i2c_port->port], portMAX_DELAY);
    if (device->i2c_port->freq == freq) {
        xSemaphoreGiveRecursive(i2c_mutex[device->i2c_port->port]);
        return ESP_OK;
    }

    device->i2c_port->freq = freq;
    if (i2c_port_used[device->i2c_port->port] == device->i2c_port) {
        i2c_port_used[device->i2c_port->port] = NULL;
    }
    xSemaphoreGiveRecursive(i2c_mutex[device->i2c_port->port]);
    return ESP_OK;
}

esp_err_t i2c_device_valid(I2CDevice_t i2c_device) {
    if (i2c_device == NULL ) {
        return ESP_FAIL;
    }

    i2c_device_t* device = (i2c_device_t *)i2c_device;

    i2c_cmd_handle_t write_cmd = i2c_cmd_link_create();
    i2c_master_start(write_cmd);
    i2c_master_write_byte(write_cmd, (device->addr << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_stop(write_cmd);

    esp_err_t err = ESP_FAIL;

    i2c_apply_bus(i2c_device);
    err = i2c_master_cmd_begin(device->i2c_port->port, write_cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_free_bus(i2c_device);

    i2c_cmd_link_delete(write_cmd);
    return err;
}
