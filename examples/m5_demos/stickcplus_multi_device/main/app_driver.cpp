/*!
 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)

 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "app_priv.h"

extern "C" {
#include <app_reset.h>
#include <sdkconfig.h>
}

bool rm_sgl_sw_state = DEFAULT_SWITCH_POWER;
bool rm_light_sw_state = DEFAULT_SWITCH_POWER;
bool rm_relay_sw_state[4] = {DEFAULT_SWITCH_POWER};

int8_t brt_level = DEFAULT_BRT;
int16_t hue_level = DEFAULT_HUE;
static bool brt_sw_direction = SW_DIRECTION_UP;
static bool hue_sw_direction = SW_DIRECTION_UP;

static unsigned long last_event_time_brt = 0;
static unsigned long last_event_time_hue = 0;

/* Index 0: on/off; Index 1: Brt; Index 2: Hue */
bool light_sw_array[3] = {false};
/* Index 0: switch_1; Index 1: switch_2; Index 2: switch_3; Index 3: switch_4 */
bool relay_sw_array[4] = {false};

/**
 * This section is for driver initialisation and other generic driver functions.
 */
//=======================================================================================================================

/*!
 *  @brief Initiate the driver, of all the display screens and modules.
 */
void init_app_driver(void) 
{
    Serial.begin(115200);
    init_m5();
    init_images();
    init_neohex();
    display_start_screen();
    display_sgl_switch_screen();
    display_light_screen();
    display_relay_screen();
    esp_log_level_set("gpio", ESP_LOG_NONE);
}

/*!
 *  @brief This is the main screen callback to switch between examples,
           reset wifi or factory reset.
 *  @param event Logs in the type of event that is driven by the type
                  of example and button pressed.
 */
void scrn_event_cb(lv_event_t *event) 
{

    lv_event_code_t event_code = lv_event_get_code(event);

    /* Handle different events */
    switch (event_code) {
    case LV_EVENT_PRESSING: // Return to home screen
        if (lvgl_port_lock()) {
            disc_relay();
            set_start_scrn();
            show_start_screen();
            lvgl_port_unlock();
        }
        break;
    case LV_EVENT_PRESSED: // Switch to next page
        if (is_sgl_sw_scrn) {
            set_lighting_state(DEFAULT_SWITCH_POWER);
            set_light_scrn();
            show_light_scrn();
            set_light_sw_state(rm_light_sw_state);
        } else if (is_light_scrn) {
            set_lighting_state(DEFAULT_SWITCH_POWER);
            init_relay();
            set_relay_sw_scrn();
            show_relay_scrn();
            set_relay_sw_state();
        } else if (is_relay_scrn) {
            disc_relay();
            set_sgle_sw_scrn();
            show_sgl_switch_screen();
            set_sgl_sw_state(rm_sgl_sw_state);
        }
        break;
    case LV_EVENT_LONG_PRESSED: // RainMaker reset or factory reset is enabled
        if (is_wifi_reset) {
            is_wifi_reset = false;
            esp_rmaker_wifi_reset(RESET_DELAY, REBOOT_DELAY);
        } else if (is_factory_reset) {
            is_factory_reset = false;
            esp_rmaker_factory_reset(RESET_DELAY, REBOOT_DELAY);
        }
        break;
    default:
        break;
    }
}

/*!
 *  @brief Update param by name or by type to the cloud of a given specific param.
 *  @param param_attr Attribute of the param, which two cases; PARAM_TYPE or PARAM_NAME.
 *  @param param_id The param identity i.e. ESP_RMAKER_PARAM_POWER
 *  @param device The specific device that have been initiated in app_main.cpp i.e. light_device.
 *  @param val Value of the param that can be of different types which includes int, float, bool, strm json
 * object.
 */
void update_rm_param(const char *param_attr, const char *param_id, esp_rmaker_device_t *device,
                     esp_rmaker_param_val_t val) 
{
    esp_rmaker_param_t *param = NULL;
    if (strcmp(param_attr, PARAM_TYPE) == 0) {
        param = esp_rmaker_device_get_param_by_type(device, param_id);
    } else if (strcmp(param_attr, PARAM_NAME) == 0) {
        param = esp_rmaker_device_get_param_by_name(device, param_id);
    }
    if (param != NULL) {
        esp_rmaker_param_update_and_report(param, val);
    } else {
        ESP_LOGE("RM", "Parameter '%s' not found for device", param_id);
    }
}

