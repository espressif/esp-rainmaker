/*!
 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)

 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "m5_relay.h"

UNIT_4RELAY relay;

void init_relay(void) 
{
    relay.begin(&Wire, M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());
    relay.Init(SYNC);
}

static void set_relay_sw(bool rm_relay_sw_state, uint8_t relay_sw) 
{
    if (rm_relay_sw_state) {
        relay.relayWrite(relay_sw, RELAY_ON);
    } else {
        relay.relayWrite(relay_sw, RELAY_OFF);
    }
}

void set_relay(void) 
{
    for (int relay_sw = 0; relay_sw < 4; relay_sw++) {
        set_relay_sw(rm_relay_sw_state[relay_sw], relay_sw);
    }
}

void set_relay_off(void) 
{
    for (int relay_sw = 0; relay_sw < 4; relay_sw++) {
        set_relay_sw(DEFAULT_SWITCH_POWER, relay_sw);
    }
}