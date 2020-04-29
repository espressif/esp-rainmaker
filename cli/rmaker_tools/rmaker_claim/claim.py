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

import os
from os import path
from pathlib import Path
from io import StringIO
import sys
from sys import exit
import time
import requests
import json
import binascii
from types import SimpleNamespace
from rmaker_lib.logger import log
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, hmac, serialization
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import rsa
from rmaker_tools.rmaker_claim.claim_config import \
    CLAIM_INITIATE_URL, CLAIM_VERIFY_URL
from rmaker_lib import session, configmanager
from rmaker_lib.exceptions import SSLError
from rmaker_cmd import node

if os.getenv('IDF_PATH'):
    sys.path.insert(0, os.path.join(os.getenv('IDF_PATH'),
                                    'components',
                                    'esptool_py',
                                    'esptool'))
    sys.path.insert(0, os.path.join(os.getenv('IDF_PATH'),
                                    'components',
                                    'nvs_flash',
                                    'nvs_partition_generator'))
    import esptool
    import nvs_partition_gen
else:
    log.error("Please set the IDF_PATH environment variable.")
    exit(0)

CERT_FILE = './server_cert/server_cert.pem'

# List of efuse blocks
#
# Name, Index, Read Address, Read Protect Bit, Write Protect Bit
BLOCKS = [
    ("BLOCK_SYS_DATA", 2, 0x3f41a05c, None, 21)
]


def flash_bin_onto_node(port, esptool, bin_to_flash):
    """
    Flash binary onto node

    :param port: Serial Port
    :type port: str

    :param esptool: esptool module
    :type esptool: module

    :param bin_to_flash: Filname of binary to flash
    :type bin_to_flash: str

    :raises Exception: If there is any issue when flashing binary onto node

    :return: None on Success
    :rtype: None
    """
    try:
        command = ['--port', port, 'write_flash', '0x340000', bin_to_flash]
        esptool.main(command)
    except Exception as err:
        log.error(err)
        sys.exit(1)


def get_node_platform_and_mac(esptool, port):
    """
    Get Node Platform and Mac Addres from device

    :param esptool: esptool module
    :type esptool: module

    :param port: Serial Port
    :type port: str

    :return: Node Platform and Mac Address on Success
    :rtype: str
    """
    sys.stdout = mystdout = StringIO()
    command = ['--port', port, 'chip_id']
    log.info("Running esptool command to get node\
    platform and mac from device")
    esptool.main(command)
    sys.stdout = sys.__stdout__
    # Finding chip type from output.
    node_platform = next(filter(lambda line: 'Detecting chip type' in line,
                                mystdout.getvalue().splitlines()))
    # Finds the first occurence of the line
    # with the MAC Address from the output.
    mac = next(filter(lambda line: 'MAC: ' in line,
                      mystdout.getvalue().splitlines()))
    mac_addr = mac.split('MAC: ')[1].replace(':', '').upper()
    platform = node_platform.split()[-1].lower()
    return platform, mac_addr


def get_secret_key(port, esptool):
    """
    Generate Secret Key

    :param port: Serial Port
    :type port: str

    :param esptool: esptool module
    :type esptool: module

    :return: Secret Key on Success
    :rtype: str
    """
    esp = esptool.ESP32ROM(port)
    esp.connect('default_reset')
    for (name, idx, read_addr, _, _) in BLOCKS:
        addrs = range(read_addr, read_addr + 32, 4)
        secret = "".join(["%08x" % esp.read_reg(addr) for addr in addrs[0:4]])
        secret = secret[6:8]+secret[4:6]+secret[2:4]+secret[0:2] +\
            secret[14:16]+secret[12:14]+secret[10:12]+secret[8:10] +\
            secret[22:24]+secret[20:22]+secret[18:20]+secret[16:18] +\
            secret[30:32]+secret[28:30]+secret[26:28]+secret[24:26]
    return secret


def gen_hmac_challenge_resp(secret_key, hmac_challenge):
    """
    Generate HMAC Challenge Response

    :param secret_key: Secret Key to generate HMAC Challenge Response
    :type secret_key: str

    :param hmac_challenge: HMAC Challenge received in
                           esp32s2 claim initate response
    :type hmac_challenge: str

    :return: HMAC Challenge Response on Success
    :rtype: str
    """
    h = hmac.HMAC(bytes.fromhex(secret_key),
                  hashes.SHA512(),
                  backend=default_backend())
    h.update(bytes(hmac_challenge, 'utf-8'))
    hmac_challenge_response = binascii.hexlify(h.finalize()).decode()
    return hmac_challenge_response


