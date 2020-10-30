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

import sys
import re
import getpass
import json
try:
    from rmaker_lib import user, configmanager, session
    from rmaker_lib.logger import log
except ImportError as err:
    print("Failed to import ESP Rainmaker library. " + str(err))
    raise err

from rmaker_cmd.browserlogin import browser_login

MAX_PASSWORD_CHANGE_ATTEMPTS = 3


def signup(vars=None):
    """
    User signup to the ESP Rainmaker.

    :param vars: `email` as key - Email address of the user, defaults to `None`
    :type vars: dict

    :raises Exception: If there is any issue in signup for user

    :return: None on Success
    :rtype: None
    """
    log.info('Signing up the user ' + vars['email'])
    u = user.User(vars['email'])
    password = get_password()
    try:
        status = u.signup_request(password)
    except Exception as signup_err:
        log.error(signup_err)
    else:
        if status is True:
            verification_code = input('Enter verification code sent on your '
                                      'Email.\n Verification Code : ')
            try:
                status = u.signup(verification_code)
            except Exception as signup_err:
                log.error(signup_err)
                return
            print('Signup Successful\n'
                  'Please login to continue with ESP Rainmaker CLI')
        else:
            log.error('Signup failed. Please try again.')
    return


def login(vars=None):
    """
    First time login of the user.

    :param vars: `email` as key - Email address of the user, defaults to `None`
    :type vars: dict

    :raises Exception: If there is any issue in login for user

    :return: None on Success and Failure
    :rtype: None
    """
    log.info('Signing in the user. Username  ' + str(vars['email']))
    config = configmanager.Config()
    
    # Set email-id
    email_id = vars['email']
    log.info('Logging in user: {}'.format(email_id))
    
    # Check user current creds exist
    resp_filename = config.check_user_creds_exists()
    
    # If current creds exist, ask user for ending current session
    if resp_filename:      
        
        # Get email-id of current logged-in user
        curr_email_id = config.get_token_attribute('email')
        log.info('Logging out user: {}'.format(curr_email_id))
        
        # Get user input
        input_resp = config.get_input_to_end_session(curr_email_id)
        if not input_resp:
            return
        
        # Remove current login creds
        ret_val = config.remove_curr_login_creds(curr_creds_file=resp_filename)
        if ret_val is None:
            print("Failed to end previous login session. Exiting.")
            return
    else:
        log.debug("Current login creds not found at path: {}".format(resp_filename))

    if vars['email'] is None:
        browser_login()
        return
    try:
        u = user.User(vars['email'])
        u.login()
        print('Login Successful')
    except Exception as login_err:
        log.error(login_err)


def logout(vars=None):
    """
    Logout of the current session.

    :raises Exception: If there is any issue in logout for user

    :return: None on Success and Failure
    :rtype: None
    """
    log.info('Logging out current logged-in user')

    # Removing the creds stored locally
    log.debug("Removing creds stored locally, invalidating token")
    config = configmanager.Config()
    
    # Get email-id of current logged-in user
    email_id = config.get_token_attribute('email')
    log.info('Logging out user: {}'.format(email_id))

    # Ask user for ending current session   
    # Get user input
    input_resp = config.get_input_to_end_session(email_id)
    if not input_resp:
        return
    
    # Call Logout API
    try:
        curr_session = session.Session()
        status_resp = curr_session.logout()
        log.debug("Logout API successful")
    except Exception as logout_err:
        log.error(logout_err)
        return

    # Check API status in response
    if 'status' in status_resp and status_resp['status'] == 'failure':
        print("Logout from ESP RainMaker Failed. Exiting.")
        print("[{}]:{}".format(status_resp['error_code'], status_resp['description']))
        return   
    
    # Remove current login creds
    ret_val = config.remove_curr_login_creds()
    if ret_val is None:
        print("Logout from ESP RainMaker Failed. Exiting.")
        return
    
    # Logout is successful
    print("Logged out from ESP RainMaker")
    log.debug('Logout Successful')
    log.debug("Local creds removed successfully")
    
    return


