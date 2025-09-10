/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_rmaker_utils.h>
#include <esp_daylight.h>
#include "esp_schedule_internal.h"

static const char *TAG = "esp_schedule";

#define SECONDS_TILL_2020 ((2020 - 1970) * 365 * 24 * 3600)
#define SECONDS_IN_DAY (60 * 60 * 24)

static bool init_done = false;

static int esp_schedule_get_no_of_days(esp_schedule_trigger_t *trigger, struct tm *current_time, struct tm *schedule_time)
{
    /* for day, monday = 0, sunday = 6. */
    int next_day = 0;
    /* struct tm has tm_wday with sunday as 0. Whereas we have monday as 0. Converting struct tm to our format */
    int today = ((current_time->tm_wday + 7 - 1) % 7);

    esp_schedule_days_t today_bit = 1 << today;
    uint8_t repeat_days = trigger->day.repeat_days;
    int current_seconds = (current_time->tm_hour * 60 + current_time->tm_min) * 60 + current_time->tm_sec;
    int schedule_seconds = (schedule_time->tm_hour * 60 + schedule_time->tm_min) * 60;

    /* Handling for one time schedule */
    if (repeat_days == ESP_SCHEDULE_DAY_ONCE) {
        if (schedule_seconds > current_seconds) {
            /* The schedule is today and is yet to go off */
            return 0;
        } else {
            /* The schedule is tomorrow */
            return 1;
        }
    }

    /* Handling for repeating schedules */
    /* Check if it is today */
    if ((repeat_days & today_bit)) {
        if (schedule_seconds > current_seconds) {
            /* The schedule is today and is yet to go off. */
            return 0;
        }
    }
    /* Check if it is this week or next week */
    if ((repeat_days & (today_bit ^ 0xFF)) > today_bit) {
        /* Next schedule is yet to come in this week */
        next_day = ffs(repeat_days & (0xFF << (today + 1))) - 1;
        return (next_day - today);
    } else {
        /* First scheduled day of the next week */
        next_day = ffs(repeat_days) - 1;
        if (next_day == today) {
            /* Same day, next week */
            return 7;
        }
        return (7 - today + next_day);
    }

    ESP_LOGE(TAG, "No of days could not be found. This should not happen.");
    return 0;
}

static uint8_t esp_schedule_get_next_month(esp_schedule_trigger_t *trigger, struct tm *current_time, struct tm *schedule_time)
{
    int current_seconds = (current_time->tm_hour * 60 + current_time->tm_min) * 60 + current_time->tm_sec;
    int schedule_seconds = (schedule_time->tm_hour * 60 + schedule_time->tm_min) * 60;
    /* +1 is because struct tm has months starting from 0, whereas we have them starting from 1 */
    uint8_t current_month = current_time->tm_mon + 1;
    /* -1 because month_bit starts from 0b1. So for January, it should be 1 << 0. And current_month starts from 1. */
    uint16_t current_month_bit = 1 << (current_month - 1);
    uint8_t next_schedule_month = 0;
    uint16_t repeat_months = trigger->date.repeat_months;

    /* Check if month is not specified */
    if (repeat_months == ESP_SCHEDULE_MONTH_ONCE) {
        if (trigger->date.day == current_time->tm_mday) {
            /* The schedule day is same. Check if time has already passed */
            if (schedule_seconds > current_seconds) {
                /* The schedule is today and is yet to go off */
                return current_month;
            } else {
                /* Today's time has passed */
                return (current_month + 1);
            }
        } else if (trigger->date.day > current_time->tm_mday) {
            /* The day is yet to come in this month */
            return current_month;
        } else {
            /* The day has passed in the current month */
            return (current_month + 1);
        }
    }

    /* Check if schedule is not this year itself, it is in future. */
    if (trigger->date.year > (current_time->tm_year + 1900)) {
        /* Find first schedule month of next year */
        next_schedule_month = ffs(repeat_months);
        /* Year will be handled by the caller. So no need to add any additional months */
        return next_schedule_month;
    }

    /* Check if schedule is this month and is yet to come */
    if (current_month_bit & repeat_months) {
        if (trigger->date.day == current_time->tm_mday) {
            /* The schedule day is same. Check if time has already passed */
            if (schedule_seconds > current_seconds) {
                /* The schedule is today and is yet to go off */
                return current_month;
            }
        }
        if (trigger->date.day > current_time->tm_mday) {
            /* The day is yet to come in this month */
            return current_month;
        }
    }

    /* Check if schedule is this year */
    if ((repeat_months & (current_month_bit ^ 0xFFFF)) > current_month_bit) {
        /* Next schedule month is yet to come in this year */
        next_schedule_month = ffs(repeat_months & (0xFFFF << (current_month)));
        return next_schedule_month;
    }

    /* Check if schedule is for this year and does not repeat */
    if (!trigger->date.repeat_every_year) {
        /* For yearly repeating schedules with year=0, treat as repeating */
        if (trigger->date.year != 0 && trigger->date.year <= (current_time->tm_year + 1900)) {
            ESP_LOGE(TAG, "Schedule does not repeat next year, but get_next_month has been called.");
            return 0;
        }
    }

    /* Schedule is not this year */
    /* Find first schedule month of next year */
    next_schedule_month = ffs(repeat_months);
    /* +12 because the schedule is next year */
    return (next_schedule_month + 12);
}

