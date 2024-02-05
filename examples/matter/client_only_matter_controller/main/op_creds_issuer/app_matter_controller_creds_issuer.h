/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <esp_matter_controller_credentials_issuer.h>

#include <controller/OperationalCredentialsDelegate.h>
#include <lib/core/CHIPCallback.h>
#include <lib/core/CHIPError.h>
#include <lib/core/CHIPPersistentStorageDelegate.h>
#include <lib/support/Span.h>

#include "app_matter_controller.h"

using chip::ByteSpan;
using chip::Callback::Callback;
using chip::Controller::OnNOCChainGeneration;

class example_op_creds_delegate : public chip::Controller::OperationalCredentialsDelegate {
public:
    example_op_creds_delegate() {}
    ~example_op_creds_delegate() override {}

    CHIP_ERROR GenerateNOCChain(const ByteSpan & csrElements, const ByteSpan & csrNonce, const ByteSpan & attestationSignature,
                                const ByteSpan & attestationChallenge, const ByteSpan & DAC, const ByteSpan & PAI,
                                Callback<OnNOCChainGeneration> * onCompletion) override
    {
        // The controller should not be able to issue NOC chains for other end devices.
        // Return BAD_REQUEST for this override function.
        return CHIP_ERROR_BAD_REQUEST;
    }
};

class example_op_creds_issuer : public esp_matter::controller::credentials_issuer {
public:
    example_op_creds_issuer(matter_controller_handle_t *controller_handle) : m_controller_handle(controller_handle) {}
    ~example_op_creds_issuer() override {}

    esp_err_t initialize_credentials_issuer(chip::PersistentStorageDelegate & storage) override
    {
        m_storage = &storage;
        return ESP_OK;
    }

    chip::Controller::OperationalCredentialsDelegate *get_delegate() override { return &m_delegate; }

    esp_err_t generate_controller_noc_chain(chip::NodeId node_id, chip::FabricId fabric,
                                            chip::Crypto::P256Keypair &keypair, chip::MutableByteSpan &rcac,
                                            chip::MutableByteSpan &icac, chip::MutableByteSpan &noc) override;

private:
    chip::PersistentStorageDelegate *m_storage;
    example_op_creds_delegate m_delegate;
    matter_controller_handle_t *m_controller_handle;
};