/*!
 *  @brief Deinitiate the i2c connection to prevent signal conflict when FastLED task is running.
 */
void deinit_i2c(void) 
{
    M5.Ex_I2C.release();
    pinMode(M5.Ex_I2C.getSCL(), OUTPUT);
    pinMode(M5.Ex_I2C.getSDA(), OUTPUT);
    digitalWrite(M5.Ex_I2C.getSCL(), HIGH);
    digitalWrite(M5.Ex_I2C.getSDA(), HIGH);
}

/**
 * This section is for start scrn driver.
 */
//=======================================================================================================================

/*!
 *  @brief Callback function for start button.
 *  @param event Logs in the type of event that is driven by the type
                  of example and button pressed.
 */
void start_btn_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (is_start_scrn) {
        if (code == LV_EVENT_CLICKED) {
            display_start_btn_pressed();
        } else if (code == LV_EVENT_RELEASED) {
            set_sgle_sw_scrn();
            show_sgl_switch_screen();
        } else if (code == LV_EVENT_REFRESH) {
            reset_start_btn();
        }
    }
}

/**
 * This section is for single switch device driver.
 */
//=======================================================================================================================

static bool is_sgl_switch_off(lv_event_code_t code) 
{ 
    return is_sgl_sw_scrn && !rm_sgl_sw_state; 
}

static bool is_sgl_switch_on(lv_event_code_t code) 
{ 
    return is_sgl_sw_scrn && rm_sgl_sw_state; 
}

static void on_sgl_sw(void) 
{
    if (lvgl_port_lock()) {
        display_light_bulb_on();
        lv_obj_add_state(sgl_sw, LV_STATE_CHECKED);
        rm_sgl_sw_state = true;
        lvgl_port_unlock();
    }
}

static void off_sgl_sw(void) {
    if (lvgl_port_lock()) {
        display_light_bulb_off();
        if (lv_obj_has_state(sgl_sw, LV_STATE_CHECKED)) {
            lv_obj_clear_state(sgl_sw, LV_STATE_CHECKED);
        }
        rm_sgl_sw_state = false;
        lvgl_port_unlock();
    }
}

void set_sgl_sw_state(bool state) {
    if (state) {
        on_sgl_sw();
    } else {
        off_sgl_sw();
    }
    set_lighting_state(rm_sgl_sw_state);
}

/*!
 *  @brief Callback function for single switch button and to update cloud.
 *  @param event Logs in the type of event that is driven by the type
                  of example and button pressed.
 */
void my_single_switch_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (is_sgl_switch_off(code)) {
        on_sgl_sw();
    } else if (is_sgl_switch_on(code)) {
        off_sgl_sw();
    }
    set_lighting_state(rm_sgl_sw_state);
    update_rm_param(PARAM_TYPE, ESP_RMAKER_PARAM_POWER, switch_device, esp_rmaker_bool(rm_sgl_sw_state));
}

/**
 * This section is for light device driver.
 */
//=======================================================================================================================

static bool is_light_sw_off(lv_event_code_t code) 
{
    return is_light_scrn && light_sw_array[ON_OFF_SW] && !rm_light_sw_state;
}

static bool is_light_sw_on(lv_event_code_t code) 
{
    return is_light_scrn && light_sw_array[ON_OFF_SW] && rm_light_sw_state;
}

static void on_light_sw(void) 
{
    if (lvgl_port_lock()) {
        lv_obj_add_state(light_sw, LV_STATE_CHECKED);
        rm_light_sw_state = true;
        lvgl_port_unlock();
    }
}

static void off_light_sw(void) 
{
    if (lvgl_port_lock()) {
        lv_obj_clear_state(light_sw, LV_STATE_CHECKED);
        rm_light_sw_state = false;
        lvgl_port_unlock();
    }
}

static void set_indicator_color(lv_obj_t *object, int color) 
{
    if (lvgl_port_lock()) {
        lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_KNOB);
        lvgl_port_unlock();
    }
}

/*!
 *  @brief To set the indicator color on the light switch to determine if it is going
           up or going down.
 *  @note  Green increases in value, orange decreases in value.
 */