static uint16_t esp_schedule_get_next_year(esp_schedule_trigger_t *trigger, struct tm *current_time, struct tm *schedule_time)
{
    uint16_t current_year = current_time->tm_year + 1900;
    uint16_t schedule_year = trigger->date.year;
    if (schedule_year > current_year) {
        return schedule_year;
    }
    /* If the schedule is set to repeat_every_year, we return the current year */
    /* If the schedule has already passed in this year, we still return current year, as the additional months will be handled in get_next_month */
    return current_year;
}

#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
/* Helper function to calculate solar time for a specific date */
static time_t esp_schedule_calc_solar_time_for_date(const esp_schedule_trigger_t *trigger,
                                                    int year, int month, int day,
                                                    const char *schedule_name)
{
    time_t sunrise_utc, sunset_utc;

    bool calc_ok = esp_daylight_calc_sunrise_sunset_utc(
                       year, month, day,
                       trigger->solar.latitude,
                       trigger->solar.longitude,
                       &sunrise_utc, &sunset_utc);

    if (!calc_ok) {
        ESP_LOGW(TAG, "Failed to calculate sunrise/sunset for date %04d-%02d-%02d for %s (likely polar night/day condition)",
                 year, month, day, schedule_name);
        return 0;
    }

    time_t solar_time = (trigger->type == ESP_SCHEDULE_TYPE_SUNRISE) ? sunrise_utc : sunset_utc;
    return esp_daylight_apply_offset(solar_time, trigger->solar.offset_minutes);
}

/* Helper function to handle logging and timer calculation for solar schedules */
static int32_t esp_schedule_finalize_solar_time(time_t solar_time, time_t now,
                                                const esp_schedule_trigger_t *trigger,
                                                const char *schedule_name)
{
    char time_str[64];
    struct tm schedule_time;

    /* Convert solar time to local time for display and DST handling */
    localtime_r(&solar_time, &schedule_time);

    /* Print schedule time */
    memset(time_str, 0, sizeof(time_str));
    strftime(time_str, sizeof(time_str), "%c %z[%Z]", &schedule_time);
    ESP_LOGI(TAG, "Schedule %s (%s%+d min) will be active on: %s. DST: %s",
             schedule_name,
             (trigger->type == ESP_SCHEDULE_TYPE_SUNRISE) ? "sunrise" : "sunset",
             trigger->solar.offset_minutes,
             time_str, schedule_time.tm_isdst ? "Yes" : "No");

    /* Simple epoch-based timer calculation */
    int32_t timer_seconds = (int32_t)difftime(solar_time, now);

    /* With proactive logic, this should not happen, but log if it does */
    if (timer_seconds < 0) {
        ESP_LOGW(TAG, "Unexpected: Solar schedule time has passed (%" PRId32 " seconds ago). This should have been handled proactively.", -timer_seconds);
        /* Return the negative value to help debug - caller will handle this as error */
    }

    return timer_seconds;
}
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

