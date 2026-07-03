#include "axp192.h"
#include "axp192_i2c.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define VALUE_LIMIT(x, min, max) (((x) < min) ? min : (((x) > max) ? max : (x))) 

void Axp192_Init() {
    Axp192_I2CInit();
}

void Axp192_EnableLDODCExt(uint8_t value) {
    uint8_t data = Axp192_Read8Bit(AXP192_LDO23_DC123_EXT_CTL_REG);
    data &= 0xa0;
    value |= 0x01 << AXP192_DC1_EN_BIT;
    value |= data;
    Axp192_Write8Bit(AXP192_LDO23_DC123_EXT_CTL_REG, value);
}

void Axp192_EnableExten(uint8_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_LDO23_DC123_EXT_CTL_REG, value, AXP192_EXT_EN_BIT, 1);
}

void Axp192_EnableLDO2(uint8_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_LDO23_DC123_EXT_CTL_REG, value, AXP192_LDO2_EN_BIT, 1);
}
 
void Axp192_EnableLDO3(uint8_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_LDO23_DC123_EXT_CTL_REG, value, 3, 1);
}
  
void Axp192_EnableDCDC1(uint8_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_LDO23_DC123_EXT_CTL_REG, value, AXP192_DC1_EN_BIT, 1);
}
 
void Axp192_EnableDCDC2(uint8_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_LDO23_DC123_EXT_CTL_REG, value, AXP192_DC2_EN_BIT, 1);
}
 
void Axp192_EnableDCDC3(uint8_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_LDO23_DC123_EXT_CTL_REG, value, AXP192_DC3_EN_BIT, 1);
}

void Axp192_SetLDO23Volt(uint16_t ldo2_voltage, uint16_t ldo3_voltage) {
    uint8_t value2 = 0, value3 = 0;
    ldo2_voltage = VALUE_LIMIT(ldo2_voltage, AXP192_LDO_VOLT_MIN, AXP192_LDO_VOLT_MAX);
    ldo3_voltage = VALUE_LIMIT(ldo3_voltage, AXP192_LDO_VOLT_MIN, AXP192_LDO_VOLT_MAX);
    value2 = (ldo2_voltage - AXP192_LDO_VOLT_MIN) / AXP192_LDO_VOLT_STEP;
    value3 = (ldo3_voltage - AXP192_LDO_VOLT_MIN) / AXP192_LDO_VOLT_STEP;
    Axp192_Write8Bit(AXP192_LDO23_VOLT_REG, ((value2 & 0x0f) << 4) | (value3 & 0x0f));
}

void Axp192_SetLDO2Volt(uint16_t voltage) {
    uint8_t value = 0;
    voltage = VALUE_LIMIT(voltage, AXP192_LDO_VOLT_MIN, AXP192_LDO_VOLT_MAX);
    value = (voltage - AXP192_LDO_VOLT_MIN) / AXP192_LDO_VOLT_STEP;
    Axp192_WriteBits(AXP192_LDO23_VOLT_REG, value, 4, 4);
}
 
void Axp192_SetLDO3Volt(uint16_t voltage) {
    uint8_t value = 0;
    voltage = VALUE_LIMIT(voltage, AXP192_LDO_VOLT_MIN, AXP192_LDO_VOLT_MAX);
    value = (voltage - AXP192_LDO_VOLT_MIN) / AXP192_LDO_VOLT_STEP;
    Axp192_WriteBits(AXP192_LDO23_VOLT_REG, value, 0, 4);
}

void Axp192_SetDCDC1Volt(uint16_t voltage) {
    uint8_t value = 0;
    voltage = VALUE_LIMIT(voltage, AXP192_DC_VOLT_MIN, AXP192_DC_VOLT_MAX);
    value = (voltage - AXP192_DC_VOLT_MIN) / AXP192_DC_VOLT_STEP;
    Axp192_Write8Bit(AXP192_DC1_VOLT_REG, value);
}
 
void Axp192_SetDCDC2Volt(uint16_t voltage) {
    uint8_t value = 0;
    voltage = VALUE_LIMIT(voltage, AXP192_DC_VOLT_MIN, AXP192_DC_VOLT_MAX);
    value = (voltage - AXP192_DC_VOLT_MIN) / AXP192_DC_VOLT_STEP;
    Axp192_WriteBits(AXP192_DC2_VOLT_REG, value, 0, 6);
}
 
void Axp192_SetDCDC3Volt(uint16_t voltage) {
    uint8_t value = 0;
    voltage = VALUE_LIMIT(voltage, AXP192_DC_VOLT_MIN, AXP192_DC_VOLT_MAX);
    value = (voltage - AXP192_DC_VOLT_MIN) / AXP192_DC_VOLT_STEP;
    Axp192_Write8Bit(AXP192_DC3_VOLT_REG, value);
}

void Axp192_SetVoffVolt(uint16_t voltage) {
    uint8_t value = 0;
    voltage = VALUE_LIMIT(voltage, AXP192_VOFF_VOLT_MIN, AXP192_VOFF_VOLT_MAX);
    value = (voltage - AXP192_VOFF_VOLT_MIN) / AXP192_VOFF_VOLT_STEP;
    Axp192_WriteBits(AXP192_VOFF_VOLT_REG, value, 0, 3);
}

void Axp192_EnablePWRONShortWake() {
    Axp192_WriteBits(AXP192_VOFF_VOLT_REG, 0x01, 3, 1);
}