static void set_indicator_dir_color(lv_obj_t *object, bool sw_direction) 
{
    if (lvgl_port_lock()) {
        if (sw_direction) {
            lv_obj_set_style_bg_color(object, lv_color_hex(COLOR_GREEN), LV_PART_KNOB);
        } else {
            lv_obj_set_style_bg_color(object, lv_color_hex(COLOR_ORANGE), LV_PART_KNOB);
        }
        lvgl_port_unlock();
    }
}

/*!
 *  @brief Sets the frequency of how fast the button can be pressed in a period of time.
 *  @param event_btn_time The time at which the button was last pressed.
 */
static void set_btn_freq(unsigned long event_btn_time) 
{
    unsigned long now = esp_timer_get_time() / 1000;
    if (now - event_btn_time < EVENT_DEBOUNCE_INTERVAL) {
        return;
    }
    event_btn_time = now;
}

bool get_light_sw_status(int call_sw) 
{ 
    return light_sw_array[call_sw]; 
}

void set_sw(int call_sw) 
{
    for (int i = 0; i < 3; i++) {
        if (i == call_sw) {
            light_sw_array[call_sw] = true;
        } else {
            light_sw_array[i] = false;
        }
    }
}

void set_light_sw_state(bool state) 
{
    if (state) {
        on_light_sw();
    } else {
        off_light_sw();
    }
    set_lighting_state(rm_light_sw_state);
}

void set_light_brt(int brt_value) 
{
    if (lvgl_port_lock()) {
        lv_slider_set_value(brt_slider, brt_value, LV_ANIM_ON);
        lvgl_port_unlock();
    }
    set_brt();
}

void set_light_hue(int hue_value) 
{
    if (lvgl_port_lock()) {
        lv_slider_set_value(hue_slider, hue_value, LV_ANIM_ON);
        lvgl_port_unlock();
    }
    set_hue();
}

/*!
 *  @brief Callback function to select the type of switch within the light device.
 *  @param event Logs in the type of event that is driven by the type of example and button pressed.
 */
void light_sw_select_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (is_light_scrn && code == LV_EVENT_SHORT_CLICKED) {
        if (get_light_sw_status(ON_OFF_SW)) {
            set_sw(BRT_SW);
            set_indicator_color(light_sw, COLOR_WHITE);
            set_indicator_dir_color(brt_slider, brt_sw_direction);
        } else if (get_light_sw_status(BRT_SW)) {
            set_sw(HUE_SW);
            set_indicator_color(brt_slider, COLOR_WHITE);
            set_indicator_dir_color(hue_slider, hue_sw_direction);
        } else if (get_light_sw_status(HUE_SW)) {
            set_sw(ON_OFF_SW);
            set_indicator_color(hue_slider, COLOR_WHITE);
            set_indicator_color(light_sw, COLOR_ORANGE);
        }
    }
}

/*!
 *  @brief Callback function to control on/off function of light device.
 *  @param event Logs in the type of event that is driven by the type of example and button pressed.
 */
void light_sw_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (is_light_sw_off(code)) {
        on_light_sw();
    } else if (is_light_sw_on(code)) {
        off_light_sw();
    }
    set_lighting_state(rm_light_sw_state);
    update_rm_param(PARAM_TYPE, ESP_RMAKER_PARAM_POWER, light_device, esp_rmaker_bool(rm_light_sw_state));
}

/*!
 *  @brief Callback function to control the brightness function of light device.
 *  @param event Logs in the type of event that is driven by the type of example and button pressed.
 */
void light_brt_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CLICKED) {
        set_btn_freq(last_event_time_brt);
        if (brt_sw_direction == SW_DIRECTION_UP && brt_level <= FULL_BRT) {
            brt_level += 5;
        } else if (brt_sw_direction == SW_DIRECTION_DOWN && brt_level >= NULL_BRT) {
            brt_level -= 5;
        }
        set_light_brt(brt_level);
        update_rm_param(PARAM_TYPE, ESP_RMAKER_PARAM_BRIGHTNESS, light_device, esp_rmaker_int(brt_level));
    } else if (code == LV_EVENT_LONG_PRESSED) {
        brt_sw_direction ^= 1;
        set_indicator_dir_color(brt_slider, brt_sw_direction);
    }
}

/*!
 *  @brief Callback function to control the hue function of light device.
 *  @param event Logs in the type of event that is driven by the type of example and button pressed.
 */
