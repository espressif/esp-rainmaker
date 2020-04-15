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

from rmaker_lib.logger import log


class Device:
    """
    Device class used to instantiate instances of device
    to perform various device operations.
    """
    def __init__(self, node, device):
        """
        Instantiate device object.
        """
        log.info("Initialising device " + device['name'])
        self.__node = node
        self.__name = device['name']
        self.__params = {}
        for param in device['params']:
            self.__params[param["name"]] = ''

    def get_device_name(self):
        """
        Get the device name.
        """
        return self.__name

    def get_params(self):
        """
        Get parameters of the device.
        """
        params = {}
        node_params = self.__node.get_node_params()
        if node_params is None:
            return params
        for key in self.__params.keys():
            params[key] = node_params[self.__name + '.' + key]
        return params

    def set_params(self, data):

        """
        Set parameters of the device.
        Input data contains the dictionary of device parameters.
        """
        request_payload = {}
        for key in data:
            request_payload[self.__name + '.' + key] = data[key]
        return self.__node.set_node_params(request_payload)