static uint32_t esp_schedule_get_next_schedule_time_diff(const char *schedule_name, esp_schedule_trigger_t *trigger)
{
    struct tm current_time, schedule_time;
    time_t now;
    char time_str[64];
    int32_t time_diff;

    /* Get current time */
    time(&now);
    /* Handling ESP_SCHEDULE_TYPE_RELATIVE first since it doesn't require any
     * computation based on days, hours, minutes, etc.
     */
    if (trigger->type == ESP_SCHEDULE_TYPE_RELATIVE) {
        /* If next scheduled time is already set, just compute the difference
         * between current time and next scheduled time and return that diff.
         */
        time_t target;
        if (trigger->next_scheduled_time_utc > 0) {
            target = (time_t)trigger->next_scheduled_time_utc;
            time_diff = difftime(target, now);
        } else {
            target = now + (time_t)trigger->relative_seconds;
            time_diff = trigger->relative_seconds;
        }
        localtime_r(&target, &schedule_time);
        trigger->next_scheduled_time_utc = mktime(&schedule_time);
        /* Print schedule time */
        memset(time_str, 0, sizeof(time_str));
        strftime(time_str, sizeof(time_str), "%c %z[%Z]", &schedule_time);
        ESP_LOGI(TAG, "Schedule %s will be active on: %s. DST: %s", schedule_name, time_str, schedule_time.tm_isdst ? "Yes" : "No");
        return time_diff;
    }

    /* Handle solar-based schedules (sunrise/sunset) */
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    if (trigger->type == ESP_SCHEDULE_TYPE_SUNRISE || trigger->type == ESP_SCHEDULE_TYPE_SUNSET) {
        time_t solar_time = 0;

        /* Start with current local time for day calculations */
        localtime_r(&now, &current_time);

        /* Determine schedule pattern using unified approach */
        if (trigger->date.day != 0) {
            /* Date-based solar schedule - check if today's solar time + offset has passed */
            struct tm schedule_time = current_time;
            schedule_time.tm_mday = trigger->date.day;

            /* For same day, check if solar time + offset has passed before deciding year */
            if (trigger->date.day == current_time.tm_mday) {
                uint8_t current_month = current_time.tm_mon + 1;
                uint16_t repeat_months = trigger->date.repeat_months;
                uint16_t current_month_bit = 1 << (current_month - 1);

                /* Check if this month is in the repeat pattern */
                if (current_month_bit & repeat_months) {
                    /* Calculate today's solar time + offset */
                    time_t today_solar = esp_schedule_calc_solar_time_for_date(trigger,
                                                                               current_time.tm_year + 1900,
                                                                               current_time.tm_mon + 1,
                                                                               current_time.tm_mday,
                                                                               schedule_name);

                    /* If today's solar time + offset hasn't passed, use today */
                    if (today_solar > 0 && today_solar > now) {
                        schedule_time.tm_mon = current_time.tm_mon;
                        schedule_time.tm_year = current_time.tm_year;
                    } else {
                        /* Solar time has passed, use next occurrence */
                        schedule_time.tm_mon = esp_schedule_get_next_month(trigger, &current_time, &schedule_time) - 1;
                        schedule_time.tm_year = esp_schedule_get_next_year(trigger, &current_time, &schedule_time) - 1900;
                    }
                } else {
                    /* Not this month, use normal logic */
                    schedule_time.tm_mon = esp_schedule_get_next_month(trigger, &current_time, &schedule_time) - 1;
                    schedule_time.tm_year = esp_schedule_get_next_year(trigger, &current_time, &schedule_time) - 1900;
                }
            } else {
                /* Different day, use normal logic */
                schedule_time.tm_mon = esp_schedule_get_next_month(trigger, &current_time, &schedule_time) - 1;
                schedule_time.tm_year = esp_schedule_get_next_year(trigger, &current_time, &schedule_time) - 1900;
            }

            if (schedule_time.tm_mon < 0) {
                ESP_LOGE(TAG, "Invalid month found for solar schedule: %s", schedule_name);
                return 0;
            }
            if (schedule_time.tm_mon >= 12) {
                schedule_time.tm_year += schedule_time.tm_mon / 12;
                schedule_time.tm_mon = schedule_time.tm_mon % 12;
            }
            mktime(&schedule_time);

            /* Calculate solar time for the determined date */
            solar_time = esp_schedule_calc_solar_time_for_date(trigger,
                                                               schedule_time.tm_year + 1900,
                                                               schedule_time.tm_mon + 1,
                                                               schedule_time.tm_mday,
                                                               schedule_name);

        } else if (trigger->day.repeat_days != 0) {
            /* Day-of-week solar schedule - check if today's solar time + offset has passed */
            struct tm schedule_time = current_time;

            /* Check if today is one of the scheduled days */
            int today = current_time.tm_wday == 0 ? 7 : current_time.tm_wday; /* Convert Sunday from 0 to 7 */
            uint8_t today_bit = 1 << (today - 1); /* Monday=bit0, Tuesday=bit1, etc. */

            if (trigger->day.repeat_days & today_bit) {
                /* Today is a scheduled day, check if solar time + offset has passed */
                time_t today_solar = esp_schedule_calc_solar_time_for_date(trigger,
                                                                           current_time.tm_year + 1900,
                                                                           current_time.tm_mon + 1,
                                                                           current_time.tm_mday,
                                                                           schedule_name);

                /* If today's solar time + offset hasn't passed, use today */
                if (today_solar > 0 && today_solar > now) {
                    /* Use today */
                    solar_time = today_solar;
                } else {
                    /* Solar time has passed, find next scheduled day */
                    int no_of_days = esp_schedule_get_no_of_days(trigger, &current_time, &schedule_time);
                    schedule_time.tm_mday += no_of_days;
                    mktime(&schedule_time);

                    solar_time = esp_schedule_calc_solar_time_for_date(trigger,
                                                                       schedule_time.tm_year + 1900,
                                                                       schedule_time.tm_mon + 1,
                                                                       schedule_time.tm_mday,
                                                                       schedule_name);
                }
            } else {
                /* Today is not a scheduled day, use normal logic */
                int no_of_days = esp_schedule_get_no_of_days(trigger, &current_time, &schedule_time);
                schedule_time.tm_mday += no_of_days;
                mktime(&schedule_time);

                solar_time = esp_schedule_calc_solar_time_for_date(trigger,
                                                                   schedule_time.tm_year + 1900,
                                                                   schedule_time.tm_mon + 1,
                                                                   schedule_time.tm_mday,
                                                                   schedule_name);
            }

        } else {
            /* Single-time solar schedule - use logic similar to regular schedules */
            solar_time = esp_schedule_calc_solar_time_for_date(trigger,
                                                               current_time.tm_year + 1900,
                                                               current_time.tm_mon + 1,
                                                               current_time.tm_mday,
                                                               schedule_name);

            /* If time has passed today, calculate for tomorrow (like regular schedules) */
            if (solar_time > 0 && solar_time <= now) {
                struct tm tomorrow_time;
                localtime_r(&now, &tomorrow_time);  // Use fresh local time
                tomorrow_time.tm_mday += 1;
                mktime(&tomorrow_time);

                solar_time = esp_schedule_calc_solar_time_for_date(trigger,
                                                                   tomorrow_time.tm_year + 1900,
                                                                   tomorrow_time.tm_mon + 1,
                                                                   tomorrow_time.tm_mday,
                                                                   schedule_name);

                /* If tomorrow's time is still in the past (due to large negative offset), try day after tomorrow */
                if (solar_time > 0 && solar_time <= now) {
                    tomorrow_time.tm_mday += 1;
                    mktime(&tomorrow_time);

                    solar_time = esp_schedule_calc_solar_time_for_date(trigger,
                                                                       tomorrow_time.tm_year + 1900,
                                                                       tomorrow_time.tm_mon + 1,
                                                                       tomorrow_time.tm_mday,
                                                                       schedule_name);
                }
            }
        }

        /* Return error if solar calculation failed */
        if (solar_time == 0) {
            ESP_LOGW(TAG, "Solar schedule %s cannot be calculated (no sunrise/sunset at this location/date)", schedule_name);
            return 0;
        }

        /* Calculate timer - should always be positive since we proactively handle past times */
        time_diff = esp_schedule_finalize_solar_time(solar_time, now, trigger, schedule_name);
        if (time_diff < 0) {
            /* This should not happen with proactive logic, but handle gracefully */
            ESP_LOGE(TAG, "Solar schedule time calculation error for %s (got %" PRId32 " seconds)", schedule_name, time_diff);
            return 0;
        }

        /* Store the final scheduled time - use raw UTC solar time for ts field (phone apps need UTC) */
        trigger->next_scheduled_time_utc = solar_time;

        return time_diff;
    }
#endif /* CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT */

    localtime_r(&now, &current_time);

    /* Get schedule time */
    localtime_r(&now, &schedule_time);
    schedule_time.tm_sec = 0;
    schedule_time.tm_min = trigger->minutes;
    schedule_time.tm_hour = trigger->hours;
    mktime(&schedule_time);

    /* Adjust schedule day */
    if (trigger->type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        int no_of_days = 0;
        no_of_days = esp_schedule_get_no_of_days(trigger, &current_time, &schedule_time);
        schedule_time.tm_sec += no_of_days * SECONDS_IN_DAY;
    }
    if (trigger->type == ESP_SCHEDULE_TYPE_DATE) {
        schedule_time.tm_mday = trigger->date.day;
        schedule_time.tm_mon = esp_schedule_get_next_month(trigger, &current_time, &schedule_time) - 1;
        schedule_time.tm_year = esp_schedule_get_next_year(trigger, &current_time, &schedule_time) - 1900;
        if (schedule_time.tm_mon < 0) {
            ESP_LOGE(TAG, "Invalid month found: %d. Setting it to next month.", schedule_time.tm_mon);
            schedule_time.tm_mon = current_time.tm_mon + 1;
        }
        if (schedule_time.tm_mon >= 12) {
            schedule_time.tm_year += schedule_time.tm_mon / 12;
            schedule_time.tm_mon = schedule_time.tm_mon % 12;
        }
    }
    mktime(&schedule_time);

    /* Adjust time according to DST */
    time_t dst_adjust = 0;
    if (!current_time.tm_isdst && schedule_time.tm_isdst) {
        dst_adjust = -3600;
    } else if (current_time.tm_isdst && !schedule_time.tm_isdst) {
        dst_adjust = 3600;
    }
    ESP_LOGD(TAG, "DST adjust seconds: %lld", (long long) dst_adjust);
    schedule_time.tm_sec += dst_adjust;
    mktime(&schedule_time);

    /* Print schedule time */
    memset(time_str, 0, sizeof(time_str));
    strftime(time_str, sizeof(time_str), "%c %z[%Z]", &schedule_time);
    ESP_LOGI(TAG, "Schedule %s will be active on: %s. DST: %s", schedule_name, time_str, schedule_time.tm_isdst ? "Yes" : "No");

    /* Calculate difference */
    time_diff = difftime((mktime(&schedule_time)), mktime(&current_time));

    /* For one time schedules to check for expiry after a reboot. If NVS is enabled, this should be stored in NVS. */
    trigger->next_scheduled_time_utc = mktime(&schedule_time);

    return time_diff;
}

