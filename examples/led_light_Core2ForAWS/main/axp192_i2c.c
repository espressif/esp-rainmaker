#include "stdint.h"
#include "i2c_device.h"
#include "esp_err.h"

#define AXP192_ADDR (0x34)

static I2CDevice_t axp192_device;

void Axp192_I2CInit() {
    axp192_device = i2c_malloc_device(I2C_NUM_1, 21, 22, 400000, AXP192_ADDR);
}

bool Axp192_WriteBytes(uint8_t reg_addr, uint8_t *data, uint16_t length) {
    return i2c_write_bytes(axp192_device, reg_addr, data, length) == ESP_OK;
}

bool Axp192_ReadBytes(uint8_t reg_addr, uint8_t *data, uint16_t length) {
    return i2c_read_bytes(axp192_device, reg_addr, data, length) == ESP_OK;
}

void Axp192_Write8Bit(uint8_t reg_addr, uint8_t value) {
    Axp192_WriteBytes(reg_addr, &value, 1);
}

void Axp192_WriteBits(uint8_t reg_addr, uint8_t data, uint8_t bit_pos, uint8_t bit_length) {
    if ((bit_pos + bit_length) > 8) {
        return ;
    }

    uint8_t value = 0x00;
    if (Axp192_ReadBytes(reg_addr, &value, 1) == false) {
        return ;
    }

    value &= ~(((1 << bit_length) - 1) << bit_pos);
    data &= (1 << bit_length) - 1;
    value |= data << bit_pos;

    Axp192_WriteBytes(reg_addr, &value, 1);
}

uint8_t Axp192_Read8Bit(uint8_t reg_addr) {
    uint8_t value = 0x00;
    Axp192_ReadBytes(reg_addr, &value, 1);
    return value;
}

uint16_t Axp192_Read12Bit(uint8_t reg_addr) {
    uint8_t buf[2];
    if (Axp192_ReadBytes(reg_addr, buf, 2)) {
        return (buf[0] << 4) | (buf[1] & 0x0F);
    } else {
        return 0;
    }
}

uint16_t Axp192_Read13Bit(uint8_t reg_addr) {
    uint8_t buf[2];
    if (Axp192_ReadBytes(reg_addr, buf, 2)) {
        return (buf[0] << 5) | (buf[1] & 0x1F);
    } else {
        return 0;
    }
}

uint16_t Axp192_Read16Bit(uint8_t reg_addr) {
    uint8_t buf[2];
    if (Axp192_ReadBytes(reg_addr, buf, 2)) {
        return (buf[0] << 8) | buf[1];
    } else {
        return 0;
    }
}

uint32_t Axp192_Read24Bit(uint8_t reg_addr) {
    uint8_t buf[3];
    if (Axp192_ReadBytes(reg_addr, buf, 3)) {
        return (buf[0] << 16) | (buf[1] << 8) | buf[2];
    } else {
        return 0;
    }
}

uint32_t Axp192_Read32Bit(uint8_t reg_addr) {
    uint8_t buf[4];
    if (Axp192_ReadBytes(reg_addr, buf, 3)) {
        return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    } else {
        return 0;
    }
}