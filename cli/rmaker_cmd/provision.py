# Copyright 2020 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License');
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

import uuid
import time
from packaging import version
import sys

try:
    from rmaker_lib.logger import log
    from rmaker_lib import session, configmanager, node
    from rmaker_lib.exceptions import NetworkError, SSLError
except ImportError as err:
    print("Failed to import ESP Rainmaker library. " + str(err))
    raise err

MINIMUM_PROTOBUF_VERSION = '3.10.0'
TRANSPORT_MODE_SOFTAP = 'softap'
MAX_HTTP_CONNECTION_RETRIES = 5
PROVISION_FAILURE_MSG = 'Provisioning Failed. Reset your board to factory'
'defaults and retry.'


def provision(vars=None):
    """
    Provisioning of the node.

    :raises NetworkError: If there is a network connection issue
                          during provisioning
    :raises Exception: If there is an HTTP issue during provisioning

    :param vars: `pop` - Proof of Possession of the node, defaults to `None`
    :type vars: dict

    :return: None on Success and Failure
    :rtype: None
    """
    try:
        from rmaker_tools.rmaker_prov.esp_rainmaker_prov\
             import provision_device
    except ImportError as err:
        import google.protobuf
        if version.parse(google.protobuf.__version__)\
                < version.parse(MINIMUM_PROTOBUF_VERSION):
            log.warn('Package protobuf does not satisfy\
                     the minimum required version.\n'
                     'Minimum required version is ' + MINIMUM_PROTOBUF_VERSION)
        else:
            log.error('Provisioning failed due to import error.', err)
            raise err
    log.info('Provisioning the node.')
    secret_key = str(uuid.uuid4())
    pop = vars['pop']
    try:
        config = configmanager.Config()
        userid = config.get_user_id()
        log.debug('User session is initialized for the user ' + userid)
    except Exception as get_user_id_err:
        log.error(get_user_id_err)
        sys.exit(1)
    try:
        input('Please connect to the wifi PROV_XXXXXX and '
              'Press Enter to continue...')
    except Exception:
        print("Exiting...")
        sys.exit(0)

    node_id = provision_device(TRANSPORT_MODE_SOFTAP, pop, userid, secret_key)
    if node_id is None:
        print(PROVISION_FAILURE_MSG)
        return
    log.debug('Node ' + node_id + ' provisioned successfully.')

    print('------------------------------------------')
    input('Please ensure host machine is connected to internet and'
          'Press Enter to continue...')
    print('Adding User-Node association...')
    retries = MAX_HTTP_CONNECTION_RETRIES
    node_object = None
    while retries > 0:
        try:
            # If session is expired then to initialise the new session
            # internet connection is required.
            node_object = node.Node(node_id, session.Session())
        except SSLError:
            log.error(SSLError())
            print(PROVISION_FAILURE_MSG)
            return
        except NetworkError:
            time.sleep(5)
            log.warn("Session is expired. Initialising new session.")
            pass
        except Exception as node_init_err:
            log.error(node_init_err)
            print(PROVISION_FAILURE_MSG)
            return
        else:
            break
        retries -= 1

    if node_object is None:
        print('Please check the internet connectivity.')
        print(PROVISION_FAILURE_MSG)
        return
    retries = MAX_HTTP_CONNECTION_RETRIES
    request_id = None
    while retries > 0:
        try:
            log.debug('Adding user-node association.')
            request_id = node_object.add_user_node_mapping(secret_key)
        except SSLError:
            log.error(SSLError())
            print(PROVISION_FAILURE_MSG)
            return
        except Exception as user_node_mapping_err:
            print('Sending User-Node association request to '
                  'ESP RainMaker Cloud - Failed\nRetrying...')
            log.warn(user_node_mapping_err)
            pass
        else:
            if request_id is not None:
                log.debug('User-node mapping added successfully'
                          'with request_id'
                          + request_id)
                break
        time.sleep(5)
        retries -= 1

    if request_id is None:
        print('Sending User-Node association request to'
              'ESP RainMaker Cloud - Failed')
        print(PROVISION_FAILURE_MSG)
        return
    print('Sending User-Node association request to'
          'ESP RainMaker Cloud - Successful')

    status = None
    while True:
        log.debug('Checking user-node association status.')
        try:
            status = node_object.get_mapping_status(request_id)
        except SSLError:
            log.error(SSLError())
            print(PROVISION_FAILURE_MSG)
            return
        except Exception as mapping_status_err:
            log.warn(mapping_status_err)
            pass
        else:
            log.debug('User-node association status ' + status)
            if status == 'requested':
                print('Checking User Node association status -'
                      'Requested\nRetrying...')
            elif status == 'confirmed':
                print('Checking User Node association status - Confirmed')
                print('Provisioning was Successful.')
                return
            elif status == 'timedout':
                print('Checking User Node association status - Timeout')
                print(PROVISION_FAILURE_MSG)
                return
            elif status == 'discarded':
                print('Checking User Node association status - Discarded')
                print(PROVISION_FAILURE_MSG)
                return
        time.sleep(5)

    if status is None:
        print(PROVISION_FAILURE_MSG)
        print('Checking User Node association status failed. '
              'Please check the internet connectivity.')
        return
    return
