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

import json
import re
import os
import sys
import time
import requests
import base64
import re
from pathlib import Path

try:
    from rmaker_lib import session, node, device, service,\
        serverconfig, configmanager
    from rmaker_lib.exceptions import NetworkError, InvalidJSONError, SSLError,\
        RequestTimeoutError
    from rmaker_lib.logger import log
    from rmaker_tools.rmaker_claim.claim import claim
except ImportError as err:
    print("Failed to import ESP Rainmaker library. " + str(err))
    raise err

MAX_HTTP_CONNECTION_RETRIES = 5

def get_nodes(vars=None):
    """
    List all nodes associated with the user.

    :param vars: No Parameters passed, defaults to `None`
    :type vars: dict | None

    :raises Exception: If there is an HTTP issue while getting nodes

    :return: None on Success
    :rtype: None
    """
    try:
        s = session.Session()
        nodes = s.get_nodes()
    except Exception as get_nodes_err:
        log.error(get_nodes_err)
    else:
        if len(nodes.keys()) == 0:
            print('User is not associated with any nodes.')
            return
        for key in nodes.keys():
            print(nodes[key].get_nodeid())
    return


def _display_dict(data):
    log.debug("Display dict....")
    log.debug("Data: {}".format(data))
    
    # Display dictionary
    for key, val in data.items():
        log.debug("key: {}, val: {}".format(key, val))
        if isinstance(val, str):
            # The string data is displayed
            log.debug("val is str")
            print("{}: {}".format(key, val), end='')
        elif isinstance(val, list):
            # If val is a list
            # values are displayed one after the other
            log.debug("val is list")
            print("\n{}: ".format(key), end='')
            first_item = val.pop(0)
            print("{}".format(first_item), end='')
            for i in val:
                print(", {}".format(i), end='')
        elif isinstance(val, dict):
            # If val is a dict
            # only the val(dict) is dislayed
            # key(of val) is not displayed
            log.debug("val is dict")
            _display_dict(val)
        else:
            log.debug("in else")
            print("{}: {}".format(key, str(val), end=''))


def _display_json(node_info):
    log.debug("Display json....")
    # Set to type list
    log.debug("Node info received: {}".format(node_info))
    if not isinstance(node_info, list):
        node_info = [ node_info ]
    log.debug("Node info set: {}".format(node_info))
    print("-"*40)
    # Display json
    for data in node_info:
        _display_dict(data)
        print("\n", end='')
        print("-"*40)


def _check_user_input(node_ids_str):
    log.debug("Check user input....")
    # Check user input format
    input_pattern = re.compile("^[0-9A-Za-z]+(,[0-9A-Za-z]+)*$")
    result = input_pattern.match(node_ids_str)
    log.debug("User input result: {}".format(result))
    if result is None:
        sys.exit("Invalid format. Expected: <nodeid>,<nodeid>,...")
    return True         


def _display_status(json_resp):
    log.debug("Displaying status....")
    resp_keys = json_resp.keys()
    log.debug("Response keys: {}".format(resp_keys))
    # Print status
    if 'status' in resp_keys:
        print("Status:", json_resp['status'])
    # Print error code
    if 'error_code' in resp_keys:
        print("Error", json_resp['error_code'], ": ", end='')
    # Print description
    if 'description' in resp_keys:
        print(json_resp['description'])


def list_shared_nodes(vars=None):
    """
    List shared nodes
    
    :param vars: `node` as key - Node Id for the node
                 (if provided)
    :type vars: dict

    :raises Exception: If there is an issue
                       while getting shared nodes

    :return: None on Success
    :rtype: None
    """
    try:
        log.debug("Get shared nodes")

        n = node.Node(vars['node'], session.Session())
        log.debug("Node id received from user: {}".format(vars['node']))

        # API
        node_json_resp = n.get_shared_nodes()
        log.debug("Get shared nodes response: {}".format(node_json_resp))
    
    except Exception as get_node_status_err:
        log.error(get_node_status_err)
    else:
        try:
            # Display result
            log.debug("Displaying status")
            _display_status(node_json_resp)
        except AttributeError as err:
            log.debug("Error: {}".format(err))
            _display_json(node_json_resp)
    
    log.debug("Get shared nodes successful")
    
    return