float Axp192_GetVbusVolt() {
    float ADCLSB = 1.7 / 1000.0;
    return ADCLSB * Axp192_Read12Bit(AXP192_VBUS_ADC_VOLTAGE_REG);
}
 
float Axp192_GetAcinVolt() {
    float ADCLSB = 1.7 / 1000.0;
    return ADCLSB * Axp192_Read12Bit(AXP192_ACIN_ADC_VOLTAGE_REG);
}
 
float Axp192_GetBatVolt() {
    float ADCLSB = 1.1 / 1000.0;
    return ADCLSB * Axp192_Read12Bit(AXP192_BAT_ADC_VOLTAGE_REG);
}
 
float Axp192_GetVbusCurrent() {
    float ADCLSB = 0.375;
    return ADCLSB * Axp192_Read12Bit(AXP192_VBUS_ADC_CURRENT_REG);
}
 
float Axp192_GetAcinCurrent() {
    float ADCLSB = 0.625;
    return ADCLSB * Axp192_Read12Bit(AXP192_ACIN_ADC_CURRENT_REG);
}
 
float Axp192_GetBatCurrent() {
    float ADCLSB = 0.5;
    uint16_t current_in = Axp192_Read13Bit(AXP192_BAT_ADC_CURRENT_IN_REG);
    uint16_t current_out = Axp192_Read13Bit(AXP192_BAT_ADC_CURRENT_OUT_REG);
    return ADCLSB * (current_in - current_out);
}
 
void Axp192_EnableCharge(uint16_t state) {
    uint8_t value = state ? 1 : 0;
    Axp192_WriteBits(AXP192_CHG_CTL1_REG, value, 7, 1);
}

void Axp192_SetChargeVoltage(Axp192_ChargeVolt_t volt) {
    Axp192_WriteBits(AXP192_CHG_CTL1_REG, volt, 5, 2);
}

void Axp192_SetChargeCurrent(Axp192_ChargeCurrent_t cur) {
    Axp192_WriteBits(AXP192_CHG_CTL1_REG, cur, 0, 4);
}

void Axp192_SetSpareBatCharge(uint8_t enable, Axp192_SpareChargeVolt_t volt, Axp192_SpareChargeCurrent_t current) {
    uint8_t value = 0x00;
    value |= enable ? 0x80 : 0x00;
    value |= volt << 5;
    value |= current;
    Axp192_Write8Bit(AXP192_SPARE_CHG_CTL_REG, value);
}

void Axp192_GetApsVoltage() {

}
 
void Axp192_GetInternalTemp() {

}
 
void Axp192_SetAdc1Enable(uint8_t value) {
    Axp192_Write8Bit(AXP192_ADC1_ENABLE_REG, value);
}
 
void Axp192_SetAdc2Enable() {

}
 
void Axp192_IsBatIn() {

}
 
void Axp192_IsCharging() {

}

void Axp192_PowerOff() {
    Axp192_WriteBits(AXP192_POWEROFF_REG, 0x01, 7, 1);
}

void Axp192_SetPressPoweroffTime(Axp192_PoweroffTime_t time) {
    Axp192_WriteBits(AXP192_PEK_CTL_REG, time, 0, 2);
}

void Axp192_SetPressStartupTime(Axp192_StartupTime_t time) {
    Axp192_WriteBits(AXP192_PEK_CTL_REG, time, 6, 2);
}

void Axp192_WriteDataStash() {

}
 
void Axp192_ReadDataStash() {

}
 
void Axp192_SetGPIO3Mode() {

}

void Axp192_SetGPIO0Mode(uint8_t mode) {
    if (mode == 0) {
        Axp192_WriteBits(AXP192_GPIO0_CTL_REG, 0x01, 0, 4);
    } else {
        Axp192_WriteBits(AXP192_GPIO0_CTL_REG, 0x02, 0, 4);
    }
}

void Axp192_SetGPIO0Volt(uint16_t volt) {
    uint8_t value;
    volt = VALUE_LIMIT(volt, 1800, 3300);
    value = (volt - 1800) / 100;
    Axp192_WriteBits(AXP192_GPIO0_VOLT_REG, value, 4, 4);
}

void Axp192_SetGPIO1Mode(uint8_t mode) {
    Axp192_WriteBits(AXP192_GPIO1_CTL_REG, mode, 0, 3);
}

void Axp192_SetGPIO1Level(uint8_t level) {
    uint8_t value = level ? 1 : 0;
    Axp192_WriteBits(AXP192_GPIO012_STATE_REG, value, 1, 1);
}

void Axp192_SetGPIO2Mode(uint8_t mode) {
    Axp192_WriteBits(AXP192_GPIO2_CTL_REG, 0x00, 0, 3);
}

void Axp192_SetGPIO2Level(uint8_t level) {
    uint8_t value = level ? 1 : 0;
    Axp192_WriteBits(AXP192_GPIO012_STATE_REG, value, 2, 1);
}

void Axp192_SetGPIO4Mode(uint8_t mode) {
    Axp192_WriteBits(AXP192_GPIO34_CTL_REG, 0x01, 2, 2);
    Axp192_WriteBits(AXP192_GPIO34_CTL_REG, 0x01, 7, 1);
}

void Axp192_SetGPIO4Level(uint8_t level) {
    uint8_t value = level ? 1 : 0;
    Axp192_WriteBits(AXP192_GPIO34_STATE_REG, value, 1, 1);
}