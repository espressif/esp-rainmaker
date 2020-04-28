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
import sys

TRANSPORT_MODE_SOFTAP = 'softap'
MAX_HTTP_CONNECTION_RETRIES = 5
PROVISION_FAILURE_MSG = ('Provisioning Failed. Reset your board to factory '
                         'defaults and retry.')

try:
    from rmaker_tools.rmaker_prov.esp_rainmaker_prov import provision_device
    from rmaker_lib.logger import log
    from rmaker_lib import session, configmanager, node
    from rmaker_lib.exceptions import NetworkError, SSLError,\
        RequestTimeoutError
except ImportError as err:
    print("Failed to import ESP Rainmaker library.\n" + str(err))
    raise err


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
        log.error(PROVISION_FAILURE_MSG)
        return
    log.debug('Node ' + node_id + ' provisioned successfully.')

    print('------------------------------------------')
    input('Please ensure host machine is connected to internet and '
          'Press Enter to continue...')

    retries = MAX_HTTP_CONNECTION_RETRIES
    node_object = None
    while retries > 0:
        try:
            # If session is expired then to initialise the new session
            # internet connection is required.
            node_object = node.Node(node_id, session.Session())
        except SSLError:
            log.error(SSLError())
            break
        except (NetworkError, RequestTimeoutError) as conn_err:
            print(conn_err)
            log.warn(conn_err)
        except Exception as node_init_err:
            log.error(node_init_err)
            break
        else:
            break
        time.sleep(5)
        retries -= 1
        if retries:
            print("Retries left:", retries)
            log.info("Retries left: " + str(retries))

    if node_object is None:
        log.error('Initialising new session...Failed\n' +
                  '\n' + PROVISION_FAILURE_MSG)
        return

    print('\nAdding User-Node association')
    log.info("Adding User-Node association")

    retries = MAX_HTTP_CONNECTION_RETRIES
    request_id = None
    log.info('Sending User-Node Association Request...')
    while retries > 0:
        print('Sending User-Node Association Request...')
        try:
            request_id = node_object.add_user_node_mapping(secret_key)
        except SSLError:
            log.error(SSLError())
            break
        except (NetworkError, RequestTimeoutError) as conn_err:
            print(conn_err)
            log.warn(conn_err)
        except Exception as mapping_err:
            print(mapping_err)
            log.warn(mapping_err)
            break
        else:
            if request_id is not None:
                log.debug('User-Node mapping added successfully '
                          'with request_id '
                          + request_id)
                break

        retries -= 1
        if retries:
            print("Retries left:", retries)
            log.info("Retries left: " + str(retries))
            time.sleep(5)

    if request_id is None:
        log.error('User-Node Association Request...Failed\n' +
                  '\n' + PROVISION_FAILURE_MSG)
        return

    print('User-Node Association Request...Success')
    log.info('User-Node Association Request...Success')

    retries = MAX_HTTP_CONNECTION_RETRIES
    status = None
    log.info('Checking User-Node Association Status...')
    while retries > 0:
        print('Checking User-Node Association Status...')
        try:
            status = node_object.get_mapping_status(request_id)
        except SSLError:
            log.error(SSLError())
            break
        except (NetworkError, RequestTimeoutError) as conn_err:
            print(conn_err)
            log.warn(conn_err)
            status = None
        except Exception as mapping_status_err:
            print(mapping_status_err)
            log.warn(mapping_status_err)
            break
        else:
            if status == 'requested':
                print('User-Node Association Status - Requested'
                      '\n')
                log.debug('User-Node Association Status - Requested'
                          '\n')
            elif status == 'confirmed':
                print('User-Node Association Status - Confirmed'
                      '\nProvisioning was Successful.')
                log.debug('User-Node Association Status - Confirmed'
                          '\nProvisioning was Successful.')
                break
            elif status == 'timedout':
                print('User-Node Association Status - Timedout')
                log.debug('User-Node Association Status - Timedout')
                break
            elif status == 'discarded':
                print('User-Node Association Status - Discarded')
                log.debug('User-Node Association Status - Discarded')
                break
            else:
                log.debug('User-Node Association Status - ' + status)
                break

        if status not in ["requested"]:
            retries -= 1
            if retries:
                print("Retries left:", retries)
                log.info("Retries left: " + str(retries))
        time.sleep(5)

    if status not in ["confirmed"]:
        log.error('Checking User-Node Association Status...Failed.\n'
                  '\nCould not confirm User-Node Association Status. '
                  '\nPlease use cli command '
                  '`python3 rainmaker.py getnodes` to confirm.')
        return
    return