def add_shared_nodes(vars=None):
    """
    Add shared nodes
    
    :param vars: `nodes` as key - Node Id for the node
    :type vars: dict

    :param vars: `email` as key - Email address of the user
    :type vars: dict

    :raises Exception: If there is an issue
                       while setting nodes to share

    :return: None on Success
    :rtype: None
    """
    try:
        log.debug("Set shared nodes")

        # Remove any spaces if exist
        node_ids = vars['nodes'].strip()
        
        # Check user input format
        ret_status = _check_user_input(node_ids)
        
        # Create list from node ids string
        node_id_list = node_ids.split(',')
        log.debug("Node ids list: {}".format(node_id_list))
        
        # Get email-id
        email_id = vars['email']
        log.debug("Email-id set: {}".format(email_id))
        
        # Create API data dictionary
        api_data = {}
        api_data['nodes'] = node_id_list
        api_data['email'] = email_id
        n = node.Node(None, session.Session())
        log.debug("API data set: {}".format(api_data))
        
        # API
        node_json_resp = n.set_shared_nodes(api_data)
        log.debug("Set shared nodes response: {}".format(node_json_resp))
    
    except Exception as get_node_status_err:
        log.error(get_node_status_err)
    else:
        try:
            # Display result
            log.debug("Displaying status")
            if not isinstance(node_json_resp, dict):
                print(node_json_resp)
            else:
                _display_status(node_json_resp)
        except AttributeError as err:
            log.debug("Error: {}".format(err))
            log.debug("Displaying status")
            _display_json(node_json_resp) 
    
    log.debug("Set shared nodes successful")
    
    return


def remove_shared_nodes(vars=None):
    """
    Remove shared nodes
    
    :param vars: `nodes` as key - Node Id for the node
    :type vars: dict

    :param vars: `email` as key - Email address of the user
    :type vars: dict

    :raises Exception: If there is an issue
                       while removing shared nodes

    :return: None on Success
    :rtype: None
    """
    try:
        log.debug("Removing shared nodes")

        # Remove any spaces if exist
        node_ids = vars['nodes'].strip()
        
        # Check user input format
        ret_status = _check_user_input(node_ids)
        
        # Get email-id
        email_id = vars['email']
        log.debug("Email-id set to: {}".format(email_id))
        
        # Create API data dictionary
        api_data = {}
        api_data['nodes'] = node_ids
        api_data['email'] = email_id
        n = node.Node(None, session.Session())
        log.debug("API data set to: {}".format(api_data))
        
        # API call to remove the shared nodes
        node_json_resp = n.remove_shared_nodes(api_data)
        log.debug("Remove shared nodes response: {}".format(node_json_resp))
    
    except Exception as get_node_status_err:
        log.error(get_node_status_err)
    else:
        try:
            log.debug("Displaying status")
            # Display result
            if not isinstance(node_json_resp, dict):
                print(node_json_resp)
            else:
                _display_status(node_json_resp)
        except AttributeError as err:
            log.debug("Error: {}".format(err))
            _display_json(node_json_resp) 
    
    log.debug("Removing shared nodes successful")
    
    return


def get_node_config(vars=None):
    """
    Shows the configuration of the node.

    :param vars: `nodeid` as key - Node ID for the node, defaults to `None`
    :type vars: dict | None

    :raises Exception: If there is an HTTP issue while getting node config

    :return: None on Success
    :rtype: None
    """
    try:
        n = node.Node(vars['nodeid'], session.Session())
        node_config = n.get_node_config()
    except Exception as get_nodes_err:
        log.error(get_nodes_err)
    else:
        print(json.dumps(node_config, indent=4))
    return node_config


