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

import requests
import json
import socket
import time
import datetime
from rmaker_lib import serverconfig, configmanager, node
from requests.exceptions import Timeout, ConnectionError,\
                                RequestException
from rmaker_lib.exceptions import NetworkError, SSLError,\
                                  RequestTimeoutError
from rmaker_lib.logger import log

OTA_PARAMS = {
    "url": "esp.param.ota_url",
    "status": "esp.param.ota_status",
    "info": "esp.param.ota_info" 
}
OTA_SERVICE_TYPE = "esp.service.ota"

class Service:
    """
    Service class used to instantiate instances of service
    to perform various service operations.
    """

    def get_service_params(self, service_config):
        """
        Get params from service config to be used
        to check for the status of the service

        :param service_config: Service config of node
        :type service_config: dict

        :return: read params and write params on Success
        :type: dict,dict
        """
        read_params = {}
        write_params = {}
        config_params = service_config["params"]
        log.debug("Getting read and write properties params from service config: " + str(config_params))
        for i in range(len(config_params)):
            cfg_type = config_params[i]["type"]
            cfg_props = config_params[i]["properties"]
            cfg_name = config_params[i]["name"]
            if 'read' in cfg_props:
                read_params[cfg_type] = cfg_name
            elif 'write' in cfg_props:
                write_params[cfg_type] = cfg_name
        return read_params, write_params

    def verify_service_exists(self, node_obj, service_type):
        """
        Verify service exists in node config

        :param node_obj: Node Object
        :type node_obj: object

        :param service_type: Type of service
        :type service_type: str

        :return: service config and service name on Success, False on Failure
        :type: dict,str | False
        """
        node_config = node_obj.get_node_config()
        service_config = node_config["services"]
        log.debug("Checking " + str(service_type) + " in node config...")
        for service in service_config:
            if service["type"] == service_type:
                return service, service["name"]
        return False, False

    def check_ota_status(self, node_obj, service_name, service_read_params):
        """
        Check status of OTA Service
        
        :param node_obj: Node Object
        :type node_obj: object
        
        :param service_name: Service Name
        :type service_name: str

        :param service_read_params: Params of service with read 
                                    properties
        :type service_read_params: dict

        :return: True on Success, False on Failure,
                 None on timeout, empty string in
                 case node reboots/connectivity low.
        :type: bool,str | None
        """
        ota_status = ""
        ota_status_empty_str = "(empty)"
        log.debug("Received service read params: " + json.dumps(service_read_params))
        ota_status_key = service_read_params[OTA_PARAMS['status']]
        ota_info_key = service_read_params[OTA_PARAMS['info']]
        log.debug("OTA Status Key : " + str(ota_status_key))
        log.debug("OTA Info Key : " + str(ota_info_key))
        while True:
            curr_status = None
            curr_info = None
            time.sleep(8)
            log.info("Getting node params for OTA status")
            new_node_params = node_obj.get_node_params()
            if service_name not in new_node_params and (curr_status not in [None, ota_status_empty_str]):
                log.info("OTA may have completed, check the node to confirm.")
                print("OTA may have completed, check the node to confirm.")
                ota_status = None
                break
            node_service_params = new_node_params[service_name]
            for k,v in node_service_params.items():
                if ota_status_key and k in ota_status_key and not v:
                    if curr_status and k.lower() in ota_status_key and not v == curr_status:
                        log.info("OTA may have completed, check the node to confirm.")
                        print("OTA may have completed, check the node to confirm.")
                        ota_status = None
                        break
                if ota_status_key and k in ota_status_key:
                    curr_status = v
                elif ota_info_key and k in ota_info_key:
                    curr_info = v

            log.debug("Current OTA status: " + str(curr_status))
            curr_time = time.time()
            if not curr_status:
                if not ota_status_key:
                    print("Node param of type: " + OTA_PARAMS['status'] + " not found... Exiting...")
                    log.debug("Node param of type: " + OTA_PARAMS['status'] + " not found...Exiting...")
                    ota_status = ""
                    break
                curr_status = ota_status_empty_str
            if not curr_info:
                if not ota_info_key:
                    print("Node param of type: " + OTA_PARAMS['info'] + " not found... Exiting...")
                    log.debug("Node param of type: " + OTA_PARAMS['info'] + " not found...Exiting...")
                    ota_status = ""
                    break
                curr_info = ota_status_empty_str
            timestamp = datetime.datetime.fromtimestamp(curr_time).strftime('%H:%M:%S')
            log.debug("[{:<6}] {:<3} : {:<3}".format(timestamp, curr_status,curr_info))
            print("[{:<8}] {:<3} : {:<3}".format(timestamp, curr_status,curr_info))

            if curr_status in ["failed"]:
                ota_status = False
                break
            elif curr_status in ["success"]:
                ota_status = True
                break

            end_time = time.time()
            log.debug("End time set to: " + str(end_time))
            if end_time - start_time > 120:
                if curr_status:
                    print("OTA taking too long...Exiting...")
                    log.info("OTA taking too long...Exiting...")
                else:
                    print("No change in OTA status, check the node to confirm...Exiting...")
                    log.info("No change in OTA status, check the node to confirm...Exiting...")
                break
        return ota_status


    def start_ota(self, node_obj, node_params, service_name, service_write_params, url_to_set):
        """
        Start OTA Service
        Set new url as node params

        :param node_obj: Node Object
        :type node_obj: object

        :param node_params: Node Params
        :type node_params: dict

        :param service_name: Service Name
        :type service_name: str

        :param url_to_set: New URL to set in node params
        :type url_to_set: str

        :return: True on Success, False on Failure
        :type: bool
        """
        global start_time
        start_time = None
        ota_url_key = service_write_params[OTA_PARAMS['url']]
        log.debug("OTA URL Key : " + str(ota_url_key))
        log.debug("Setting new url: " + str(url_to_set))
        params_to_set = {service_name: {ota_url_key: url_to_set}}
        log.debug("New node params after setting url: " + json.dumps(params_to_set))
        set_node_status = node_obj.set_node_params(params_to_set)
        if not set_node_status:
            return False
        start_time = time.time()
        log.debug("Start time set to: " + str(start_time))
        print("OTA Upgrade Started. This may take time.")
        log.info("OTA Upgrade Started. This may take time.")
        return True

    def upload_ota_image(self, node, img_name, fw_img):
        """
        Push OTA Firware image to cloud

        :param node_id: Node Id
        :type node_id: str

        :param img_file_path: Firmware Image filepath
        :type img_file_path: str

        :raises SSLError: If there is any SSL authentication error
        :raises NetworkError: If there is a network connection issue while
                              push firmware image to cloud
        :raises RequestTimeoutError: If the request is sent but there
                                     is no response from server within
                                     the timeout
        :raises Exception: If there is an HTTP issue while pushing
                           firmware image to cloud or JSON format issue
                           in HTTP response

        :return: Request Status on Success, None on Failure
        :type: str | None
        """
        socket.setdefaulttimeout(100)
        path = 'user/otaimage'
        request_payload = {
            'image_name': img_name,
            'base64_fwimage': fw_img
        }

        request_url = serverconfig.HOST + path
        try:
            log.debug("Uploading OTA Firmware Image Request URL : " +
                      str(request_url)
                     )
            response = requests.post(url=request_url,
                                     data=json.dumps(request_payload),
                                     headers=node.request_header,
                                     verify=configmanager.CERT_FILE,
                                     timeout=(60.0, 60.0))
            log.debug("Uploading OTA Firmware Image Status Response : " +
                      str(response.text))
            response.raise_for_status()
        except requests.exceptions.SSLError as ssl_err:
            log.debug(ssl_err)
            raise SSLError
        except (ConnectionError, socket.timeout) as conn_err:
            log.debug(conn_err)
            raise NetworkError
        except Timeout as time_err:
            log.debug(time_err)
            raise RequestTimeoutError
        except RequestException as mapping_status_err:
            log.debug(mapping_status_err)
            raise mapping_status_err

        try:
            response = json.loads(response.text)
        except Exception as upload_img_status_err:
            raise upload_img_status_err

        if 'status' in response:
            return response['status'], response
        return None, None
