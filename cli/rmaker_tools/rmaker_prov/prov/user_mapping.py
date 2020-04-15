# Copyright 2020 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# APIs for interpreting and creating protobuf packets for `custom-config` protocomm endpoint

from __future__ import print_function
from future.utils import tobytes
import sys
import os
prov_path = os.path.join(os.path.dirname(__file__),"../")
sys.path.insert(0, prov_path)

import utils

#import sys
#sys.path.append('../')
import proto


def print_verbose(security_ctx, data):
    if (security_ctx.verbose):
        print("++++ " + data + " ++++")


def custom_cloud_config_request(security_ctx, userid, secretkey):
    # Form protobuf request packet from custom-config data
    cmd = proto.custom_cloud_config_pb2.CloudConfigPayload()
    cmd.msg = proto.custom_cloud_config_pb2.TypeCmdGetSetDetails
    cmd.cmd_get_set_details.UserID = tobytes(userid)
    cmd.cmd_get_set_details.SecretKey = tobytes(secretkey)

    enc_cmd = security_ctx.encrypt_data(cmd.SerializeToString()).decode('latin-1')
    print_verbose(security_ctx, "Client -> Device (CustomConfig cmd) " + utils.str_to_hexstr(enc_cmd))
    return enc_cmd

def custom_cloud_config_response(security_ctx, response_data):
    # Interpret protobuf response packet
    decrypt = security_ctx.decrypt_data(tobytes(response_data))
    cmd_resp = proto.custom_cloud_config_pb2.CloudConfigPayload()
    cmd_resp.ParseFromString(decrypt)
    print_verbose(security_ctx, "CustomConfig msg value " + str(cmd_resp.msg))
    print_verbose(security_ctx, "CustomConfig Status " + str(cmd_resp.resp_get_set_details.Status))
    print_verbose(security_ctx, "CustomConfig Device Secret " + str(cmd_resp.resp_get_set_details.DeviceSecret))
    return cmd_resp.resp_get_set_details.Status, cmd_resp.resp_get_set_details.DeviceSecret