static bool esp_schedule_is_expired(esp_schedule_trigger_t *trigger)
{
    time_t current_timestamp = 0;
    struct tm current_time = {0};
    time(&current_timestamp);
    localtime_r(&current_timestamp, &current_time);

    if (trigger->type == ESP_SCHEDULE_TYPE_RELATIVE) {
        if (trigger->next_scheduled_time_utc > 0 && trigger->next_scheduled_time_utc <= current_timestamp) {
            /* Relative seconds based schedule has expired */
            return true;
        } else if (trigger->next_scheduled_time_utc == 0) {
            /* Schedule has been disabled , so it is as good as expired. */
            return true;
        }
    } else if (trigger->type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        if (trigger->day.repeat_days == ESP_SCHEDULE_DAY_ONCE) {
            if (trigger->next_scheduled_time_utc > 0 && trigger->next_scheduled_time_utc <= current_timestamp) {
                /* One time schedule has expired */
                return true;
            } else if (trigger->next_scheduled_time_utc == 0) {
                /* Schedule has been disabled , so it is as good as expired. */
                return true;
            }
        }
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
    } else if (trigger->type == ESP_SCHEDULE_TYPE_SUNRISE || trigger->type == ESP_SCHEDULE_TYPE_SUNSET) {
        /* Check if this is a single-time solar schedule */
        if (trigger->date.day == 0 && trigger->day.repeat_days == 0) {
            if (trigger->next_scheduled_time_utc > 0 && trigger->next_scheduled_time_utc <= current_timestamp) {
                /* One time solar schedule has expired */
                return true;
            } else if (trigger->next_scheduled_time_utc == 0) {
                /* Schedule has been disabled , so it is as good as expired. */
                return true;
            }
        }
        /* Repeating solar schedules (day-of-week or date-based) never expire - they recalculate */
#endif
    } else if (trigger->type == ESP_SCHEDULE_TYPE_DATE) {
        if (trigger->date.repeat_months == 0) {
            if (trigger->next_scheduled_time_utc > 0 && trigger->next_scheduled_time_utc <= current_timestamp) {
                /* One time schedule has expired */
                return true;
            } else {
                return false;
            }
        }
        if (trigger->date.repeat_every_year == true) {
            return false;
        }

        struct tm schedule_time = {0};
        localtime_r(&current_timestamp, &schedule_time);
        schedule_time.tm_sec = 0;
        schedule_time.tm_min = trigger->minutes;
        schedule_time.tm_hour = trigger->hours;
        schedule_time.tm_mday = trigger->date.day;
        /* For expiry, just check the last month of the repeat_months. */
        /* '-1' because struct tm has months starting from 0 and we have months starting from 1. */
        schedule_time.tm_mon = fls(trigger->date.repeat_months) - 1;
        /* '-1900' because struct tm has number of years after 1900 */
        schedule_time.tm_year = trigger->date.year - 1900;
        time_t schedule_timestamp = mktime(&schedule_time);

        if (schedule_timestamp < current_timestamp) {
            return true;
        }
    } else {
        /* Invalid type. Mark as expired */
        return true;
    }
    return false;
}

