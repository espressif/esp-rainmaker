/* Code taken from: https://github.com/shahpiyushv/json_gen_generator */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <json_generator.h>
/* Logging management */
//#define JSON_GEN_DEBUG

#ifdef JSON_GEN_DEBUG
  #define json_gen_gen_d(fmt, ...)     printf("[json_gen_gen] " fmt, ##__VA_ARGS__);
#else
  #define json_gen_gen_d(fmt, ...)
#endif

#define MAX_INT_IN_STR  	24
#define MAX_FLOAT_IN_STR 	30

static inline int json_gen_get_empty_len(json_gen_str_t *jstr)
{
	return (jstr->buf_size - (jstr->free_ptr - jstr->buf) - 1);
}

/* This will add the incoming string to the JSON string buffer
 * and flush it out if the buffer is full. Note that the data being
 * flushed out will always be equal to the size of the buffer unless
 * this is the last chunk being flushed out on json_gen_end_str()
 */
static int json_gen_add_to_str(json_gen_str_t *jstr, char *str)
{
    if (!str) {
        return 0;
    }
	int len = strlen(str);
	char *cur_ptr = str;
	while (1) {
		int len_remaining = json_gen_get_empty_len(jstr);
		int copy_len = len_remaining > len ? len : len_remaining;
		memmove(jstr->free_ptr, cur_ptr, copy_len);
		cur_ptr += copy_len;
		jstr->free_ptr += copy_len;
		len -= copy_len;
		if (len) {
			*jstr->free_ptr = '\0';
			/* Report error if the buffer is full and no flush callback
			 * is registered
			 */
			if (!jstr->flush_cb) {
				json_gen_gen_d("Cannot flush. End of string");
				return -1;
			}
			jstr->flush_cb(jstr->buf, jstr->priv);
			jstr->free_ptr = jstr->buf;
		} else
			break;
	}
	return 0;
}


void json_gen_str_start(json_gen_str_t *jstr, char *buf, int buf_size,
		json_gen_flush_cb_t flush_cb, void *priv)
{
	memset(jstr, 0, sizeof(json_gen_str_t));
	jstr->buf = buf;
	jstr->buf_size = buf_size;
	jstr->flush_cb = flush_cb;
	jstr->free_ptr = buf;
	jstr->priv = priv;
}

void json_gen_str_end(json_gen_str_t *jstr)
{
	*jstr->free_ptr = '\0';
	if (jstr->flush_cb)
		jstr->flush_cb(jstr->buf, jstr->priv);
	memset(jstr, 0, sizeof(json_gen_str_t));
}

static inline void json_gen_handle_comma(json_gen_str_t *jstr)
{
	if (jstr->comma_req)
		json_gen_add_to_str(jstr, ",");
}


static int json_gen_handle_name(json_gen_str_t *jstr, char *name)
{
	json_gen_add_to_str(jstr, "\"");
	json_gen_add_to_str(jstr, name);
	return json_gen_add_to_str(jstr, "\":");
}


int json_gen_start_object(json_gen_str_t *jstr)
{
	json_gen_handle_comma(jstr);
	jstr->comma_req = false;
	return json_gen_add_to_str(jstr, "{");
}

int json_gen_end_object(json_gen_str_t *jstr)
{
	jstr->comma_req = true;
	return json_gen_add_to_str(jstr, "}");
}


int json_gen_start_array(json_gen_str_t *jstr)
{
	json_gen_handle_comma(jstr);
	jstr->comma_req = false;
	return json_gen_add_to_str(jstr, "[");
}

int json_gen_end_array(json_gen_str_t *jstr)
{
	jstr->comma_req = true;
	return json_gen_add_to_str(jstr, "]");
}

int json_gen_push_object(json_gen_str_t *jstr, char *name)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	jstr->comma_req = false;
	return json_gen_add_to_str(jstr, "{");
}
int json_gen_pop_object(json_gen_str_t *jstr)
{
	jstr->comma_req = true;
	return json_gen_add_to_str(jstr, "}");
}
int json_gen_push_array(json_gen_str_t *jstr, char *name)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	jstr->comma_req = false;
	return json_gen_add_to_str(jstr, "[");
}
int json_gen_pop_array(json_gen_str_t *jstr)
{
	jstr->comma_req = true;
	return json_gen_add_to_str(jstr, "]");
}

