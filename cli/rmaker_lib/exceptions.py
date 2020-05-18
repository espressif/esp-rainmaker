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


class NetworkError(Exception):
    """ Raised when internet connection is not available """
    def __str__(self):
        return ('Could not connect. '
                'Please check your Internet connection.')


class RequestTimeoutError(Exception):
    """ Raised when HTTP Request times out """
    def __str__(self):
        return ('HTTP Request timed out. '
                'Please check your Internet connection.')


class InvalidJSONError(Exception):
    """ Raised for invalid JSON input """
    def __str__(self):
        return 'Invalid JSON received.'


class ExpiredSessionError(Exception):
    """ Raised when user session expires """
    def __str__(self):
        return 'User session is expired. Please login again.'


class InvalidConfigError(Exception):
    """ Raised for invalid configuration """
    def __str__(self):
        return 'Invalid configuration. Please login again.'


class InvalidUserError(Exception):
    """ Raised when config file does not exists """
    def __str__(self):
        return 'User not logged in. Please use login command.'


class AuthenticationError(Exception):
    """ Raised when user login fails """
    def __str__(self):
        return 'Login failed. Please try again'


class InvalidApiVersionError(Exception):
    """ Raised when current API version is not supported """
    def __str__(self):
        return 'API Version not supported. Please upgrade ESP Rainmaker CLI.'


class InvalidClassInput(Exception):
    """ Raised for invalid Session input """
    def __init__(self, input_arg, err_str):
        self.arg = input_arg
        self.err_str = err_str

    def __str__(self):
        return '{} {}'.format(self.err_str, self.arg)


class SSLError(Exception):
    """ Raised when invalid SSL certificate is passed """
    def __str__(self):
        return 'Unable to verify SSL certificate.'
