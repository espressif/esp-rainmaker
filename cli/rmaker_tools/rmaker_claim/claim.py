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
import re
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


def flash_nvs_partition_bin(port, bin_to_flash, address):
    """
    Flash binary onto node

    :param port: Serial Port
    :type port: str

    :param bin_to_flash: Filname of binary to flash
    :type bin_to_flash: str

    :raises Exception: If there is any issue when flashing binary onto node

    :return: None on Success
    :rtype: None
    """
    try:
        if not port:
            sys.exit("If you want to write the claim data to flash, please provide the <port> argument.")
        print("Flashing binary onto node")
        log.info("Flashing binary onto node")
        command = ['--port', port, 'write_flash', address, bin_to_flash]
        esptool.main(command)
    except Exception as err:
        log.error(err)
        sys.exit(1)

def get_node_platform_and_mac(port):
    """
    Get Node Platform and Mac Addres from device

    :param port: Serial Port
    :type port: str

    :return: Node Platform and Mac Address on Success
    :rtype: str
    """
    if not port:
        sys.exit("<port> argument not provided. Cannot read platform and MAC address from node.")
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
    platform = node_platform.split()[-1].lower().replace('-', '')
    print("Node platform detected is: ", platform)
    print("MAC address is: ", mac_addr)
    log.debug("MAC address received: " + mac_addr)
    log.debug("Node platform is: " + platform)
    return platform, mac_addr

def get_secret_key(port):
    """
    Generate Secret Key

    :param port: Serial Port
    :type port: str

    :param esptool: esptool module
    :type esptool: module

    :return: Secret Key on Success
    :rtype: str
    """
    if not port:
        sys.exit("<port> argument not provided. Cannot read secret_key from node.")
    esp = esptool.ESP32S2ROM(port)
    esp.connect('default_reset')
    for (name, idx, read_addr, _, _) in BLOCKS:
        addrs = range(read_addr, read_addr + 32, 4)
        secret = "".join(["%08x" % esp.read_reg(addr) for addr in addrs[0:4]])
        secret = secret[6:8]+secret[4:6]+secret[2:4]+secret[0:2] +\
            secret[14:16]+secret[12:14]+secret[10:12]+secret[8:10] +\
            secret[22:24]+secret[20:22]+secret[18:20]+secret[16:18] +\
            secret[30:32]+secret[28:30]+secret[26:28]+secret[24:26]
    # Verify secret key exists
    secret_key_tmp = secret.strip('0')
    if not secret_key_tmp:
        return False
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

def save_claim_data(dest_filedir, node_id, private_key, node_cert, endpointinfo, node_info_csv):
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
    # Create files of each claim data info
    print("\nSaving claiming data info at location: ", dest_filedir)
    log.debug("Saving claiming data info at location: " +
                dest_filedir)
    try:
        log.debug("Writing node info at location: " + dest_filedir +
                  'node.info')
        # Create files for each claim data - node info, node key,
        # node cert, endpoint info
        with open(str(dest_filedir+'node.info'), 'w+') as info_file:
            info_file.write(node_id)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'node.key')
        with open(str(dest_filedir+'node.key'), 'wb+') as info_file:
            info_file.write(private_key)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'node.crt')
        with open(str(dest_filedir+'node.crt'), 'w+') as info_file:
            info_file.write(node_cert)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'endpoint.info')
        with open(str(dest_filedir+'endpoint.info'), 'w+') as info_file:
            info_file.write(endpointinfo)

        log.debug("Writing node info at location: " +
                  dest_filedir + 'node_info.csv')
        with open(str(dest_filedir+'node_info.csv'), 'w+') as info_file:
            for input_line in node_info_csv:
                info_file.write(input_line)
                info_file.write("\n")
    except Exception as file_error:
        raise file_error

def gen_nvs_partition_bin(dest_filedir, output_bin_filename):
    # Generate nvs args to be sent to NVS Partition Utility
    nvs_args = SimpleNamespace(input=dest_filedir+'node_info.csv',
                                output=output_bin_filename,
                                size='0x6000',
                                outdir=dest_filedir,
                                version=2)
    # Run NVS Partition Utility to create binary of node info data
    print("\nGenerating NVS Partition Binary from claiming data: " +
            dest_filedir + output_bin_filename)
    log.debug("Generating NVS Partition Binary from claiming data: " +
                dest_filedir + output_bin_filename)
    nvs_partition_gen.generate(nvs_args)