def gen_host_csr(private_key, common_name=None):
    """
    Generate Host CSR

    :param private_key: RSA Private Key to sign CSR
    :type private_key: RSA Key Object

    :param common_name: Common Name used in subject name of certificate,
                        defaults to None
    :type common_name: str|None

    :return: CSR on Success, None on Failure
    :rtype: str|None
    """
    # Generate CSR on host
    builder = x509.CertificateSigningRequestBuilder()
    builder = builder.subject_name(x509.Name([
        x509.NameAttribute(NameOID.COMMON_NAME, common_name),
    ]))
    builder = builder.add_extension(
        x509.BasicConstraints(ca=False, path_length=None), critical=True,
    )
    request = builder.sign(
        private_key, hashes.SHA256(), default_backend()
    )
    if isinstance(request, x509.CertificateSigningRequest) is not True:
        print("Certificate Signing Request could not be created")
        return None

    csr = request.public_bytes(serialization.Encoding.PEM).decode("utf-8")
    return csr


def create_files_of_claim_info(dest_filedir, node_id, private_key, node_cert,
                               endpointinfo, node_info_csv):
    """
    Create files with claiming details

    :param dest_filedir: Destination File Directory
    :type port: str

    :param node_id: Node Id (data) to write to `node.info` file
    :type port: str

    :param private_key: Private Key (data) to write to `node.key` file
    :type port: bytes

    :param node_cert: Node Certificate (data) to write to `node.crt` file
    :type port: str

    :param endpointinfo: MQTT endpoint (data) to write to `endpoint.info` file
    :type port: str

    :param node_info_csv: List of output csv file details (node information)
                          to write to `node_info.csv` file
    :type port: list

    :raises Exception: If there is any issue when writing to file

    :return: None on Success
    :rtype: None
    """
    try:
        log.debug("Writing node info at location: " + dest_filedir +
                  'node.info')
        # Create files for each claim data - node info, node key,
        # node cert, endpoint info
        with open(dest_filedir+'node.info', 'w+') as info_file:
            info_file.write(node_id)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'node.key')
        with open(dest_filedir+'node.key', 'wb+') as info_file:
            info_file.write(private_key)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'node.crt')
        with open(dest_filedir+'node.crt', 'w+') as info_file:
            info_file.write(node_cert)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'endpoint.info')
        with open(dest_filedir+'endpoint.info', 'w+') as info_file:
            info_file.write(endpointinfo)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'node_info.csv')
        with open(dest_filedir+'node_info.csv', 'w+') as info_file:
            for input_line in node_info_csv:
                info_file.write(input_line)
                info_file.write("\n")
    except Exception as file_error:
        raise file_error


