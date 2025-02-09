#pragma once
#include "stdint.h"

#define AXP192_DC_VOLT_STEP  25
#define AXP192_DC_VOLT_MIN   700
#define AXP192_DC_VOLT_MAX   3500

#define AXP192_LDO_VOLT_STEP 100
#define AXP192_LDO_VOLT_MIN  1800
#define AXP192_LDO_VOLT_MAX  3300

#define AXP192_VOFF_VOLT_STEP 100
#define AXP192_VOFF_VOLT_MIN  2600
#define AXP192_VOFF_VOLT_MAX  3300

#define AXP192_LDO23_DC123_EXT_CTL_REG 0x12
#define AXP192_DC1_EN_BIT   (0)
#define AXP192_DC3_EN_BIT   (1)
#define AXP192_LDO2_EN_BIT  (2)
#define AXP192_LDO3_EN_BIT  (3)
#define AXP192_DC2_EN_BIT   (4)
#define AXP192_EXT_EN_BIT   (6)

#define AXP192_DC1_VOLT_REG         0x26
#define AXP192_DC2_VOLT_REG         0x23
#define AXP192_DC3_VOLT_REG         0x27
#define AXP192_LDO23_VOLT_REG       0x28
#define AXP192_VBUS_IPSOUT_CTL_REG  0x30 // not support yet
#define AXP192_VOFF_VOLT_REG        0x31 // PWRON short press in here
#define AXP192_POWEROFF_REG         0x32 // CHGLED in here
#define AXP192_CHG_CTL1_REG         0x33 
#define AXP192_CHG_CTL2_REG         0x34 // i don`t know how it works 
#define AXP192_SPARE_CHG_CTL_REG    0x35
#define AXP192_PEK_CTL_REG          0x36

#define AXP192_ADC1_ENABLE_REG      0x82
#define BAT_VOLT_BIT        (7)
#define BAT_CURRENT_BIT     (6)
#define ACIN_VOLT_BIT       (5)
#define ACIN_CURRENT_BIT    (4)
#define VBUS_VOLT_BIT       (3)
#define VBUS_CURRENT_BIT    (2)
#define APS_VOLT_BIT        (1)
#define TS_BIT              (0)

#define AXP192_ACIN_ADC_VOLTAGE_REG         0x56
#define AXP192_ACIN_ADC_CURRENT_REG         0x58

#define AXP192_VBUS_ADC_VOLTAGE_REG         0x5A
#define AXP192_VBUS_ADC_CURRENT_REG         0x5C

#define AXP192_BAT_ADC_VOLTAGE_REG          0x78
#define AXP192_BAT_ADC_CURRENT_IN_REG       0x7A
#define AXP192_BAT_ADC_CURRENT_OUT_REG      0x7C

#define AXP192_GPIO0_CTL_REG                0x90                   
#define AXP192_GPIO0_VOLT_REG               0x91                   
#define AXP192_GPIO1_CTL_REG                0x92                   
#define AXP192_GPIO2_CTL_REG                0x93
#define AXP192_GPIO34_CTL_REG               0x95

#define AXP192_GPIO34_STATE_REG             0x96
#define AXP192_GPIO012_STATE_REG            0x94

typedef enum {
    CHARGE_VOLT_4100mV = 0b0000,
    CHARGE_VOLT_4150mV = 0b0001,    
    CHARGE_VOLT_4200mV = 0b0010,    
    CHARGE_VOLT_4360mV = 0b0011,    
} Axp192_ChargeVolt_t;

typedef enum {
    CHARGE_Current_100mA = 0b0000,
    CHARGE_Current_190mA,
    CHARGE_Current_280mA,
    CHARGE_Current_360mA,
    CHARGE_Current_450mA,
    CHARGE_Current_550mA,
    CHARGE_Current_630mA,
    CHARGE_Current_700mA,
    CHARGE_Current_780mA,
    CHARGE_Current_880mA,
    CHARGE_Current_960mA,
    CHARGE_Current_1000mA,
    CHARGE_Current_1080mA,
    CHARGE_Current_1160mA,
    CHARGE_Current_1240mA,
    CHARGE_Current_1320mA,
} Axp192_ChargeCurrent_t;