static void esp_schedule_stop_timer(esp_schedule_t *schedule)
{
    xTimerStop(schedule->timer, portMAX_DELAY);
}

static void esp_schedule_start_timer(esp_schedule_t *schedule)
{
    time_t current_time = 0;
    time(&current_time);
    if (current_time < SECONDS_TILL_2020) {
        ESP_LOGE(TAG, "Time is not updated");
        return;
    }

    schedule->next_scheduled_time_diff = esp_schedule_get_next_schedule_time_diff(schedule->name, &schedule->trigger);

    /* Check if schedule calculation failed (returns 0) */
    if (schedule->next_scheduled_time_diff == 0) {
        ESP_LOGW(TAG, "Schedule %s calculation failed or returned invalid time. Skipping timer creation.", schedule->name);
        /* Reset timestamp to indicate schedule is not active */
        schedule->trigger.next_scheduled_time_utc = 0;
        return;
    }

    ESP_LOGI(TAG, "Starting a timer for %"PRIu32" seconds for schedule %s", schedule->next_scheduled_time_diff, schedule->name);

    if (schedule->timestamp_cb) {
        schedule->timestamp_cb((esp_schedule_handle_t)schedule, schedule->trigger.next_scheduled_time_utc, schedule->priv_data);
    }

    xTimerStop(schedule->timer, portMAX_DELAY);
    xTimerChangePeriod(schedule->timer, (schedule->next_scheduled_time_diff * 1000) / portTICK_PERIOD_MS, portMAX_DELAY);
}