void light_hue_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_CLICKED) {
        set_btn_freq(last_event_time_hue);
        if (hue_sw_direction == SW_DIRECTION_UP && hue_level <= MAX_HUE) {
            hue_level += 10;

        } else if (hue_sw_direction == SW_DIRECTION_DOWN && hue_level >= MIN_HUE) {
            hue_level -= 10;
        }
        if (hue_level > MAX_HUE) {
            hue_level = MAX_HUE;
        } else if (hue_level < MIN_HUE) {
            hue_level = MIN_HUE;
        }
        set_light_hue(hue_level);
        update_rm_param(PARAM_TYPE, ESP_RMAKER_PARAM_HUE, light_device, esp_rmaker_int(hue_level));
    } else if (code == LV_EVENT_LONG_PRESSED) {
        hue_sw_direction ^= 1;
        set_indicator_dir_color(hue_slider, hue_sw_direction);
    }
}

/**
 * This section is for relay device driver.
 */
//=======================================================================================================================

static bool is_relay_sw_off(int relay_sw, bool rm_relay_sw_state) 
{
    return is_relay_scrn && relay_sw_array[relay_sw] && !rm_relay_sw_state;
}

static bool is_relay_sw_on(int relay_sw, bool rm_relay_sw_state) 
{
    return is_relay_scrn && relay_sw_array[relay_sw] && rm_relay_sw_state;
}

static bool get_relay_sw_status(int call_sw) 
{ 
    return relay_sw_array[call_sw]; 
}

static void on_relay_sw(lv_obj_t *relay_sw) 
{
    if (lvgl_port_lock()) {
        lv_obj_add_state(relay_sw, LV_STATE_CHECKED);
        lvgl_port_unlock();
    }
}

static void off_relay_sw(lv_obj_t *relay_sw) 
{
    if (lvgl_port_lock()) {
        lv_obj_clear_state(relay_sw, LV_STATE_CHECKED);
        lvgl_port_unlock();
    }
}

/*!
 *  @brief Set the relay switch state based on the button pressed and update cloud.
 *  @param relay_sw object of the relay switch on M5 interface.
 *  @param relay_name param name set in app_main.cpp during param creation for relay device.
 *  @param relay_index To select the switch index to obtain its bool state as it is stored in an array.
 *  @param rm_relay_state actual state of particular relay switch state.
 */
static void set_relay_sw_btn(lv_obj_t *relay_sw, const char *relay_name, uint8_t relay_index,
                             bool rm_relay_state) 
{
    if (rm_relay_state) {
        rm_relay_sw_state[relay_index] = false;
        off_relay_sw(relay_sw);
    } else {
        rm_relay_sw_state[relay_index] = true;
        on_relay_sw(relay_sw);
    }
    update_rm_param(PARAM_NAME, relay_name, relay_device, esp_rmaker_bool(rm_relay_sw_state[relay_index]));
}

/*!
 *  @brief Set which relay switch to be selected on the M5 interface.
 *  @param call_sw An indexing of the relay switch.
 *                 Index 0: switch_1; Index 1: switch_2
 *                 Index 2: switch_3; Index 3: switch_4
 */
void set_relay_sw(int call_sw) 
{
    for (int i = 0; i < 4; i++) {
        if (i == call_sw) {
            relay_sw_array[call_sw] = true;
        } else {
            relay_sw_array[i] = false;
        }
    }
}

/*!
 *  @brief To set the state of the relay switch in the M5stickCplus interface.
 */
void set_relay_sw_state(void) 
{
    for (int relay_sw = 0; relay_sw < 4; relay_sw++) {
        if (rm_relay_sw_state[relay_sw]) {
            if (relay_sw == RELAY_SW_1) {
                on_relay_sw(relay_sw_1);
            } else if (relay_sw == RELAY_SW_2) {
                on_relay_sw(relay_sw_2);
            } else if (relay_sw == RELAY_SW_3) {
                on_relay_sw(relay_sw_3);
            } else if (relay_sw == RELAY_SW_4) {
                on_relay_sw(relay_sw_4);
            }
        } else {
            if (relay_sw == RELAY_SW_1) {
                off_relay_sw(relay_sw_1);
            } else if (relay_sw == RELAY_SW_2) {
                off_relay_sw(relay_sw_2);
            } else if (relay_sw == RELAY_SW_3) {
                off_relay_sw(relay_sw_3);
            } else if (relay_sw == RELAY_SW_4) {
                off_relay_sw(relay_sw_4);
            }
        }
    }
    set_relay();
}