def get_node_status(vars=None):
    """
    Shows the online/offline status of the node.

    :param vars: `nodeid` as key - Node ID for the node, defaults to `None`
    :type vars: dict | None

    :raises Exception: If there is an HTTP issue while getting node status

    :return: None on Success
    :rtype: None
    """
    try:
        n = node.Node(vars['nodeid'], session.Session())
        node_status = n.get_node_status()
    except Exception as get_node_status_err:
        log.error(get_node_status_err)
    else:
        print(json.dumps(node_status, indent=4))
    return


def set_params(vars=None):
    """
    Set parameters of the node.

    :param vars:
        `nodeid` as key - Node ID for the node,\n
        `data` as key - JSON data containing parameters to be set `or`\n
        `filepath` as key - Path of the JSON file containing parameters
                            to be set,\n
        defaults to `None`
    :type vars: dict | None

    :raises Exception: If there is an HTTP issue while setting params or
                       JSON format issue in HTTP response

    :return: None on Success
    :rtype: None
    """
    log.info('Setting params of the node with nodeid : ' + vars['nodeid'])
    if 'data' in vars:
        data = vars['data']
    if 'filepath' in vars:
        filepath = vars['filepath']

    if data is not None:
        log.debug('Setting node parameters using JSON data.')
        # Trimming white spaces except the ones between two strings
        data = re.sub(r"(?<![a-z]|[A-Z])\s(?![a-z]|[A-Z])|\
            (?<=[a-z]|[A-Z])\s(?![a-z]|[A-Z])|\
                (?<![a-z]|[A-Z])\s(?=[a-z]|[A-Z])", "", data)
        try:
            log.debug('JSON data : ' + data)
            data = json.loads(data)
        except Exception:
            raise InvalidJSONError
            return

    elif filepath is not None:
        log.debug('Setting node parameters using JSON file.')
        file = Path(filepath)
        if not file.exists():
            log.error('File %s does not exist!' % file.name)
            return
        with open(file) as fh:
            try:
                data = json.load(fh)
                log.debug('JSON filename :' + file.name)
            except Exception:
                raise InvalidJSONError
                return

    try:
        n = node.Node(vars['nodeid'], session.Session())
        status = n.set_node_params(data)
    except SSLError:
        log.error(SSLError())
    except NetworkError as conn_err:
        print(conn_err)
        log.warn(conn_err)
    except Exception as set_params_err:
        log.error(set_params_err)
    else:
        print('Node state updated successfully.')
    return


def get_params(vars=None):
    """
    Get parameters of the node.

    :param vars: `nodeid` as key - Node ID for the node, defaults to `None`
    :type vars: dict | None

    :raises Exception: If there is an HTTP issue while getting params or
                       JSON format issue in HTTP response

    :return: None on Success
    :rtype: None
    """
    try:
        n = node.Node(vars['nodeid'], session.Session())
        params = n.get_node_params()
    except SSLError:
        log.error(SSLError())
    except NetworkError as conn_err:
        print(conn_err)
        log.warn(conn_err)
    except Exception as get_params_err:
        log.error(get_params_err)
    else:
        if params is None:
            log.error('Node status not updated.')
            return
        else:
            print(json.dumps(params, indent=4))
    return params


def remove_node(vars=None):
    """
    Removes the user node mapping.

    :param vars: `nodeid` as key - Node ID for the node, defaults to `None`
    :type vars: dict | None

    :raises NetworkError: If there is a network connection issue during
                          HTTP request for removing node
    :raises Exception: If there is an HTTP issue while removing node or
                       JSON format issue in HTTP response

    :return: None on Success
    :rtype: None
    """
    log.info('Removing user node mapping for node ' + vars['nodeid'])
    try:
        n = node.Node(vars['nodeid'], session.Session())
        params = n.remove_user_node_mapping()
    except Exception as remove_node_err:
        log.error(remove_node_err)
    else:
        log.debug('Removed the user node mapping successfully.')
        print('Removed node ' + vars['nodeid'] + ' successfully.')
    return