static void esp_schedule_common_timer_cb(TimerHandle_t timer)
{
    void *priv_data = pvTimerGetTimerID(timer);
    if (priv_data == NULL) {
        return;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)priv_data;
    time_t now;
    time(&now);
    struct tm validity_time;
    char time_str[64] = {0};
    if (schedule->validity.start_time != 0) {
        if (now < schedule->validity.start_time) {
            memset(time_str, 0, sizeof(time_str));
            localtime_r(&schedule->validity.start_time, &validity_time);
            strftime(time_str, sizeof(time_str), "%c %z[%Z]", &validity_time);
            ESP_LOGW(TAG, "Schedule %s skipped. It will be active only after: %s. DST: %s.", schedule->name, time_str, validity_time.tm_isdst ? "Yes" : "No");
            /* TODO: Start the timer such that the next time it triggeres, it will be within the valid window.
             * Currently, it will just keep triggering and then get skipped if not in valid range.
             */
            goto restart_schedule;
        }
    }
    if (schedule->validity.end_time != 0) {
        if (now > schedule->validity.end_time) {
            localtime_r(&schedule->validity.end_time, &validity_time);
            strftime(time_str, sizeof(time_str), "%c %z[%Z]", &validity_time);
            ESP_LOGW(TAG, "Schedule %s skipped. It can't be active after: %s. DST: %s.", schedule->name, time_str, validity_time.tm_isdst ? "Yes" : "No");
            /* Return from here will ensure that the timer does not start again for this schedule */
            return;
        }
    }
    ESP_LOGI(TAG, "Schedule %s triggered", schedule->name);
    if (schedule->trigger_cb) {
        schedule->trigger_cb((esp_schedule_handle_t)schedule, schedule->priv_data);
    }

restart_schedule:

    if (esp_schedule_is_expired(&schedule->trigger)) {
        /* Not deleting the schedule here. Just not starting it again. */
        return;
    }
    esp_schedule_start_timer(schedule);
}

static void esp_schedule_delete_timer(esp_schedule_t *schedule)
{
    xTimerDelete(schedule->timer, portMAX_DELAY);
}