def forgot_password(vars=None):
    """
    Forgot password request to reset the password.

    :param vars: `email` as key - Email address of the user, defaults to `None`
    :type vars: dict

    :raises Exception: If there is an HTTP issue while
                       changing password for user

    :return: None on Success and Failure
    :rtype: None
    """
    log.info('Changing user password. Username ' + vars['email'])
    config = configmanager.Config()
    
    # Get email-id if present
    try:
        email_id = config.get_token_attribute('email')
    except Exception:
        email_id = None
    
    # If current logged-in user is same as
    # the email-id given as user input
    # end current session
    # (invalidate current logged-in user token)
    log.debug("Current user email-id: {}, user input email-id: {}".format(email_id, vars['email']))
    if email_id and email_id == vars['email']:
        log.debug("Ending current session for user: {}".format(email_id))
        
        # Check user current creds exist
        resp_filename = config.check_user_creds_exists()
        if not resp_filename:
            log.debug("Current login creds not found at path: {}".format(resp_filename))
            log.error("User not logged in")
            return

        # If current creds exist, ask user for ending current session   
        # Get user input
        input_resp = config.get_input_to_end_session(email_id)
        if not input_resp:
            return
        
        # Remove current login creds
        ret_val = config.remove_curr_login_creds(curr_creds_file=resp_filename)
        if ret_val is None:
            print("Failed to end previous login session. Exiting.")
            return
    
    u = user.User(vars['email'])
    status = False
    
    try:
        status = u.forgot_password()
    except Exception as forgot_pwd_err:
        log.error(forgot_pwd_err)
    else:
        verification_code = input('Enter verification code sent on your Email.'
                                  '\n Verification Code : ')
        password = get_password()
        if status is True:
            try:
                log.debug('Received verification code on email ' +
                          vars['email'])
                status = u.forgot_password(password, verification_code)
            except Exception as forgot_pwd_err:
                log.error(forgot_pwd_err)
            else:
                print('Password changed successfully.'
                      'Please login with the new password.')
        else:
            log.error('Failed to reset password. Please try again.')
    return


def get_password():
    """
    Get Password as input and perform basic password validation checks.

    :raises SystemExit: If there is an issue in getting password

    :return: Password for User on Success
    :rtype: str
    """
    log.info('Doing basic password confirmation checks.')
    password_policy = '8 characters, 1 digit, 1 uppercase and 1 lowercase.'
    password_change_attempt = 0

    print('Choose a password')
    while password_change_attempt < MAX_PASSWORD_CHANGE_ATTEMPTS:
        log.debug('Password change attempt number ' +
                  str(password_change_attempt+1))
        password = getpass.getpass('Password : ')
        if len(password) < 8 or re.search(r"\d", password) is None or\
           re.search(r"[A-Z]", password) is None or\
           re.search(r"[a-z]", password) is None:
            print('Password should contain at least', password_policy)
            password_change_attempt += 1
            continue
        confirm_password = getpass.getpass('Confirm Password : ')
        if password == confirm_password:
            return password
        else:
            print('Passwords do not match!\n'
                  'Please enter the password again ..')
        password_change_attempt += 1

    log.error('Maximum attempts to change password over. Please try again.')
    sys.exit(1)


def get_user_details(vars=None):
    """
    Get details of current logged-in user
    """
    try:
        # Get user details
        log.debug('Getting details of current logged-in user')
        curr_session = session.Session()
        user_info = curr_session.get_user_details()
        log.debug("User details received")
    except Exception as err:
        log.error(err)
    else:
        # Print API response output
        for key, val in user_info.items():
            if key == "user_name":
                key = key + " (email)"
            title = key.replace("_", " ").title()
            print("{}: {}".format(title, val))
    return