typedef enum {
    SPARE_CHARGE_VOLT_3100mV = 0x00,
    SPARE_CHARGE_VOLT_3000mV = 0x01,    
    SPARE_CHARGE_VOLT_2500mV = 0x03,    
} Axp192_SpareChargeVolt_t;

typedef enum {
    SPARE_CHARGE_Current_50uA = 0x00,
    SPARE_CHARGE_Current_100uA = 0x01,    
    SPARE_CHARGE_Current_200uA = 0x02,    
    SPARE_CHARGE_Current_400uA = 0x03,    
} Axp192_SpareChargeCurrent_t;

typedef enum {
    STARTUP_128mS = 0x00,
    STARTUP_512mS = 0x01,
    STARTUP_1S = 0x02,
    STARTUP_2S = 0x03,
} Axp192_StartupTime_t;

typedef enum {
    POWEROFF_4S = 0x00,
    POWEROFF_6S = 0x01,
    POWEROFF_8S = 0x02,
    POWEROFF_10S = 0x03,
} Axp192_PoweroffTime_t;

void Axp192_Init();

void Axp192_EnableLDODCExt(uint8_t value);

void Axp192_EnableExten(uint8_t state);

void Axp192_EnableLDO2(uint8_t state);

void Axp192_EnableLDO3(uint8_t state);

void Axp192_EnableDCDC1(uint8_t state);

void Axp192_EnableDCDC2(uint8_t state);

void Axp192_EnableDCDC3(uint8_t state);

void Axp192_SetLDO23Volt(uint16_t ldo2_voltage, uint16_t ldo3_voltage);

void Axp192_SetLDO2Volt(uint16_t voltage);

void Axp192_SetLDO3Volt(uint16_t voltage);

void Axp192_SetDCDC1Volt(uint16_t voltage);

void Axp192_SetDCDC2Volt(uint16_t voltage);

void Axp192_SetDCDC3Volt(uint16_t voltage);

void Axp192_SetVoffVolt(uint16_t voltage);

void Axp192_EnablePWRONShortWake();

float Axp192_GetVbusVolt();

float Axp192_GetAcinVolt();

float Axp192_GetBatVolt();

float Axp192_GetVbusCurrent();

float Axp192_GetAcinCurrent();

float Axp192_GetBatCurrent();

void Axp192_EnableCharge(uint16_t state);

void Axp192_SetChargeVoltage(Axp192_ChargeVolt_t volt);

void Axp192_SetChargeCurrent(Axp192_ChargeCurrent_t cur);

void Axp192_SetSpareBatCharge(uint8_t enable, Axp192_SpareChargeVolt_t volt, Axp192_SpareChargeCurrent_t current);

void Axp192_GetApsVoltage();

void Axp192_GetInternalTemp();

void Axp192_SetAdc1Enable(uint8_t value);

void Axp192_SetAdc2Enable();

void Axp192_IsBatIn();

void Axp192_IsCharging();

void Axp192_PowerOff();

void Axp192_SetPressPoweroffTime(Axp192_PoweroffTime_t time);

void Axp192_SetPressStartupTime(Axp192_StartupTime_t time);

void Axp192_WriteDataStash();

void Axp192_ReadDataStash();

void Axp192_SetGPIO4Mode(uint8_t mode);

void Axp192_SetGPIO4Level(uint8_t level);

void Axp192_SetGPIO2Mode(uint8_t mode);

void Axp192_SetGPIO2Level(uint8_t level);

void Axp192_SetGPIO0Mode(uint8_t mode);

void Axp192_SetGPIO0Volt(uint16_t volt);

void Axp192_SetGPIO1Mode(uint8_t mode);

void Axp192_SetGPIO1Level(uint8_t level);
