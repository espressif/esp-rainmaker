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
from rmaker_lib import serverconfig, configmanager
from rmaker_lib import node
from rmaker_lib.exceptions import NetworkError, InvalidConfigError, SSLError,\
    RequestTimeoutError
from rmaker_lib.logger import log


class Session:
    """
    Session class for logged in user.
    """
    def __init__(self):
        """
        Instantiate session for logged in user.
        """
        config = configmanager.Config()
        log.info("Initialising session for user " +
                 config.get_token_attribute('email'))
        self.id_token = config.get_access_token()
        if self.id_token is None:
            raise InvalidConfigError
        self.request_header = {'Content-Type': 'application/json',
                               'Authorization': self.id_token}

    def get_nodes(self):
        """
        Get list of all nodes associated with the user.

        :raises NetworkError: If there is a network connection issue
                              while getting nodes associated with user
        :raises Exception: If there is an HTTP issue while getting nodes

        :return: Nodes associated with user on Success
        :rtype: dict
        """
        log.info("Getting nodes associated with the user.")
        path = 'user/nodes'
        getnodes_url = serverconfig.HOST + path
        try:
            log.debug("Get nodes request url : " + getnodes_url)
            response = requests.get(url=getnodes_url,
                                    headers=self.request_header,
                                    verify=configmanager.CERT_FILE)
            log.debug("Get nodes request response : " + response.text)
            response.raise_for_status()
        except requests.exceptions.SSLError:
            raise SSLError
        except requests.exceptions.ConnectionError:
            raise NetworkError
        except Exception:
            raise Exception(response.text)

        node_map = {}
        for nodeid in json.loads(response.text)['nodes']:
            node_map[nodeid] = node.Node(nodeid, self)
        log.info("Received nodes for user successfully.")
        return node_map

    def get_mqtt_host(self):
        """
        Get the MQTT Host endpoint.

        :raises NetworkError: If there is a network connection issue
                              while getting MQTT Host endpoint
        :raises Exception: If there is an HTTP issue while getting MQTT host
                           or JSON format issue in HTTP response

        :return: MQTT Host on Success, None on Failure
        :rtype: str | None
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
        except requests.exceptions.ConnectionError:
            raise NetworkError
        except Exception as mqtt_host_err:
            raise mqtt_host_err

        try:
            response = json.loads(response.text)
        except Exception as json_decode_err:
            raise json_decode_err

        if 'mqtt_host' in response:
            log.info("Received MQTT Host endpoint successfully.")
            return response['mqtt_host']
        return None

    def get_user_details(self):
        """
        Get details of current logged-in user
        
        :raises SSLError: If there is an SSL issue
        :raises HTTPError: If the HTTP response is an HTTPError
        :raises NetworkError: If there is a network connection issue
        :raises Timeout: If there is a timeout issue
        :raises RequestException: If there is an issue during
                                  the HTTP request
        :raises Exception: If there is an HTTP issue while getting user details
                           or JSON format issue in HTTP response

        :return: HTTP response on Success
        :rtype: dict
        """
        socket.setdefaulttimeout(10)
        log.info('Getting details of current logged-in user')
        version = serverconfig.VERSION
        path = '/user'
        getdetails_url = serverconfig.HOST.rstrip('/') + path
        try:
            log.debug("Get user details request url : " + getdetails_url)
            response = requests.get(url=getdetails_url,
                                    headers=self.request_header,
                                    verify=configmanager.CERT_FILE,
                                    timeout=(5.0, 5.0))
            log.debug("Get user details request response : " + response.text)

        except requests.exceptions.HTTPError as http_err:
            log.debug(http_err)
            return json.loads(http_err.response.text)
        except requests.exceptions.SSLError:
            raise SSLError
        except requests.exceptions.ConnectionError:
            raise NetworkError
        except requests.exceptions.Timeout as time_err:
            log.debug(time_err)
            raise RequestTimeoutError
        except requests.exceptions.RequestException as req_err:
            log.debug(req_err)
            raise req_err
        except Exception:
            raise Exception(response.text)
        
        log.info("Received user details successfully.")
        try:
            return json.loads(response.text)
        except Exception as resp_err:
            raise resp_err

    def logout(self):
        """
        Logout current logged-in user
        
        :raises SSLError: If there is an SSL issue
        :raises HTTPError: If the HTTP response is an HTTPError
        :raises NetworkError: If there is a network connection issue
        :raises Timeout: If there is a timeout issue
        :raises RequestException: If there is an issue during
                                  the HTTP request
        :raises Exception: If there is an HTTP issue while logging out
                           or JSON format issue in HTTP response

        :return: HTTP response on Success
        :rtype: dict
        """
        socket.setdefaulttimeout(10)
        log.info('Logging out current logged-in user')
        version = serverconfig.VERSION
        path = '/logout'
        # Logout only from current session
        query_params = 'logout_all=false'
        logout_url = serverconfig.HOST.rstrip('/') + path + '?' + query_params
        try:
            log.debug("Logout request url : " + logout_url)
            log.debug("Logout headers: {}".format(self.request_header))
            response = requests.post(url=logout_url,
                                     headers=self.request_header,
                                     verify=configmanager.CERT_FILE,
                                     timeout=(5.0, 5.0))
            log.debug("Logout request response : " + response.text)

        except requests.exceptions.HTTPError as http_err:
            log.debug(http_err)
            return json.loads(http_err.response.text)
        except requests.exceptions.SSLError:
            raise SSLError
        except requests.exceptions.ConnectionError:
            raise NetworkError
        except requests.exceptions.Timeout as time_err:
            log.debug(time_err)
            raise RequestTimeoutError
        except requests.exceptions.RequestException as req_err:
            log.debug(req_err)
            raise req_err
        except Exception:
            raise Exception(response.text)

        try:
            log.info("Logout API call successful")
            return json.loads(response.text)
        except Exception as resp_err:
            raise resp_err