/*!
 *  @brief To disconnect the relay module from the current task. Switch all relay to off,
 *         then turning deinitialising i2c signal else. set FastLED to default state.
 */
void disc_relay(void) 
{
    if (is_relay_scrn) {
        set_lighting_state(DEFAULT_SWITCH_POWER);
        set_relay_off();
        deinit_i2c();
    } else {
        set_lighting_state(DEFAULT_SWITCH_POWER);
    }
}

/*!
 *  @brief Callback function for selection of relay switch.
 *  @param event Logs in the type of event that is driven by the type of example and button pressed.
 */
void relay_sw_select_cb(lv_event_t *event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    if (is_relay_scrn && code == LV_EVENT_SHORT_CLICKED) {
        if (get_relay_sw_status(RELAY_SW_1)) {
            set_relay_sw(RELAY_SW_2);
            set_indicator_color(relay_sw_1, COLOR_WHITE);
            set_indicator_color(relay_sw_2, COLOR_ORANGE);
        } else if (get_relay_sw_status(RELAY_SW_2)) {
            set_relay_sw(RELAY_SW_3);
            set_indicator_color(relay_sw_2, COLOR_WHITE);
            set_indicator_color(relay_sw_3, COLOR_ORANGE);
        } else if (get_relay_sw_status(RELAY_SW_3)) {
            set_relay_sw(RELAY_SW_4);
            set_indicator_color(relay_sw_3, COLOR_WHITE);
            set_indicator_color(relay_sw_4, COLOR_ORANGE);
        } else if (get_relay_sw_status(RELAY_SW_4)) {
            set_relay_sw(RELAY_SW_1);
            set_indicator_color(relay_sw_4, COLOR_WHITE);
            set_indicator_color(relay_sw_1, COLOR_ORANGE);
        }
    }
}

/*!
 *  @brief Callback function for relay switch.
 *  @param event Logs in the type of event that is driven by the type of example and button pressed.
 */
void relay_sw_cb(lv_event_t *event) 
{
    if (is_relay_sw_on(RELAY_SW_1, rm_relay_sw_state[RELAY_SW_1])) {
        set_relay_sw_btn(relay_sw_1, "switch_1", RELAY_SW_1, rm_relay_sw_state[RELAY_SW_1]);
    } else if (is_relay_sw_on(RELAY_SW_2, rm_relay_sw_state[RELAY_SW_2])) {
        set_relay_sw_btn(relay_sw_2, "switch_2", RELAY_SW_2, rm_relay_sw_state[RELAY_SW_2]);
    } else if (is_relay_sw_on(RELAY_SW_3, rm_relay_sw_state[RELAY_SW_3])) {
        set_relay_sw_btn(relay_sw_3, "switch_3", RELAY_SW_3, rm_relay_sw_state[RELAY_SW_3]);
    } else if (is_relay_sw_on(RELAY_SW_4, rm_relay_sw_state[RELAY_SW_4])) {
        set_relay_sw_btn(relay_sw_4, "switch_4", RELAY_SW_4, rm_relay_sw_state[RELAY_SW_4]);
    } else if (is_relay_sw_off(RELAY_SW_1, rm_relay_sw_state[RELAY_SW_1])) {
        set_relay_sw_btn(relay_sw_1, "switch_1", RELAY_SW_1, rm_relay_sw_state[RELAY_SW_1]);
    } else if (is_relay_sw_off(RELAY_SW_2, rm_relay_sw_state[RELAY_SW_2])) {
        set_relay_sw_btn(relay_sw_2, "switch_2", RELAY_SW_2, rm_relay_sw_state[RELAY_SW_2]);
    } else if (is_relay_sw_off(RELAY_SW_3, rm_relay_sw_state[RELAY_SW_3])) {
        set_relay_sw_btn(relay_sw_3, "switch_3", RELAY_SW_3, rm_relay_sw_state[RELAY_SW_3]);
    } else if (is_relay_sw_off(RELAY_SW_4, rm_relay_sw_state[RELAY_SW_4])) {
        set_relay_sw_btn(relay_sw_4, "switch_4", RELAY_SW_4, rm_relay_sw_state[RELAY_SW_4]);
    }
    set_relay();
}