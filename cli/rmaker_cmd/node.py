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
import requests
from pathlib import Path

try:
    from rmaker_lib import session, node, serverconfig, configmanager
    from rmaker_lib.exceptions import NetworkError, InvalidJSONError, SSLError
    from rmaker_lib.logger import log
except ImportError as err:
    print("Failed to import ESP Rainmaker library. " + str(err))
    raise err


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
    return


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
    data = vars['data']
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
    except Exception as get_params_err:
        log.error(get_params_err)
    else:
        if params is None:
            log.error('Node does not have updated its state.')
            return
        print(json.dumps(params, indent=4))
    return


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
        log.debug("Get MQTT Host resonse : " + response.text)
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
