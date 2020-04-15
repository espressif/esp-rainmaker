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

import webbrowser
import socket
import requests
from oauth2client import _helpers
from six.moves import BaseHTTPServer, http_client, urllib
import os
import sys

try:
    from rmaker_lib import serverconfig, configmanager
    from rmaker_lib.exceptions import SSLError, NetworkError
    from rmaker_lib.logger import log
except ImportError as err:
    print("Failed to import ESP Rainmaker library. " + str(err))
    raise err


class HttpdServer(BaseHTTPServer.HTTPServer):
    """
    A server to handle requests on localhost.

    Waits for a single request and parses the query parameters
    and then stops serving.
    """
    query_params = {}


class HttpdRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    """
    A HTTP handler of requests on localhost.
    """

    def do_GET(self):
        """
        Handle a GET request and
        writes the ESP Rainmaker Welcome HTML page(response)
        back to HTTP Client

        :raises Exception: If there is any File Handling Issue

        :return: None on Success and Failure
        :rtype: None
        """
        log.debug('Loading the welcome page after successful login.')
        self.send_response(http_client.OK)
        self.send_header('Content-type', 'text/html')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        parts = urllib.parse.urlparse(self.path)
        query = _helpers.parse_unique_urlencoded(parts.query)
        self.server.query_params = query
        index_file = os.path.join(os.path.expanduser('.'), 'html/welcome.html')

        try:
            with open(index_file, 'rb') as home_page:
                self.wfile.write(home_page.read())
        except Exception as file_err:
            log.error(file_err)
            sys.exit(1)

    def log_message(self, format, *args):
        """
        Do not log messages to the command prompt.
        """


def get_free_port():
    """
    Get Free port

    :return: port on Success
    :rtype: int
    """
    tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp.bind(('', 0))
    addr, port = tcp.getsockname()
    tcp.close()
    return port


def browser_login():
    """
    Opens browser with login url using Httpd Server.

    :raises Exception: If there is an HTTP issue while
                       logging in through browser

    :return: None on Success and Failure
    :rtype: None
    """
    log.info('Logging in through browser')
    server_instance = None
    for attempt in range(10):
        try:
            port = get_free_port()
            server_instance = HttpdServer(('localhost', port),
                                          HttpdRequestHandler)
            # Added timeout to handle keyboard interrupts for browser login.
            server_instance.timeout = 0.5
            break
        except socket.error as err:
            log.warn('Error %s. Port %s is not available.'
                     'Trying with next port.', err, port)

    if server_instance is None:
        log.error('Error: Could not launch local webserver.'
                  'Use --email option instead.')
        return

    url = serverconfig.LOGIN_URL + str(port) +\
        '&host_url=' + serverconfig.HOST + 'login' +\
        '&github_url=' + serverconfig.EXTERNAL_LOGIN_URL +\
        str(port)

    print('Opening browser window for login...')
    open_status = webbrowser.open(url)
    if open_status is False:
        log.error('Failed to open login page. Please try again.')
        return
    else:
        print('Use the browser for login. Press ctrl+C to abort.')
    log.debug('Web browser opened. Waiting for user login.')
    try:
        while True:
            server_instance.handle_request()
            if 'error' in server_instance.query_params:
                log.error('Authentication Error: "%s". Description: "%s" ' +
                          server_instance.query_params['error'] +
                          server_instance.query_params.ge('error_description'))
                return
            if 'code' in server_instance.query_params:
                log.debug('Login successful. Received authorization code.')
                code = server_instance.query_params['code']
                get_tokens(code)
                print('Login successful')
                return

            if 'id_token' in server_instance.query_params and \
                    'refresh_token' in server_instance.query_params:
                log.debug('Login successful.'
                          'Received idtoken and refresh token.')
                config_data = {}
                config_data['idtoken'] = server_instance.query_params[
                                                                'id_token'
                                                                ]
                config_data['refreshtoken'] = server_instance.query_params[
                                                                'refresh_token'
                                                                ]
                config_data['accesstoken'] = server_instance.query_params[
                                                                'access_token'
                                                                ]
                configmanager.Config().set_config(config_data)
                print('Login successful')
                return
    except Exception as browser_login_err:
        log.error(browser_login_err)


def get_tokens(code):
    """
    Get access token and set the config file after successful browser login.

    :raises Exception: If there is an HTTP issue in getting access token
    :raises SystemExit: If Exception is raised

    :return: None on Success and Failure
    :rtype: None
    """
    log.info('Getting access tokens using authorization code.')
    client_id = serverconfig.CLIENT_ID
    request_data = 'grant_type=authorization_code&client_id=' + client_id +\
                   '&code=' + code + '&redirect_uri=' +\
                   serverconfig.REDIRECT_URL

    request_header = {'content-type': 'application/x-www-form-urlencoded'}
    try:
        response = requests.post(url=serverconfig.TOKEN_URL,
                                 data=request_data,
                                 headers=request_header,
                                 verify=configmanager.CERT_FILE)
        response.raise_for_status()
    except requests.exceptions.SSLError:
        raise SSLError
    except requests.exceptions.ConnectionError:
        raise NetworkError
    except Exception as get_token_err:
        log.error(get_token_err)
        sys.exit(1)
    else:
        config_data = {}
        result = response.json()
        config_data['idtoken'] = result['id_token']
        config_data['refreshtoken'] = result['refresh_token']
        config_data['accesstoken'] = result['access_token']
        log.debug('Received access tokens using authorization code.')
        configmanager.Config().set_config(config_data)
    return
