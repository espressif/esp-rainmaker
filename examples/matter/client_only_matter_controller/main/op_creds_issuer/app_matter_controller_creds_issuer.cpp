/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_check.h>
#include <esp_err.h>
#include <controller_rest_apis.h>
#include <stdint.h>

#include <credentials/CHIPCert.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/NodeId.h>
#include <lib/support/PersistentStorageMacros.h>
#include <lib/support/ScopedBuffer.h>
#include <lib/support/Span.h>

#include <app_matter_controller_creds_issuer.h>

#define TAG "MatterController"

using chip::Platform::ScopedMemoryBufferWithSize;

constexpr static char *k_controller_keypair_key = "ctl-kp";
constexpr static char *k_controller_rcac_key = "ctl-rcac";
constexpr static char *k_controller_noc_key = "ctl-noc";

static esp_err_t app_matter_controller_generate_csr(chip::MutableByteSpan &csr_span, chip::Crypto::P256Keypair &keypair)
{
    ESP_RETURN_ON_FALSE(keypair.Initialize(chip::Crypto::ECPKeyTarget::ECDSA) == CHIP_NO_ERROR, ESP_FAIL, TAG,
                        "Failed to initialize keypair");
    size_t csr_len = csr_span.size();
    ESP_RETURN_ON_FALSE(keypair.NewCertificateSigningRequest(csr_span.data(), csr_len) == CHIP_NO_ERROR, ESP_FAIL, TAG,
                        "Failed to generate CSR");
    csr_span.reduce_size(csr_len);
    return ESP_OK;
}

static esp_err_t app_matter_controller_fetch_matter_rcac(matter_controller_handle_t *handle, chip::MutableByteSpan &rcac)
{
    if (!handle->base_url || !handle->access_token || !handle->rmaker_group_id) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t rcac_der_len = rcac.size();
    ESP_RETURN_ON_ERROR(
        fetch_fabric_rcac_der(handle->base_url, handle->access_token, handle->rmaker_group_id, rcac.data(),
                              &rcac_der_len),
        TAG, "Failed to fetch fabric RCAC der file");
    rcac.reduce_size(rcac_der_len);
    return ESP_OK;
}

static esp_err_t app_matter_controller_issue_matter_noc(matter_controller_handle_t *handle, const chip::MutableByteSpan &csr_span,
                                                        chip::MutableByteSpan &noc)
{
    if (!handle->base_url || !handle->access_token || !handle->rmaker_group_id) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t noc_der_len = noc.size();
    ESP_RETURN_ON_ERROR(create_matter_controller(handle->base_url, handle->access_token, esp_rmaker_get_node_id(),
                                                 handle->rmaker_group_id, &(handle->matter_node_id)), TAG,
                        "Failed to create matter controller");
    ESP_RETURN_ON_ERROR(
        issue_noc_with_csr(handle->base_url, handle->access_token, CSR_TYPE_CONTROLLER, csr_span.data(), csr_span.size(),
                           handle->rmaker_group_id, &(handle->matter_node_id), noc.data(), &noc_der_len),
        TAG, "Failed to issue user NOC");
    noc.reduce_size(noc_der_len);
    return ESP_OK;
}


esp_err_t example_op_creds_issuer::generate_controller_noc_chain(chip::NodeId node_id, chip::FabricId fabric,
                                                                chip::Crypto::P256Keypair &keypair, chip::MutableByteSpan &rcac,
                                                                chip::MutableByteSpan &icac, chip::MutableByteSpan &noc)
{
    ESP_RETURN_ON_FALSE(m_storage, ESP_ERR_INVALID_STATE, TAG, "Storage not set for credentials issuer");
    uint16_t rcac_len = rcac.size();
    if (m_storage->SyncGetKeyValue(k_controller_rcac_key, rcac.data(), rcac_len) == CHIP_NO_ERROR) {
        rcac.reduce_size(rcac_len);
    } else {
        // Add new fabric, so we need to query RCAC and IPK for the new fabric
        ESP_RETURN_ON_ERROR(app_matter_controller_fetch_matter_rcac(m_controller_handle, rcac), TAG,
                            "Failed on fetching RCAC");
        ESP_RETURN_ON_FALSE(
            m_storage->SyncSetKeyValue(k_controller_rcac_key, rcac.data(), rcac.size()) == CHIP_NO_ERROR, ESP_FAIL,
            TAG, "Failed on storing RCAC");
    }

    icac.reduce_size(0);

    chip::Crypto::P256SerializedKeypair serialized_keypair;
    uint16_t serialized_keypair_len = chip::Crypto::kP256_PublicKey_Length + chip::Crypto::kP256_PrivateKey_Length;
    uint16_t noc_len = noc.size();
    if (m_storage->SyncGetKeyValue(k_controller_keypair_key, serialized_keypair.Bytes(), serialized_keypair_len) == CHIP_NO_ERROR &&
        m_storage->SyncGetKeyValue(k_controller_noc_key, noc.data(), noc_len) == CHIP_NO_ERROR) {
        serialized_keypair.SetLength(serialized_keypair_len);
        ESP_RETURN_ON_FALSE(keypair.Deserialize(serialized_keypair) == CHIP_NO_ERROR, ESP_FAIL, TAG,
                            "Failed on deserializing keypair");
        noc.reduce_size(noc_len);
        chip::Credentials::ChipDN dn;
        ESP_RETURN_ON_FALSE(chip::Credentials::ExtractSubjectDNFromX509Cert(noc, dn) == CHIP_NO_ERROR, ESP_FAIL, TAG,
                            "Failed on extracting subject DN from NOC");
        ESP_RETURN_ON_FALSE(dn.GetCertChipId(m_controller_handle->matter_node_id) == CHIP_NO_ERROR, ESP_FAIL, TAG,
                            "Failed on get Node Id from NOC subject DN");
    } else {
        uint8_t csr_der_buf[chip::Crypto::kMIN_CSR_Buffer_Size];
        chip::MutableByteSpan csr_span(csr_der_buf);
        ESP_RETURN_ON_ERROR(app_matter_controller_generate_csr(csr_span, keypair), TAG,
                            "Failed on generating CSR");
        ESP_RETURN_ON_ERROR(app_matter_controller_issue_matter_noc(m_controller_handle, csr_span, noc), TAG,
                            "Failed on issuing NOC");
        ESP_RETURN_ON_FALSE(keypair.Serialize(serialized_keypair) == CHIP_NO_ERROR, ESP_FAIL, TAG,
                            "Failed on serializing keypair");
        ESP_RETURN_ON_FALSE(
            m_storage->SyncSetKeyValue(k_controller_keypair_key, serialized_keypair.Bytes(),
                                       serialized_keypair.Length()) == CHIP_NO_ERROR, ESP_FAIL, TAG,
            "Failed on storing keypair");
        ESP_RETURN_ON_FALSE(
            m_storage->SyncSetKeyValue(k_controller_noc_key, noc.data(), noc.size()) == CHIP_NO_ERROR, ESP_FAIL,
            TAG, "Failed on storing NOC");
        m_controller_handle->matter_noc_installed = true;
    }
    return ESP_OK;
}
