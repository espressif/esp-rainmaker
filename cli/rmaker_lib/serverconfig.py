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

VERSION = 'v1'

HOST = 'https://api.rainmaker.espressif.com/' + VERSION + '/'

CLIENT_ID = '60i6kac5f9rjuetqnq5mnmaqv6'

LOGIN_URL = 'https://rainmaker-signin-ui.s3.amazonaws.com/index.html?port='

TOKEN_URL = ('https://auth.rainmaker.espressif.com/'
             'oauth2/token')

REDIRECT_URL = 'https://rainmaker-login-ui.s3.amazonaws.com/welcome.html'

EXTERNAL_LOGIN_URL = (
                     'https://auth.rainmaker.espressif.com/'
                     'oauth2/authorize?&redirect_uri=' +
                     REDIRECT_URL + '&response_type=CODE&client_id=' +
                     CLIENT_ID + '&scope=aws.cognito.signin.user.'
                     'admin%20email%20openid%20phone%20profile&state=port:'
                     )
