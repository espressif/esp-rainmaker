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
import errno
import os
import base64
import time
import requests
import socket
from pathlib import Path
from os import path

from rmaker_lib import serverconfig
from rmaker_lib.exceptions import NetworkError,\
                                  InvalidConfigError,\
                                  InvalidUserError,\
                                  InvalidApiVersionError,\
                                  ExpiredSessionError,\
                                  SSLError,\
                                  RequestTimeoutError
from rmaker_lib.logger import log

CONFIG_DIRECTORY = '.espressif/rainmaker'
CONFIG_FILE = CONFIG_DIRECTORY + '/rainmaker_config.json'
HOME_DIRECTORY = '~/'
CURR_DIR = os.path.dirname(__file__)
CERT_FILE = CURR_DIR + '/../server_cert/server_cert.pem'


class Config:
    """
    Config class used to instantiate instances of config to
    perform various get/set configuration operations
    """
    def set_config(self, data, config_file=CONFIG_FILE):
        """
        Set the configuration file.

        :params data: Config Data to write to file
        :type data: dict

        :params config_file: Config filename to write config data to
        :type data: str

        :raises OSError: If there is an OS issue while creating new directory
                         for config file
        :raises Exception: If there is a File Handling error while saving
                           config to file

        :return: None on Success and Failure
        :rtype: None
        """
        log.info("Configuring config file.")
        file_dir = Path(path.expanduser(HOME_DIRECTORY + CONFIG_DIRECTORY))
        file = Path(path.expanduser(HOME_DIRECTORY) + config_file)
        if not file.exists():
            try:
                if not file_dir.exists():
                    log.debug('Config directory does not exist,'
                              'creating new directory.')
                    os.makedirs(path.expanduser(HOME_DIRECTORY) +
                                CONFIG_DIRECTORY)
            except OSError as set_config_err:
                log.error(set_config_err)
                if set_config_err.errno != errno.EEXIST:
                    raise set_config_err
        try:
            with open(path.join(path.expanduser(HOME_DIRECTORY),
                      config_file), 'w') as config_file:
                json.dump(data, config_file)
        except Exception as set_config_err:
            raise set_config_err
        log.info("Configured config file successfully.")

    def get_config(self, config_file=CONFIG_FILE):
        """
        Get the configuration details from config file.

        :params config_file: Config filename to read config data from
        :type data: str

        :raises Exception: If there is a File Handling error while reading
                           from config file

        :return:
            idtoken - Id Token from config saved\n
            refreshtoken - Refresh Token from config saved\n
            accesstoken - Access Token from config saved\n
        :rtype: str
        """
        file = Path(path.expanduser(HOME_DIRECTORY) + config_file)
        if not file.exists():
            raise InvalidUserError
        try:
            with open(path.join(path.expanduser(HOME_DIRECTORY),
                      config_file), 'r') as config_file:
                data = json.load(config_file)
                idtoken = data['idtoken']
                refresh_token = data['refreshtoken']
                access_token = data['accesstoken']
        except Exception as get_config_err:
            raise get_config_err
        return idtoken, refresh_token, access_token

    def get_binary_config(self, config_file=CONFIG_FILE):
        """
        Get the configuration details from binary config file.

        :params config_file: Config filename to read config data from
        :type data: str

        :raises Exception: If there is a File Handling error while reading
                           from config file

        :return: Config data read from file on Success, None on Failure
        :rtype: str | None
        """
        file = Path(path.expanduser(HOME_DIRECTORY) + config_file)
        if not file.exists():
            return None
        try:
            with open(file, 'rb') as cfg_file:
                data = cfg_file.read()
                return data
        except Exception as get_config_err:
            raise get_config_err
        return

    def update_config(self, access_token, id_token):
        """
        Update the configuration file.

        :params access_token: Access Token to update in config file
        :type access_token: str

        :params id_token: Id Token to update in config file
        :type id_token: str

        :raises OSError: If there is an OS issue while creating new directory
                         for config file
        :raises Exception: If there is a FILE Handling error while reading
                           from/writing config to file

        :return: None on Success and Failure
        :rtype: None
        """
        file = Path(path.expanduser(HOME_DIRECTORY) + CONFIG_FILE)
        if not file.exists():
            try:
                os.makedirs(path.expanduser(HOME_DIRECTORY) + CONFIG_DIRECTORY)
            except OSError as set_config_err:
                if set_config_err.errno != errno.EEXIST:
                    raise set_config_err
        try:
            with open(path.join(path.expanduser(HOME_DIRECTORY),
                      CONFIG_FILE), 'r') as config_file:
                config_data = json.load(config_file)
                config_data['accesstoken'] = access_token
                config_data['idtoken'] = id_token
            with open(path.join(path.expanduser(HOME_DIRECTORY),
                      CONFIG_FILE), 'w') as config_file:
                json.dump(config_data, config_file)
        except Exception as set_config_err:
            raise set_config_err

    def get_token_attribute(self, attribute_name, is_access_token=False):
        """
        Get access token attributes.

        :params attribute_name: Attribute Name
        :type attribute_name: str

        :params is_access_token: Is Access Token
        :type is_access_token: bool

        :raises InvalidConfigError: If there is an error in the config
        :raises Exception: If there is a File Handling error while reading
                           from/writing config to file

        :return: Attribute Value on Success, None on Failure
        :rtype: int | str | None
        """
        if is_access_token:
            log.debug('Getting access token for attribute ' + attribute_name)
            _, _, token = self.get_config()
        else:
            log.debug('Getting idtoken for attribute ' + attribute_name)
            token, _, _ = self.get_config()
        token_payload = token.split('.')[1]
        if len(token_payload) % 4:
            token_payload += '=' * (4 - len(token_payload) % 4)
        try:
            str_token_payload = base64.b64decode(token_payload).decode("utf-8")
            attribute_value = json.loads(str_token_payload)[attribute_name]
        except Exception:
            raise InvalidConfigError
        if attribute_value is None:
            raise InvalidConfigError
        return attribute_value

    def get_access_token(self):
        """
        Get Access Token for User

        :raises InvalidConfigError: If there is an issue in getting config
                                    from file

        :return: Access Token on Success
        :rtype: str
        """
        _, _, access_token = self.get_config()
        if access_token is None:
            raise InvalidConfigError
        if self.__is_valid_token() is False:
            print('Previous Session expired. Initialising new session...')
            log.info('Previous Session expired. Initialising new session...')
            username = self.get_token_attribute('email')
            refresh_token = self.get_refresh_token()
            access_token, id_token = self.__get_new_token(username,
                                                          refresh_token)
            self.update_config(access_token, id_token)
            print('Previous Session expired. Initialising new session...'
                  'Success')
            log.info('Previous Session expired. Initialising new session...'
                     'Success')
        return access_token

    def get_user_id(self):
        """
        Get User Id

        :return: Attribute value for attribute name passed
        :rtype: str
        """
        return self.get_token_attribute('custom:user_id')

    def get_refresh_token(self):
        """
        Get Refresh Token

        :raises InvalidApiVersionError: If current API version is not supported

        :return: Refresh Token
        :rtype: str
        """
        if self.__is_valid_version() is False:
            raise InvalidApiVersionError
        _, refresh_token, _ = self.get_config()
        return refresh_token

    def __is_valid_token(self):
        """
        Check if access token is valid i.e. login session is still active
        or session is expired

        :return True on Success and False on Failure
        :rtype: bool
        """
        log.info("Checking for session timeout.")
        exp_timestamp = self.get_token_attribute('exp', is_access_token=True)
        current_timestamp = int(time.time())
        if exp_timestamp > current_timestamp:
            return True
        return False

    def __is_valid_version(self):
        """
        Check if API Version is valid

        :raises NetworkError: If there is a network connection issue during
                              HTTP request for getting version
        :raises Exception: If there is an HTTP issue or JSON format issue in
                           HTTP response

        :return: True on Success, False on Failure
        :rtype: bool
        """
        socket.setdefaulttimeout(10)
        log.info("Checking for supported version.")
        path = 'apiversions'
        request_url = serverconfig.HOST.split(serverconfig.VERSION)[0] + path
        try:
            log.debug("Version check request url : " + request_url)
            response = requests.get(url=request_url, verify=CERT_FILE,
                                    timeout=(5.0, 5.0))
            log.debug("Version check response : " + response.text)
            response.raise_for_status()
        except requests.exceptions.SSLError:
            raise SSLError
        except requests.exceptions.Timeout:
            raise RequestTimeoutError
        except requests.exceptions.ConnectionError:
            raise NetworkError
        except Exception as ver_err:
            raise ver_err

        try:
            response = json.loads(response.text)
        except Exception as json_decode_err:
            raise json_decode_err

        if 'supported_versions' in response:
            supported_versions = response['supported_versions']
            if serverconfig.VERSION in supported_versions:
                supported_versions.sort()
                latest_version = supported_versions[len(supported_versions)
                                                    - 1]
                if serverconfig.VERSION < latest_version:
                    print('Please check the updates on GitHub for newer'
                          'functionality enabled by ' + latest_version +
                          ' APIs.')
                return True
        return False

    def __get_new_token(self, username, refresh_token):
        """
        Get new token for User Login Session

        :raises NetworkError: If there is a network connection issue during
                              HTTP request for getting token
        :raises Exception: If there is an HTTP issue or JSON format issue in
                           HTTP response

        :return: accesstoken and idtoken on Success, None on Failure
        :rtype: str | None

        """
        socket.setdefaulttimeout(10)
        log.info("Extending user login session.")
        path = 'login'
        request_payload = {
            'user_name':  username,
            'refreshtoken': refresh_token
            }

        request_url = serverconfig.HOST + path
        try:
            log.debug("Extend session url : " + request_url)
            response = requests.post(url=request_url,
                                     data=json.dumps(request_payload),
                                     verify=CERT_FILE,
                                     timeout=(5.0, 5.0))
            response.raise_for_status()
            log.debug("Extend session response : " + response.text)
        except requests.exceptions.SSLError:
            raise SSLError
        except requests.exceptions.ConnectionError:
            raise NetworkError
        except requests.exceptions.Timeout:
            raise RequestTimeoutError
        except Exception:
            raise ExpiredSessionError

        try:
            response = json.loads(response.text)
        except Exception:
            raise ExpiredSessionError

        if 'accesstoken' in response and 'idtoken' in response:
            log.info("User session extended successfully.")
            return response['accesstoken'], response['idtoken']
        return None

    def check_user_creds_exists(self):
        '''
        Check if user creds exist
        '''
        curr_login_creds_file = os.path.expanduser(HOME_DIRECTORY + CONFIG_FILE)
        if os.path.exists(curr_login_creds_file):
            return curr_login_creds_file
        else:
            return False

    def get_input_to_end_session(self, email_id):
        '''
        Get input(y/n) from user to end current session
        '''
        while True:
            user_input = input("This will end your current session for {}. Do you want to continue (Y/N)? :".format(email_id))
            if user_input not in ["Y", "y", "N", "n"]:
                print("Please provide Y/N only")
                continue
            elif user_input in ["N", "n"]:
                return False
            else:
                break
        return True

    def remove_curr_login_creds(self, curr_creds_file=None):
        '''
        Remove current login creds
        '''
        log.info("Removing current login creds")
        if not curr_creds_file:
            curr_creds_file = os.path.expanduser(HOME_DIRECTORY + CONFIG_FILE)
        try:
            os.remove(curr_creds_file)
            log.info("Previous login session ended. Removing current login creds...Success...")
            return True
        except Exception as e:
            log.debug("Removing current login creds from path {}. Failed: {}".format(curr_creds_file, e))
        return None