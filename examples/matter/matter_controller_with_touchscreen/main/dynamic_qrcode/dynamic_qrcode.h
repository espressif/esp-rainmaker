/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_err.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <platform/ESP32/ESP32FactoryDataProvider.h>
#include <lib/support/Span.h>

class DynamicPasscodeCommissionableDataProvider : public chip::DeviceLayer::CommissionableDataProvider
{
public:
    // ===== Members functions that implement the CommissionableDataProvider
    CHIP_ERROR GetSetupDiscriminator(uint16_t & setupDiscriminator) override;
    CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR GetSpake2pIterationCount(uint32_t & iterationCount) override;
    CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan & saltBuf) override;
    CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan & verifierBuf, size_t & verifierLen) override;
    CHIP_ERROR GetSetupPasscode(uint32_t & setupPasscode) override;
    CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    static DynamicPasscodeCommissionableDataProvider &GetInstance()
    {
        static DynamicPasscodeCommissionableDataProvider sCommissionableDataProvider;
        return sCommissionableDataProvider;
    }
    const char *GetDynamicQRcodeStr();

private:
    DynamicPasscodeCommissionableDataProvider() :
        chip::DeviceLayer::CommissionableDataProvider() {}
    uint32_t passcode = 0;
    uint16_t discriminator = 4096;
    char dynamicQrcodeData[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1] = {0};
};
