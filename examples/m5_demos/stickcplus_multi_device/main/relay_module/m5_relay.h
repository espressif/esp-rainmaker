/*!
 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)

 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once
#include "interface_module/m5_interface.h"
#include "m5_relay/src/Unit_4RELAY.h"

#define RELAY_ON 1
#define RELAY_OFF 0
#define ASYNC 0
#define SYNC 1

extern "C" 
{
void init_relay(void);
void set_relay(void);
void set_relay_off(void);
}