static void esp_schedule_create_timer(esp_schedule_t *schedule)
{
    if (esp_schedule_nvs_is_enabled()) {
        /* This is just used for calculating next_scheduled_time_utc for ESP_SCHEDULE_DAY_ONCE (in case of ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) or for ESP_SCHEDULE_MONTH_ONCE (in case of ESP_SCHEDULE_TYPE_DATE), and only used when NVS is enabled. And if NVS is enabled, time will already be synced and the time will be correctly calculated. */
        schedule->next_scheduled_time_diff = esp_schedule_get_next_schedule_time_diff(schedule->name, &schedule->trigger);
    }

    /* Temporarily setting the timer for 1 (anything greater than 0) tick. This will get changed when xTimerChangePeriod() is called. */
    schedule->timer = xTimerCreate("schedule", 1, pdFALSE, (void *)schedule, esp_schedule_common_timer_cb);
}

esp_err_t esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (schedule_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;

    strcpy(schedule_config->name, schedule->name);
    schedule_config->trigger.type = schedule->trigger.type;
    schedule_config->trigger.hours = schedule->trigger.hours;
    schedule_config->trigger.minutes = schedule->trigger.minutes;
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
        schedule_config->trigger.day.repeat_days = schedule->trigger.day.repeat_days;
    } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DATE) {
        schedule_config->trigger.date.day = schedule->trigger.date.day;
        schedule_config->trigger.date.repeat_months = schedule->trigger.date.repeat_months;
        schedule_config->trigger.date.year = schedule->trigger.date.year;
        schedule_config->trigger.date.repeat_every_year = schedule->trigger.date.repeat_every_year;
    }

    schedule_config->trigger_cb = schedule->trigger_cb;
    schedule_config->timestamp_cb = schedule->timestamp_cb;
    schedule_config->priv_data = schedule->priv_data;
    schedule_config->validity = schedule->validity;
    return ESP_OK;
}

esp_err_t esp_schedule_enable(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_start_timer(schedule);
    return ESP_OK;
}

esp_err_t esp_schedule_disable(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    esp_schedule_stop_timer(schedule);
    /* Disabling a schedule should also reset the next_scheduled_time.
     * It would be re-computed after enabling.
     */
    schedule->trigger.next_scheduled_time_utc = 0;
    return ESP_OK;
}

static esp_err_t esp_schedule_set(esp_schedule_t *schedule, esp_schedule_config_t *schedule_config)
{
    /* Setting everything apart from name. */
    schedule->trigger.type = schedule_config->trigger.type;
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        schedule->trigger.relative_seconds = schedule_config->trigger.relative_seconds;
        schedule->trigger.next_scheduled_time_utc = schedule_config->trigger.next_scheduled_time_utc;
    } else {
        schedule->trigger.hours = schedule_config->trigger.hours;
        schedule->trigger.minutes = schedule_config->trigger.minutes;

        if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DAYS_OF_WEEK) {
            schedule->trigger.day.repeat_days = schedule_config->trigger.day.repeat_days;
        } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_DATE) {
            schedule->trigger.date.day = schedule_config->trigger.date.day;
            schedule->trigger.date.repeat_months = schedule_config->trigger.date.repeat_months;
            schedule->trigger.date.year = schedule_config->trigger.date.year;
            schedule->trigger.date.repeat_every_year = schedule_config->trigger.date.repeat_every_year;
#if CONFIG_ESP_SCHEDULE_ENABLE_DAYLIGHT
        } else if (schedule->trigger.type == ESP_SCHEDULE_TYPE_SUNRISE || schedule->trigger.type == ESP_SCHEDULE_TYPE_SUNSET) {
            schedule->trigger.solar.latitude = schedule_config->trigger.solar.latitude;
            schedule->trigger.solar.longitude = schedule_config->trigger.solar.longitude;
            schedule->trigger.solar.offset_minutes = schedule_config->trigger.solar.offset_minutes;
            /* Copy day and date fields for unified solar schedule approach */
            schedule->trigger.day.repeat_days = schedule_config->trigger.day.repeat_days;
            schedule->trigger.date.day = schedule_config->trigger.date.day;
            schedule->trigger.date.repeat_months = schedule_config->trigger.date.repeat_months;
            schedule->trigger.date.year = schedule_config->trigger.date.year;
            schedule->trigger.date.repeat_every_year = schedule_config->trigger.date.repeat_every_year;
#endif
        }
    }

    schedule->trigger_cb = schedule_config->trigger_cb;
    schedule->timestamp_cb = schedule_config->timestamp_cb;
    schedule->priv_data = schedule_config->priv_data;
    schedule->validity = schedule_config->validity;
    esp_schedule_nvs_add(schedule);
    return ESP_OK;
}