static int json_gen_set_bool(json_gen_str_t *jstr, bool val)
{
	jstr->comma_req = true;
	if (val)
		return json_gen_add_to_str(jstr, "true");
	else
		return json_gen_add_to_str(jstr, "false");
}
int json_gen_obj_set_bool(json_gen_str_t *jstr, char *name, bool val)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	return json_gen_set_bool(jstr, val);
}

int json_gen_arr_set_bool(json_gen_str_t *jstr, bool val)
{
	json_gen_handle_comma(jstr);
	return json_gen_set_bool(jstr, val);
}

static int json_gen_set_int(json_gen_str_t *jstr, int val)
{
	jstr->comma_req = true;
	char str[MAX_INT_IN_STR];
	snprintf(str, MAX_INT_IN_STR, "%d", val);
	return json_gen_add_to_str(jstr, str);
}

int json_gen_obj_set_int(json_gen_str_t *jstr, char *name, int val)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	return json_gen_set_int(jstr, val);
}

int json_gen_arr_set_int(json_gen_str_t *jstr, int val)
{
	json_gen_handle_comma(jstr);
	return json_gen_set_int(jstr, val);
}


static int json_gen_set_float(json_gen_str_t *jstr, float val)
{
	jstr->comma_req = true;
	char str[MAX_FLOAT_IN_STR];
	snprintf(str, MAX_FLOAT_IN_STR, "%.*f", JSON_FLOAT_PRECISION, val);
	return json_gen_add_to_str(jstr, str);
}
int json_gen_obj_set_float(json_gen_str_t *jstr, char *name, float val)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	return json_gen_set_float(jstr, val);
}
int json_gen_arr_set_float(json_gen_str_t *jstr, float val)
{
	json_gen_handle_comma(jstr);
	return json_gen_set_float(jstr, val);
}

static int json_gen_set_string(json_gen_str_t *jstr, char *val)
{
	jstr->comma_req = true;
	json_gen_add_to_str(jstr, "\"");
	json_gen_add_to_str(jstr, val);
	return json_gen_add_to_str(jstr, "\"");
}

int json_gen_obj_set_string(json_gen_str_t *jstr, char *name, char *val)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	return json_gen_set_string(jstr, val);
}

int json_gen_arr_set_string(json_gen_str_t *jstr, char *val)
{
	json_gen_handle_comma(jstr);
	return json_gen_set_string(jstr, val);
}

static int json_gen_set_long_string(json_gen_str_t *jstr, char *val)
{
	jstr->comma_req = true;
	json_gen_add_to_str(jstr, "\"");
	return json_gen_add_to_str(jstr, val);
}

int json_gen_obj_start_long_string(json_gen_str_t *jstr, char *name, char *val)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
    return json_gen_set_long_string(jstr, val);
}

int json_gen_arr_start_long_string(json_gen_str_t *jstr, char *val)
{
	json_gen_handle_comma(jstr);
    return json_gen_set_long_string(jstr, val);
}

int json_gen_add_to_long_string(json_gen_str_t *jstr, char *val)
{
    return json_gen_add_to_str(jstr, val);
}

int json_gen_end_long_string(json_gen_str_t *jstr)
{
    return json_gen_add_to_str(jstr, "\"");
}
static int json_gen_set_null(json_gen_str_t *jstr)
{
	jstr->comma_req = true;
	return json_gen_add_to_str(jstr, "null");
}
int json_gen_obj_set_null(json_gen_str_t *jstr, char *name)
{
	json_gen_handle_comma(jstr);
	json_gen_handle_name(jstr, name);
	return json_gen_set_null(jstr);
}

int json_gen_arr_set_null(json_gen_str_t *jstr)
{
	json_gen_handle_comma(jstr);
	return json_gen_set_null(jstr);
}
