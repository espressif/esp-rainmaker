/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <crypto/CHIPCryptoPAL.h>
#include <platform/CommissionableDataProvider.h>

using chip::MutableByteSpan;
using chip::DeviceLayer::CommissionableDataProvider;

class dynamic_commissionable_data_provider : public CommissionableDataProvider {
public:
    dynamic_commissionable_data_provider()
        : CommissionableDataProvider() {}

    // Members functions that implement the CommissionableDataProvider
    CHIP_ERROR GetSetupDiscriminator(uint16_t &setupDiscriminator) override;
    CHIP_ERROR SetSetupDiscriminator(uint16_t setupDiscriminator) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR GetSpake2pIterationCount(uint32_t &iterationCount) override;
    CHIP_ERROR GetSpake2pSalt(MutableByteSpan &saltBuf) override;
    CHIP_ERROR GetSpake2pVerifier(MutableByteSpan &verifierBuf, size_t &verifierLen) override;
    CHIP_ERROR GetSetupPasscode(uint32_t &setupPasscode) override;
    CHIP_ERROR SetSetupPasscode(uint32_t setupPasscode) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
private:
    CHIP_ERROR GenerateRandomPasscode(uint32_t &passcode);
    uint32_t mSetupPasscode = 0;
};
