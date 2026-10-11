#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
void Axp192_I2CInit();

void Axp192_WriteBytes(uint8_t reg_addr, uint8_t *data, uint16_t length);

void Axp192_ReadBytes(uint8_t reg_addr, uint8_t *data, uint16_t length);

void Axp192_Write8Bit(uint8_t reg_addr, uint8_t value);

void Axp192_WriteBits(uint8_t reg_addr, uint8_t data, uint8_t bit_pos, uint8_t bit_length);

uint8_t Axp192_Read8Bit(uint8_t reg_addr);

uint16_t Axp192_Read12Bit(uint8_t reg_addr);

uint16_t Axp192_Read13Bit(uint8_t reg_addr);

uint16_t Axp192_Read16Bit(uint8_t reg_addr);

uint32_t Axp192_Read24Bit(uint8_t reg_addr);

uint32_t Axp192_Read32Bit(uint8_t reg_addr);


#ifdef __cplusplus
}
#endif