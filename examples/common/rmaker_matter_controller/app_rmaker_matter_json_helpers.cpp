/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app_rmaker_matter_json_helpers.h>

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <lib/core/TLV.h>
#include <lib/core/TLVReader.h>

static bool appendf(char *buf, size_t buf_size, size_t &pos, const char *fmt, ...)
{
    if (!buf || buf_size == 0 || pos >= buf_size) {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + pos, buf_size - pos, fmt, args);
    va_end(args);
    if (written < 0) {
        return false;
    }
    if (static_cast<size_t>(written) >= (buf_size - pos)) {
        pos = buf_size - 1;
        buf[pos] = '\0';
        return false;
    }
    pos += static_cast<size_t>(written);
    return true;
}

static void append_json_escaped_charspan(const chip::CharSpan &str, char *buf, size_t buf_size, size_t &pos)
{
    appendf(buf, buf_size, pos, "\"");
    for (size_t i = 0; i < str.size(); ++i) {
        const char c = str.data()[i];
        if (c == '"' || c == '\\') {
            appendf(buf, buf_size, pos, "\\%c", c);
        } else if (c == '\n') {
            appendf(buf, buf_size, pos, "\\n");
        } else if (c == '\r') {
            appendf(buf, buf_size, pos, "\\r");
        } else if (c == '\t') {
            appendf(buf, buf_size, pos, "\\t");
        } else if (static_cast<unsigned char>(c) < 0x20) {
            appendf(buf, buf_size, pos, "\\u%04x", static_cast<unsigned char>(c));
        } else {
            appendf(buf, buf_size, pos, "%c", c);
        }
    }
    appendf(buf, buf_size, pos, "\"");
}

static bool decode_tlv_reader_to_string(chip::TLV::TLVReader &reader, char *buf, size_t buf_size, size_t &pos, int depth)
{
    if (depth > 8) {
        return appendf(buf, buf_size, pos, "\"max_depth\"");
    }

    switch (reader.GetType()) {
    case chip::TLV::kTLVType_Boolean: {
        bool v = false;
        return reader.Get(v) == CHIP_NO_ERROR ? appendf(buf, buf_size, pos, "%s", v ? "true" : "false")
                                              : appendf(buf, buf_size, pos, "\"bool_decode_failed\"");
    }
    case chip::TLV::kTLVType_SignedInteger: {
        int64_t v = 0;
        return reader.Get(v) == CHIP_NO_ERROR ? appendf(buf, buf_size, pos, "%" PRId64, v)
                                              : appendf(buf, buf_size, pos, "\"int_decode_failed\"");
    }
    case chip::TLV::kTLVType_UnsignedInteger: {
        uint64_t v = 0;
        return reader.Get(v) == CHIP_NO_ERROR ? appendf(buf, buf_size, pos, "%" PRIu64, v)
                                              : appendf(buf, buf_size, pos, "\"uint_decode_failed\"");
    }
    case chip::TLV::kTLVType_FloatingPointNumber: {
        double v = 0.0;
        if (reader.Get(v) == CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%g", v);
        }
        float fv = 0.0f;
        return reader.Get(fv) == CHIP_NO_ERROR ? appendf(buf, buf_size, pos, "%g", static_cast<double>(fv))
                                               : appendf(buf, buf_size, pos, "\"float_decode_failed\"");
    }
    case chip::TLV::kTLVType_UTF8String: {
        chip::CharSpan str;
        if (reader.Get(str) == CHIP_NO_ERROR) {
            append_json_escaped_charspan(str, buf, buf_size, pos);
            return true;
        }
        return appendf(buf, buf_size, pos, "\"string_decode_failed\"");
    }
    case chip::TLV::kTLVType_ByteString: {
        chip::ByteSpan bytes;
        if (reader.Get(bytes) != CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "\"bytes_decode_failed\"");
        }
        appendf(buf, buf_size, pos, "\"0x");
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (!appendf(buf, buf_size, pos, "%02x", bytes.data()[i])) {
                appendf(buf, buf_size, pos, "...");
                break;
            }
        }
        return appendf(buf, buf_size, pos, "\"");
    }
    case chip::TLV::kTLVType_Null:
        return appendf(buf, buf_size, pos, "null");
    case chip::TLV::kTLVType_Structure:
    case chip::TLV::kTLVType_Array:
    case chip::TLV::kTLVType_List: {
        const bool is_object_like = (reader.GetType() == chip::TLV::kTLVType_Structure);
        appendf(buf, buf_size, pos, "%c", is_object_like ? '{' : '[');

        chip::TLV::TLVType outer_container_type = chip::TLV::kTLVType_NotSpecified;
        if (reader.EnterContainer(outer_container_type) != CHIP_NO_ERROR) {
            return appendf(buf, buf_size, pos, "%c", is_object_like ? '}' : ']');
        }
        bool first = true;
        while (reader.Next() == CHIP_NO_ERROR) {
            if (!first) {
                appendf(buf, buf_size, pos, ",");
            }
            first = false;
            if (is_object_like) {
                if (chip::TLV::IsContextTag(reader.GetTag())) {
                    appendf(buf, buf_size, pos, "\"%" PRIu32 "\":",
                            static_cast<uint32_t>(chip::TLV::TagNumFromTag(reader.GetTag())));
                } else {
                    appendf(buf, buf_size, pos, "\"tag\":");
                }
            }
            if (!decode_tlv_reader_to_string(reader, buf, buf_size, pos, depth + 1)) {
                break;
            }
        }
        reader.ExitContainer(outer_container_type);
        return appendf(buf, buf_size, pos, "%c", is_object_like ? '}' : ']');
    }
    default:
        return appendf(buf, buf_size, pos, "\"unsupported_tlv_type\"");
    }
}

esp_err_t app_rmaker_matter_tlv_to_json(chip::TLV::TLVReader &reader, cJSON **json)
{
    if (!json) {
        return ESP_ERR_INVALID_ARG;
    }
    *json = NULL;

    char json_buf[384] = {0};
    size_t pos = 0;
    chip::TLV::TLVReader copy;
    copy.Init(reader);
    if (!decode_tlv_reader_to_string(copy, json_buf, sizeof(json_buf), pos, 0)) {
        return ESP_FAIL;
    }
    *json = cJSON_Parse(json_buf);
    if (!*json) {
        *json = cJSON_CreateString(json_buf);
    }
    return *json ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t app_rmaker_matter_tlv_to_json_string(chip::TLV::TLVReader *reader, char *buf, size_t buf_size)
{
    if (!reader || !buf || buf_size == 0) {
        if (buf && buf_size > 0) {
            buf[0] = '\0';
        }
        return ESP_ERR_INVALID_ARG;
    }
    chip::TLV::TLVReader copy;
    copy.Init(*reader);
    size_t pos = 0;
    bool ok = decode_tlv_reader_to_string(copy, buf, buf_size, pos, 0);
    if (pos < buf_size) {
        buf[pos] = '\0';
    } else {
        buf[buf_size - 1] = '\0';
    }
    return ok ? ESP_OK : ESP_FAIL;
}
