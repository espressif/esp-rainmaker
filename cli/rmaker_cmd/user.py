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
try:
    from rmaker_lib import user
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
            verification_code = input('Enter verification code sent on your'
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

    :return: None on Success
    :rtype: None
    """
    log.info('Signing in the user. Username  ' + str(vars['email']))
    if vars['email'] is None:
        browser_login()
        return
    u = user.User(vars['email'])
    try:
        u.login()
    except Exception as login_err:
        log.error(login_err)
    else:
        print('Login Successful')


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