def claim(port):
    """
    Claim the node connected to the given serial port
    (Get cloud credentials)

    :param port: Serial Port
    :type port: str

    :raises Exception: If there is an HTTP issue while claiming
            SSLError: If there is an issue in SSL certificate validation
            ConnectionError: If there is network connection issue

    :return: None on Success
    :rtype: None
    """
    try:
        node_id = None
        node_info = None
        hmac_challenge = None
        claim_verify_data = None
        claim_initiate_url = CLAIM_INITIATE_URL
        claim_verify_url = CLAIM_VERIFY_URL
        private_key = None
        curr_claim_data = None
        user_whitelist_err_msg = ('user is not allowed to claim esp32 device.'
                                  ' please contact administrator')

        config = configmanager.Config()
        userid = config.get_user_id()

        creds_dir = Path(path.expanduser(
            str(Path(path.expanduser(configmanager.HOME_DIRECTORY))) +
            '/' +
            str(Path(path.expanduser(
                configmanager.CONFIG_DIRECTORY))) +
            '/claim_data/' +
            userid
            ))
        if not creds_dir.exists():
            os.makedirs(path.expanduser(creds_dir))
            log.debug("Creating new directory " + str(creds_dir))

        print("\nClaiming process started. This may take time.")
        log.info("Claiming process started. This may take time.")

        node_platform, mac_addr = get_node_platform_and_mac(esptool, port)
        print("Node platform detected is: ", node_platform)
        print("MAC address is: ", mac_addr)
        log.debug("MAC address received: " + mac_addr)
        log.debug("Node platform detected is: " + node_platform)

        log.info("Creating session")
        curr_session = session.Session()
        header = curr_session.request_header

        start = time.time()

        mac_dir = Path(path.expanduser(str(creds_dir) + '/' + mac_addr))
        if not mac_dir.exists():
            os.makedirs(path.expanduser(mac_dir))
            log.debug("Creating new directory " + str(mac_dir))

        output_bin_filename = mac_addr + '.bin'
        mac_dir_path = str(mac_dir) + '/'

        # Set values
        dest_filedir = mac_dir_path

        # Set csv file data
        node_info_csv = [
                         'key,type,encoding,value',
                         'rmaker_creds,namespace,,',
                         'node_id,file,binary,' +
                         dest_filedir + 'node.info',
                         'mqtt_host,file,binary,' +
                         dest_filedir + 'endpoint.info',
                         'client_cert,file,binary,' +
                         dest_filedir + 'node.crt',
                         'client_key,file,binary,' +
                         dest_filedir + 'node.key'
                        ]

        # Generate nvs args to be sent to NVS Partition Utility
        nvs_args = SimpleNamespace(input=dest_filedir+'node_info.csv',
                                   output=output_bin_filename,
                                   size='0x6000',
                                   outdir=dest_filedir,
                                   version=2)

        # Set config mac addr path
        mac_addr_config_path = str(Path(path.expanduser(
            configmanager.CONFIG_DIRECTORY))) + '/claim_data/' +\
            userid +\
            '/' +\
            mac_addr +\
            '/' + output_bin_filename

        # Check if claim data for node exists in CONFIG directory
        log.debug("Checking if claim data for node exists in directory: " +
                  configmanager.HOME_DIRECTORY +
                  configmanager.CONFIG_DIRECTORY)
        curr_claim_data = configmanager.Config().get_binary_config(
            config_file=mac_addr_config_path)
        if curr_claim_data:
            print("\nClaiming data already exists at location: " +
                  dest_filedir)
            log.debug("Claiming data already exists at location: " +
                      dest_filedir)
            log.info("Using existing claiming data")
            print("Using existing claiming data")
            # Flashing existing binary onto node
            print("\nFlashing existing binary onto node\n")
            log.info("Flashing existing binary onto node")
            flash_bin_onto_node(port, esptool, dest_filedir +
                                output_bin_filename)
            log.info("Binary flashed onto node")
            return

        # Generate Key
        log.info("Generate RSA key")
        private_key = rsa.generate_private_key(
                        public_exponent=65537,
                        key_size=2048,
                        backend=default_backend()
        )

        log.info("RSA Private Key generated")
        # Set Claim initiate request data
        claim_initiate_data = {"mac_addr": mac_addr, "platform": node_platform}
        claim_init_enc_data = str(claim_initiate_data).replace(
            "'", '"')

        print("Claim initiate started")
        # Sign the CSR using the CA
        try:
            # Claim Initiate Request
            log.info("Claim initiate started. Sending claim/initiate POST\
                     request")
            log.debug("Claim Initiate POST Request: url: " +
                      claim_initiate_url + "data: " +
                      str(claim_init_enc_data) +
                      "headers: " + str(header) +
                      "verify: " + CERT_FILE)
            claim_initiate_response = requests.post(url=claim_initiate_url,
                                                    data=claim_init_enc_data,
                                                    headers=header,
                                                    verify=CERT_FILE)
            if claim_initiate_response.status_code != 200:
                log.error("Claim initiate failed.\n" +
                          claim_initiate_response.text)
                exit(0)

            print("Claim initiate done")
            log.debug("Claim Initiate POST Response: status code: " +
                      str(claim_initiate_response.status_code) +
                      " and response text: " + claim_initiate_response.text)
            log.info("Claim initiate done")
            # Get data from response depending on node_platform
            if node_platform == "esp32":
                # Generate CSR with common_name=node_id received in response
                node_id = str(json.loads(
                    claim_initiate_response.text)['node_id'])
                print("Generating CSR")
                log.info("Generating CSR")
                csr = gen_host_csr(private_key, common_name=node_id)
                if not csr:
                    raise Exception("CSR Not Generated. Claiming Failed")
                log.info("CSR generated")
                claim_verify_data = {"csr": csr}
                # Save node id as node info to use while saving claim data
                # in csv file
                node_info = node_id
            else:
                auth_id = str(json.loads(
                    claim_initiate_response.text)['auth_id'])
                hmac_challenge = str(json.loads(
                    claim_initiate_response.text)['challenge'])
                print("Generating CSR")
                log.info("Generating CSR")
                csr = gen_host_csr(private_key, common_name=mac_addr)
                if not csr:
                    raise Exception("CSR Not Generated. Claiming Failed")
                log.info("CSR generated")
                log.info("Getting secret key from device")
                secret_key = get_secret_key(port, esptool)
                log.info("Getting secret key from device")
                log.info("Generating hmac challenge response")
                hmac_challenge_response = gen_hmac_challenge_resp(
                    secret_key,
                    hmac_challenge)
                hmac_challenge_response = hmac_challenge_response.strip('\n')
                log.debug("Secret Key generated: " + secret_key)
                log.debug("HMAC Challenge Response: " +
                          hmac_challenge_response)
                claim_verify_data = {"auth_id":
                                     auth_id,
                                     "challenge_response":
                                     hmac_challenge_response,
                                     "csr":
                                     csr}
                # Save node id as node info to use while saving claim data
                # in csv file
                node_info = mac_addr

            claim_verify_enc_data = str(claim_verify_data).replace(
                "'", '"')
            log.debug("Claim Verify POST Request: url: " + claim_verify_url +
                      "data: " + str(claim_verify_enc_data) + "headers: " +
                      str(header) + "verify: " + CERT_FILE)
            claim_verify_response = requests.post(url=claim_verify_url,
                                                  data=claim_verify_enc_data,
                                                  headers=header,
                                                  verify=CERT_FILE)
            if claim_verify_response.status_code != 200:
                claim_verify_response_json = json.loads(
                    claim_verify_response.text.lower())
                if (claim_verify_response_json["description"] in
                        user_whitelist_err_msg):
                    log.error('Claim verification failed.\n' +
                              claim_verify_response.text)
                    print('\nYour account isn\'t whitelisted for ESP32.'
                          ' Please send your registered email address to'
                          ' esp-rainmaker-admin@espressif.com for whitelisting'
                          )
                else:
                    log.error('Claim verification failed.\n' +
                              claim_verify_response.text)
                exit(0)
            print("Claim verify done")
            log.debug("Claim Verify POST Response: status code: " +
                      str(claim_verify_response.status_code) +
                      " and response text: " + claim_verify_response.text)
            log.info("Claim verify done")
            node_cert = json.loads(claim_verify_response.text)['certificate']
            print("Claim certificate received")
            log.info("Claim certificate received")
        except requests.exceptions.SSLError:
            raise SSLError
        except requests.ConnectionError:
            log.error("Please check the Internet connection.")
            exit(0)

        # Set node claim data
        sys.stdout = StringIO()
        log.info("Getting MQTT Host")
        endpointinfo = node.get_mqtt_host(None)
        log.debug("Endpoint info received: " + endpointinfo)
        sys.stdout = sys.__stdout__

        # Extract private key in bytes from private key object generated
        log.info("Extracting private key in bytes")
        node_private_key = private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption())

        # Create files of each claim data info
        print("\nSaving claiming data info at location: ", dest_filedir)
        log.debug("Saving claiming data info at location: " +
                  dest_filedir)
        create_files_of_claim_info(dest_filedir, node_info, node_private_key,
                                   node_cert, endpointinfo, node_info_csv)

        # Run NVS Partition Utility to create binary of node info data
        print("\nGenerating NVS Partition Binary from claiming data: " +
              dest_filedir + output_bin_filename)
        log.debug("Generating NVS Partition Binary from claiming data: " +
                  dest_filedir + output_bin_filename)
        nvs_partition_gen.generate(nvs_args)
        print("\nFlashing onto node\n")
        log.info("Flashing binary onto node")
        flash_bin_onto_node(port, esptool, dest_filedir + output_bin_filename)

        print("Claiming done")
        log.info("Claiming done")
        print("Time(s):" + str(time.time() - start))
    except Exception as err:
        log.error(err)
        sys.exit(err)