def get_mqtt_host(vars=None):
    """
    Returns MQTT Host endpoint

    :param vars: No Parameters passed, defaults to `None`
    :type vars: dict | None

    :raises NetworkError: If there is a network connection issue while
                          getting MQTT Host endpoint
    :raises Exception: If there is an HTTP issue while getting
                       MQTT Host endpoint or JSON format issue in HTTP response

    :return: MQTT Host endpoint
    :rtype: str
    """
    log.info("Getting MQTT Host endpoint.")
    path = 'mqtt_host'
    request_url = serverconfig.HOST.split(serverconfig.VERSION)[0] + path
    try:
        log.debug("Get MQTT Host request url : " + request_url)
        response = requests.get(url=request_url,
                                verify=configmanager.CERT_FILE)
        log.debug("Get MQTT Host response : " + response.text)
        response.raise_for_status()
    except requests.exceptions.SSLError:
        raise SSLError
    except requests.ConnectionError:
        raise NetworkError
        return
    except Exception as mqtt_host_err:
        log.error(mqtt_host_err)
        return
    try:
        response = json.loads(response.text)
    except Exception as json_decode_err:
        log.error(json_decode_err)
    if 'mqtt_host' in response:
        log.info("Received MQTT Host endpoint successfully.")
        print(response['mqtt_host'])
    else:
        log.error("MQTT Host does not exists.")
    return response['mqtt_host']


def claim_node(vars=None):
    """
    Claim the node connected to the given serial port
    (Get cloud credentials)

    :param vars: `port` as key - Serial Port, defaults to `None`
    :type vars: str | None

    :raises Exception: If there is an HTTP issue while claiming

    :return: None on Success
    :rtype: None
    """
    try:
        if not vars['port'] and not vars['mac'] and not vars['addr'] and not vars['platform']:
            sys.exit(vars['parser'].print_help())
        if vars['addr'] and not vars['port'] and not vars['platform']:
            sys.exit('Invalid. <port> or --platform argument is needed.')
        if vars['port']:
            if not vars['mac'] and not vars['platform']:
                claim(port=vars['port'], node_platform=vars['platform'], mac_addr=vars['mac'], flash_address=vars['addr'])
                return
        if (vars['mac'] and not vars['platform']):
            sys.exit("Invalid. --platform argument needed.")
        if (not vars['mac'] and vars['platform']):
            sys.exit("Invalid. --mac argument needed.")
        if vars['mac']:
            if not re.match(r'([0-9A-F]:?){12}', vars['mac']):
                sys.exit('Invalid MAC address.')
        claim(port=vars['port'], node_platform=vars['platform'], mac_addr=vars['mac'], flash_address=vars['addr'])
    except Exception as claim_err:
        log.error(claim_err)
        return