esp_err_t esp_schedule_edit(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config)
{
    if (handle == NULL || schedule_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    if (strncmp(schedule->name, schedule_config->name, sizeof(schedule->name)) != 0) {
        ESP_LOGE(TAG, "Schedule name mismatch. Expected: %s, Passed: %s", schedule->name, schedule_config->name);
        return ESP_FAIL;
    }

    /* Editing a schedule with relative time should also reset it. */
    if (schedule->trigger.type == ESP_SCHEDULE_TYPE_RELATIVE) {
        schedule->trigger.next_scheduled_time_utc = 0;
    }
    esp_schedule_set(schedule, schedule_config);
    ESP_LOGD(TAG, "Schedule %s edited", schedule->name);
    return ESP_OK;
}

esp_err_t esp_schedule_delete(esp_schedule_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_schedule_t *schedule = (esp_schedule_t *)handle;
    ESP_LOGI(TAG, "Deleting schedule %s", schedule->name);
    if (schedule->timer) {
        esp_schedule_stop_timer(schedule);
        esp_schedule_delete_timer(schedule);
    }
    esp_schedule_nvs_remove(schedule);
    free(schedule);
    return ESP_OK;
}

esp_schedule_handle_t esp_schedule_create(esp_schedule_config_t *schedule_config)
{
    if (schedule_config == NULL) {
        return NULL;
    }
    if (strlen(schedule_config->name) <= 0) {
        ESP_LOGE(TAG, "Set schedule failed. Please enter a unique valid name for the schedule.");
        return NULL;
    }

    if (schedule_config->trigger.type == ESP_SCHEDULE_TYPE_INVALID) {
        ESP_LOGE(TAG, "Schedule type is invalid.");
        return NULL;
    }

    esp_schedule_t *schedule = (esp_schedule_t *)MEM_CALLOC_EXTRAM(1, sizeof(esp_schedule_t));
    if (schedule == NULL) {
        ESP_LOGE(TAG, "Could not allocate handle");
        return NULL;
    }
    strlcpy(schedule->name, schedule_config->name, sizeof(schedule->name));

    esp_schedule_set(schedule, schedule_config);

    esp_schedule_create_timer(schedule);
    ESP_LOGD(TAG, "Schedule %s created", schedule->name);
    return (esp_schedule_handle_t)schedule;
}

esp_schedule_handle_t *esp_schedule_init(bool enable_nvs, char *nvs_partition, uint8_t *schedule_count)
{
    if (!esp_sntp_enabled()) {
        ESP_LOGI(TAG, "Initializing SNTP");
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
    }

    if (!enable_nvs) {
        return NULL;
    }

    /* Wait for time to be updated here */

    /* Below this is initialising schedules from NVS */
    esp_schedule_nvs_init(nvs_partition);

    /* Get handle list from NVS */
    esp_schedule_handle_t *handle_list = NULL;
    *schedule_count = 0;
    handle_list = esp_schedule_nvs_get_all(schedule_count);
    if (handle_list == NULL) {
        ESP_LOGI(TAG, "No schedules found in NVS");
        return NULL;
    }
    ESP_LOGI(TAG, "Schedules found in NVS: %"PRIu8, *schedule_count);
    /* Start/Delete the schedules */
    esp_schedule_t *schedule = NULL;
    for (size_t handle_count = 0; handle_count < *schedule_count; handle_count++) {
        schedule = (esp_schedule_t *)handle_list[handle_count];
        schedule->trigger_cb = NULL;
        schedule->timer = NULL;
        /* Check for ONCE and expired schedules and delete them. */
        if (esp_schedule_is_expired(&schedule->trigger)) {
            /* This schedule has already expired. */
            ESP_LOGI(TAG, "Schedule %s does not repeat and has already expired. Deleting it.", schedule->name);
            esp_schedule_delete((esp_schedule_handle_t)schedule);
            /* Removing the schedule from the list */
            handle_list[handle_count] = handle_list[*schedule_count - 1];
            (*schedule_count)--;
            handle_count--;
            continue;
        }
        esp_schedule_create_timer(schedule);
        esp_schedule_start_timer(schedule);
    }
    init_done = true;
    return handle_list;
}
