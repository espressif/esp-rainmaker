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

import uuid
import time
import sys

try:
    from rmaker_lib import session, node, configmanager
    from rmaker_lib.logger import log
except ImportError as err:
    print("Failed to import ESP Rainmaker library. " + str(err))
    raise err


def add_node(node_object):
    secret_key = str(uuid.uuid4())
    request_id = node_object.add_user_node_mapping(secret_key)
    return request_id, secret_key


def check_status(node_object, request_id):
    status = None
    while True:
        log.debug('Checking user-node association status.')
        try:
            status = node_object.get_mapping_status(request_id)
        except Exception as mapping_status_err:
            log.error(mapping_status_err)
            return
        else:
            log.debug('User-node association status ' + status)
            if status == 'requested':
                print('Checking User Node association status - Requested\n'
                      'Retrying...')
            elif status == 'confirmed':
                print('Checking User Node association status - Confirmed')
                return
            elif status == 'timedout':
                print('Checking User Node association status - Timeout')
                return
            elif status == 'discarded':
                print('Checking User Node association status - Discarded')
                return
        time.sleep(5)
    return


def test(vars=None):
    """
    Check user node mapping

    :param vars:
        `addnode` as key - Node ID of node to be mapped to user,\n
        defaults to `None`
    :type vars: dict

    """
    node_id = vars['addnode']
    if node_id is None or not vars.get('addnode'):
        print('Error: The following arguments are required: --addnode\n'
              'Check usage: rainmaker.py [-h]\n')
        sys.exit(0)
    node_object = node.Node(node_id, session.Session())
    request_id, secret_key = add_node(node_object)
    config = configmanager.Config()
    user_id = config.get_user_id()
    print('Use following command on node simulator or '
          'node CLI to confirm user node mapping:')
    print(" add-user " + user_id + " " + secret_key)
    print("---------------------------------------------------")
    print("RequestId for user node mapping request : " + request_id)
    check_status(node_object, request_id)
    return