def ota_upgrade(vars=None):
    """
    Upload OTA Firmware Image
    and
    Set image url returned in response as node params
    """
    try:
        node_id = vars['nodeid']
        img_file_path = vars['otaimagepath']
        if os.path.isabs(img_file_path) is False:
            img_file_path = os.path.join(os.getcwd(), img_file_path)
        img_name = img_file_path.split('/')[-1].split('.bin')[0]
        with open(img_file_path, 'rb') as f:
            fw_img_bytes = f.read()
        base64_fw_img = base64.b64encode(fw_img_bytes).decode('ascii')

        retries = MAX_HTTP_CONNECTION_RETRIES
        node_object = None
        status = None
        response = None
        service_name = None
        service_obj = None
        service_config = None
        node_params = None
        param_url_to_set = None
        curr_session = None
        while retries > 0:
            try:
                # If session is expired then to initialise the new session
                # internet connection is required.
                if not curr_session:
                    curr_session = session.Session()
                if not node_object:
                    node_object = node.Node(node_id, curr_session)
                    log.info("Creating service object...")
                if not service_obj:
                    service_obj = service.Service()
                    log.info("Checking service " + service.OTA_SERVICE_TYPE + " in node config...")
                    print("Checking " + service.OTA_SERVICE_TYPE + " in node config...")
                if not service_config and not service_name:
                    service_config, service_name = service_obj.verify_service_exists(node_object, service.OTA_SERVICE_TYPE)
                    if not service_config:
                        log.error(service.OTA_SERVICE_TYPE + " not found.")
                        break
                    log.info("Checking service " + service.OTA_SERVICE_TYPE + " in config...Success")
                    log.debug("Service config received: " + str(service_config) +
                              " Service name received: " + str(service_name))
                    print("Uploading OTA Firmware Image...This may take time...")
                    log.info("Uploading OTA Firmware Image...This may take time...")
                if not status and not response:
                    # Upload OTA Firwmare Image
                    status, response = service_obj.upload_ota_image(node_object, img_name, base64_fw_img)
                    if status:
                        break
            except SSLError:
                log.error(SSLError())
                break
            except (NetworkError, RequestTimeoutError) as conn_err:
                print(conn_err)
                log.warn(conn_err)
            except Exception as node_init_err:
                log.error(node_init_err)
                break
            time.sleep(5)
            retries -= 1
            if retries:
                print("Retries left:", retries)
                log.info("Retries left: " + str(retries))

        if node_object is None:
            log.error('Initialising new session...Failed\n')
            return

        if not status or not 'success' in status:
            print("\n")
            log.error("OTA Upgrade...Failed")
            log.debug('OTA Upgrade...Failed '
                      'status: ' + str(status) + ' response: ' + str(response))
            return

        log.info('Upload OTA Firmware Image Request...Success')
        log.debug("Upload OTA Firmware Image Request - Status: " + json.dumps(status) +
                  " Response: " + json.dumps(response))


        retries = MAX_HTTP_CONNECTION_RETRIES
        ota_start_status = None
        node_params = None
        service_read_params = None
        service_write_params = None
        ota_status = None

        while retries > 0:
            try:
                if 'image_url' in response:
                    param_url_to_set = response["image_url"]

                    if not service_read_params and not service_write_params:
                        log.info("Getting service params from node config")
                        service_read_params, service_write_params = service_obj.get_service_params(service_config)
                        log.debug("Service params received with read properties: " + str(service_read_params) +
                                  " Service params received with write properties: " + str(service_write_params))
                        log.info("Getting node params...")
                    if not node_params:
                        node_params = node_object.get_node_params()
                        log.debug("Node params received: " + json.dumps(node_params))
                        print("Setting the OTA URL parameter...")

                    if not ota_start_status:
                        ota_start_status = service_obj.start_ota(node_object, node_params, service_name,
                                                        service_write_params, param_url_to_set)
                        log.debug("OTA status received: " + str(ota_start_status))
                        if not ota_start_status:
                            log.error("Failed to start OTA service...Exiting...")
                            break
                        print("Getting OTA Status...")
                    if not ota_status:
                        ota_status = service_obj.check_ota_status(node_object, service_name, service_read_params)
                        break
            except SSLError:
                log.error(SSLError())
                break
            except (NetworkError, RequestTimeoutError) as conn_err:
                print(conn_err)
                log.warn(conn_err)
            except Exception as node_init_err:
                log.error(node_init_err)
                break
            time.sleep(5)
            retries -= 1
            if retries:
                print("Retries left:", retries)
                log.info("Retries left: " + str(retries))

        if ota_status in [None, False]:
            log.error("OTA Upgrade...Failed")
            log.debug('OTA Upgrade...Failed '
                      'ota_status: ' + str(ota_status))
    except KeyError as key_err:
        log.error("Key Error: " + str(key_err))
    except Exception as ota_err:
        log.error(ota_err)
    return
