/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app/server/OnboardingCodesUtil.h>
#include <crypto/CHIPCryptoPAL.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <protocols/secure_channel/PASESession.h>
#include <setup_payload/SetupPayload.h>

#include "dynamic_qrcode.h"
#include "esp_log.h"

static const char *TAG = "dynamic_qrcode";

CHIP_ERROR DynamicPasscodeCommissionableDataProvider::GetSetupDiscriminator(uint16_t &setupDiscriminator)
{
    if (discriminator >= 4096) {
        chip::Crypto::DRBG_get_bytes(reinterpret_cast<uint8_t *>(&discriminator), sizeof(discriminator));
        discriminator %= 4096;
    }
    setupDiscriminator = discriminator;
    return CHIP_NO_ERROR;
}

CHIP_ERROR DynamicPasscodeCommissionableDataProvider::GetSpake2pIterationCount(uint32_t &iterationCount)
{
    return chip::DeviceLayer::Internal::ESP32Config::ReadConfigValue(
        chip::DeviceLayer::Internal::ESP32Config::kConfigKey_Spake2pIterationCount, iterationCount);
}

CHIP_ERROR DynamicPasscodeCommissionableDataProvider::GetSpake2pSalt(chip::MutableByteSpan &saltBuf)
{
    static constexpr size_t kSpake2pSalt_MaxBase64Len =
        BASE64_ENCODED_LEN(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length) + 1;
    CHIP_ERROR err = CHIP_NO_ERROR;
    char saltB64[kSpake2pSalt_MaxBase64Len] = {0};
    size_t saltB64Len = 0;
    err = chip::DeviceLayer::Internal::ESP32Config::ReadConfigValueStr(
        chip::DeviceLayer::Internal::ESP32Config::kConfigKey_Spake2pSalt, saltB64, sizeof(saltB64), saltB64Len);
    ReturnErrorOnFailure(err);
    size_t saltLen = chip::Base64Decode32(saltB64, saltB64Len, reinterpret_cast<uint8_t *>(saltB64));
    VerifyOrReturnValue(saltLen <= saltBuf.size(), CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy(saltBuf.data(), saltB64, saltLen);
    saltBuf.reduce_size(saltLen);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DynamicPasscodeCommissionableDataProvider::GetSpake2pVerifier(chip::MutableByteSpan &verifierBuf,
                                                                         size_t &verifierLen)
{
    chip::Crypto::Spake2pVerifier verifier;
    uint32_t iterationCount = 0;
    uint32_t setupPasscode = 0;
    uint8_t saltData[chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length] = {0};
    chip::MutableByteSpan saltBuf(saltData);
    ReturnErrorOnFailure(DynamicPasscodeCommissionableDataProvider::GetSpake2pIterationCount(iterationCount));
    ReturnErrorOnFailure(DynamicPasscodeCommissionableDataProvider::GetSetupPasscode(setupPasscode));
    ReturnErrorOnFailure(DynamicPasscodeCommissionableDataProvider::GetSpake2pSalt(saltBuf));
    CHIP_ERROR err = chip::PASESession::GeneratePASEVerifier(verifier, iterationCount, saltBuf, false, setupPasscode);
    ReturnErrorOnFailure(err);
    err = verifier.Serialize(verifierBuf);
    verifierLen = verifierBuf.size();
    ReturnErrorOnFailure(err);
    return CHIP_NO_ERROR;
}

CHIP_ERROR DynamicPasscodeCommissionableDataProvider::GetSetupPasscode(uint32_t &setupPasscode)
{
    if (passcode == 0) {
        while (!chip::SetupPayload::IsValidSetupPIN(passcode)) {
            chip::Crypto::DRBG_get_bytes(reinterpret_cast<uint8_t *>(&passcode), sizeof(passcode));
            passcode = (passcode % chip::kSetupPINCodeMaximumValue) + 1;
        }
    }
    setupPasscode = passcode;
    return CHIP_NO_ERROR;
}

const char *DynamicPasscodeCommissionableDataProvider::GetDynamicQRcodeStr()
{
    static bool hasInitialized = false;
    if (hasInitialized) {
        return dynamicQrcodeData;
    }
    CHIP_ERROR ret = CHIP_NO_ERROR;
    chip::PayloadContents payload;
    uint16_t setupDiscriminator = 0;
    char qrCodeData[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1] = {0};
    chip::MutableCharSpan qrCodeBuf(qrCodeData);
    chip::RendezvousInformationFlags rendezvous(chip::RendezvousInformationFlag::kSoftAP,
                                                chip::RendezvousInformationFlag::kBLE);
    chip::DeviceLayer::GetDeviceInstanceInfoProvider()->GetVendorId(payload.vendorID);
    chip::DeviceLayer::GetDeviceInstanceInfoProvider()->GetProductId(payload.productID);
    GetInstance().GetSetupDiscriminator(setupDiscriminator);
    payload.discriminator.SetLongValue(setupDiscriminator);
    GetInstance().GetSetupPasscode(payload.setUpPINCode);
    payload.rendezvousInformation.SetValue(rendezvous);
    ESP_LOGI(TAG, "DeviceInfo vid:%d pid:%d discriminator:%d passcode:%d", payload.vendorID, payload.productID,
             payload.discriminator.GetLongValue(), payload.setUpPINCode);
    ret = GetQRCode(qrCodeBuf, payload);
    for (size_t i = 0; i < qrCodeBuf.size(); ++i) {
        *(dynamicQrcodeData + i) = *(qrCodeBuf.data() + i);
    }
    *(dynamicQrcodeData + qrCodeBuf.size()) = '\0';

    if (ret != CHIP_NO_ERROR) {
        return nullptr;
    }
    hasInitialized = true;
    return dynamicQrcodeData;
}