def set_claim_verify_data(claim_init_resp, private_key, mac_addr=None, secret_key=None):
    # set claim verify data for node_platform = esp32
    if not mac_addr and not secret_key:
        # Generate CSR with common_name=node_id received in response
        node_id = str(json.loads(
            claim_init_resp.text)['node_id'])
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
        # set claim verify data for node_platform = esp32s2
        auth_id = str(json.loads(
            claim_init_resp.text)['auth_id'])
        hmac_challenge = str(json.loads(
            claim_init_resp.text)['challenge'])
        print("Generating CSR")
        log.info("Generating CSR")
        csr = gen_host_csr(private_key, common_name=mac_addr)
        if not csr:
            raise Exception("CSR Not Generated. Claiming Failed")
        log.info("CSR generated")
        log.info("Generating hmac challenge response")
        hmac_challenge_response = gen_hmac_challenge_resp(
            secret_key,
            hmac_challenge)
        hmac_challenge_response = hmac_challenge_response.strip('\n')
        log.debug("Secret Key: " + secret_key)
        log.debug("HMAC Challenge Response: " +
                    hmac_challenge_response)
        claim_verify_data = {"auth_id":
                             auth_id,
                             "challenge_response":
                             hmac_challenge_response,
                             "csr":
                             csr}
        # Save mac addr as node info to use while saving claim data
        # in csv file
        node_info = mac_addr
    return claim_verify_data, node_info

def set_claim_initiate_data(mac_addr, node_platform):
    # Set Claim initiate request data
    claim_initiate_data = {"mac_addr": mac_addr, "platform": node_platform}
    claim_init_enc_data = str(claim_initiate_data).replace(
        "'", '"')
    return claim_init_enc_data

def claim_verify(claim_verify_data, header):
    claim_verify_url = CLAIM_VERIFY_URL
    user_whitelist_err_msg = ('User is not allowed to claim esp32 device.'
                              ' please contact administrator')
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
    return claim_verify_response

def claim_initiate(claim_init_data, header):
    print("Claim initiate started")
    claim_initiate_url = CLAIM_INITIATE_URL
    try:
        # Claim Initiate Request
        log.info("Claim initiate started. Sending claim/initiate POST request")
        log.debug("Claim Initiate POST Request: url: " +
                    claim_initiate_url + "data: " +
                    str(claim_init_data) +
                    "headers: " + str(header) +
                    "verify: " + CERT_FILE)
        claim_initiate_response = requests.post(url=claim_initiate_url,
                                                data=claim_init_data,
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
        return claim_initiate_response
    except requests.exceptions.SSLError:
        raise SSLError
    except requests.ConnectionError:
        log.error("Please check the Internet connection.")
        exit(0)

def start_claim_process(node_platform, mac_addr, private_key, secret_key=None):
    log.info("Creating session")
    curr_session = session.Session()
    header = curr_session.request_header
    try:
        # Set claim initiate data
        claim_init_data = set_claim_initiate_data(mac_addr, node_platform)

        # Perform claim initiate request
        claim_init_resp = claim_initiate(claim_init_data, header)

        # Set claim verify data
        if node_platform == "esp32":
            claim_verify_data, node_info = set_claim_verify_data(claim_init_resp, private_key)
        else:
            claim_verify_data, node_info = set_claim_verify_data(claim_init_resp, private_key, mac_addr=mac_addr, secret_key=secret_key)

        # Perform claim verify request
        claim_verify_resp = claim_verify(claim_verify_data, header)

        # Get certificate from claim verify response
        node_cert = json.loads(claim_verify_resp.text)['certificate']
        print("Claim certificate received")
        log.info("Claim certificate received")

        return node_info, node_cert
    except requests.exceptions.SSLError:
        raise SSLError
    except requests.ConnectionError:
        log.error("Please check the Internet connection.")
        exit(0)

def generate_private_key():
    # Generate Key
    log.info("Generate RSA key")
    private_key = rsa.generate_private_key(
                    public_exponent=65537,
                    key_size=2048,
                    backend=default_backend()
    )
    log.info("RSA Private Key generated")
    # Extract private key in bytes from private key object generated
    log.info("Extracting private key in bytes")
    private_key_bytes = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption())
    return private_key, private_key_bytes

def verify_secret_key_exists(secret_key):
    secret_key_tmp = secret_key.strip('0')
    if not secret_key_tmp:
        return False
    return True

def verify_mac_dir_exists(creds_dir, mac_addr):
    mac_dir = Path(path.expanduser(str(creds_dir) + '/' + str(mac_addr)))
    if mac_dir.exists():
        dest_filedir = str(mac_dir) + '/'
        output_bin_filename = str(mac_addr) + '.bin'
        return dest_filedir, output_bin_filename
    return False, False

def create_mac_dir(creds_dir, mac_addr):
    # Create MAC directory
    mac_dir = Path(path.expanduser(str(creds_dir) + '/' + str(mac_addr)))
    os.makedirs(path.expanduser(str(mac_dir)))
    log.debug("Creating new directory " + str(mac_dir))
    output_bin_filename = str(mac_addr) + '.bin'
    dest_filedir = str(mac_dir) + '/'
    return dest_filedir, output_bin_filename

