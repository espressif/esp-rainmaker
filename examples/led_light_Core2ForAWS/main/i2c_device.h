#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

// Konw reg update value
// #define I2C_DEVICE_DEBUG_REG

// Know write or read success and now i2c device status 
// #define I2C_DEVICE_DEBUG_INFO

// Know write or read failed
// #define I2C_DEVICE_DEBUG_ERROR

typedef void * I2CDevice_t;

I2CDevice_t i2c_malloc_device(i2c_port_t i2c_num, gpio_num_t sda, gpio_num_t scl, uint32_t freq, uint8_t device_addr);

void i2c_free_device(I2CDevice_t i2c_device);

esp_err_t i2c_apply_bus(I2CDevice_t i2c_device);

void i2c_free_bus(I2CDevice_t i2c_device);

esp_err_t i2c_device_change_freq(I2CDevice_t i2c_device, uint32_t freq);

esp_err_t i2c_read_bytes(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint16_t length);

esp_err_t i2c_read_byte(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t* data);

esp_err_t i2c_read_bit(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint8_t bit_pos);

/*
    Read bits from 8 bit reg
    bit_pos = 4, bit_length = 3
    read ->  0b|1|0|1|0|1|1|0|0| 
             0b|-|x|x|x|-|-|-|-|   
    data = 0b00000010
*/
esp_err_t i2c_read_bits(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint8_t bit_pos, uint8_t bit_length);

esp_err_t i2c_write_bytes(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t *data, uint16_t length);

esp_err_t i2c_write_byte(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t data);

esp_err_t i2c_write_bit(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t data, uint8_t bit_pos);

/*
    Read before bits from 8 bit reg, then update write bits
    1. Read data 0b10101100
    2. write, 0b0101, bit_pos = 4, bit_length = 3 
    read ->  0b|1|0|1|0|1|1|0|0| 
             0b|-|x|x|x|-|-|-|-| 
    write -> 0b|1|1|0|1|1|1|0|0|  
    data = 0b00000101
*/
esp_err_t i2c_write_bits(I2CDevice_t i2c_device, uint8_t reg_addr, uint8_t data, uint8_t bit_pos, uint8_t bit_length);

esp_err_t i2c_device_valid(I2CDevice_t i2c_device);

BaseType_t i2c_take_port(i2c_port_t i2c_num, uint32_t timeout);

BaseType_t i2c_free_port(i2c_port_t i2c_num);


#ifdef __cplusplus
}
#endif