def create_config_dir():
    config = configmanager.Config()
    userid = config.get_user_id()

    creds_dir = Path(path.expanduser(
        str(Path(path.expanduser(configmanager.HOME_DIRECTORY))) +
        '/' +
        str(Path(path.expanduser(
            configmanager.CONFIG_DIRECTORY))) +
        '/claim_data/' +
        str(userid)
        ))
    if not creds_dir.exists():
        os.makedirs(path.expanduser(str(creds_dir)))
        log.debug("Creating new directory " + str(creds_dir))
    return userid, creds_dir

def get_mqtt_endpoint():
    # Set node claim data
    sys.stdout = StringIO()
    log.info("Getting MQTT Host")
    endpointinfo = node.get_mqtt_host(None)
    log.debug("Endpoint info received: " + endpointinfo)
    sys.stdout = sys.__stdout__
    return endpointinfo

def verify_claim_data_binary_exists(userid, mac_addr, dest_filedir, output_bin_filename):
    # Set config mac addr path
    mac_addr_config_path = str(Path(path.expanduser(
        configmanager.CONFIG_DIRECTORY))) + '/claim_data/' +\
        str(userid) +\
        '/' +\
        str(mac_addr) +\
        '/' + str(output_bin_filename)
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
        return True
    return False

def flash_existing_data(port, bin_to_flash, address):
    # Flashing existing binary onto node
    if not port:
        sys.exit("If you want to write the claim data to flash, please provide the <port> argument.")
    log.info("Using existing claiming data")
    print("Using existing claiming data")
    flash_nvs_partition_bin(port, bin_to_flash, address)
    log.info("Binary flashed onto node")

def set_csv_file_data(dest_filedir):
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
    return node_info_csv

def validate_secret_key(secret_key):
    if not re.match(r'([0-9a-f]){32}', secret_key):
        return False
    return True

def claim(port=None, node_platform=None, mac_addr=None, secret_key=None, flash_address=None):
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
        node_info = None
        private_key = None
        secret_key_valid = None
        claim_data_binary_exists = False
        dest_filedir = None
        output_bin_filename = None

        if not flash_address:
            flash_address = '0x340000'

        # Create base config creds dir
        userid, creds_dir = create_config_dir()

        # Get node platform and mac addr if not provided
        if not node_platform and not mac_addr:
            node_platform, mac_addr = get_node_platform_and_mac(port)
            # Node platform detected is esp32s2
            if node_platform not in ["esp32"]:
                # Get secret key
                secret_key = get_secret_key(port)
                # Set platform to esp32 if node does not have secret key
                if not secret_key:
                    node_platform="esp32"

        # Verify mac directory exists
        dest_filedir, output_bin_filename  = verify_mac_dir_exists(creds_dir, mac_addr)

        # Create mac subdirectory in creds config directory created above
        if not dest_filedir and not output_bin_filename:
            dest_filedir, output_bin_filename = create_mac_dir(creds_dir, mac_addr)
        
        # Set NVS binary filename
        nvs_bin_filename = dest_filedir + output_bin_filename

        # Set csv file output data
        node_info_csv=set_csv_file_data(dest_filedir)

        # Verify existing data exists
        claim_data_binary_exists = verify_claim_data_binary_exists(userid, mac_addr, dest_filedir, output_bin_filename)
        if claim_data_binary_exists:
            # Flash existing NVS binary onto node
            flash_existing_data(port, nvs_bin_filename, flash_address)
            return

        if node_platform not in ["esp32"]:
            if not secret_key:
                sys.exit("Invalid. --secret-key argument needed for platform {}.".format(node_platform))
            secret_key_valid = validate_secret_key(secret_key)
            if not secret_key_valid:
                sys.exit('Invalid Secret Key.')

        start = time.time()

        # Generate private key
        private_key, private_key_bytes = generate_private_key()

        print("\nClaiming process started. This may take time.")
        log.info("Claiming process started. This may take time.")

        # Start claim process
        node_info, node_cert = start_claim_process(node_platform, mac_addr, private_key, secret_key=secret_key)

        # Get MQTT endpoint
        endpointinfo = get_mqtt_endpoint()

        # Create output claim files
        save_claim_data(dest_filedir, node_info, private_key_bytes, node_cert, endpointinfo, node_info_csv)

        # Generate nvs partition binary
        gen_nvs_partition_bin(dest_filedir, output_bin_filename)

        # Flash generated NVS partition binary
        flash_nvs_partition_bin(port, nvs_bin_filename, flash_address)

        print("Claiming done")
        log.info("Claiming done")
        print("Time(s):" + str(time.time() - start))
    except Exception as err:
        log.error(err)
        sys.exit(err)
