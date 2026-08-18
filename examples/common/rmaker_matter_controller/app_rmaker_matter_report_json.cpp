/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app_rmaker_matter_report_json.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static void cjson_hexify_matter_keys(cJSON *item)
{
    if (!item) {
        return;
    }
    if (cJSON_IsObject(item)) {
        cJSON *child = item->child;
        while (child) {
            cJSON *next = child->next;
            const char *key = child->string;
            if (key && key[0] != '\0') {
                bool need_hex = false;
                if (strncmp(key, "0x", 2) != 0 && strncmp(key, "0X", 2) != 0) {
                    char *end = NULL;
                    (void)strtoul(key, &end, 10);
                    if (end && end > key && *end == '\0') {
                        need_hex = true;
                    }
                }
                if (need_hex) {
                    char new_key[32];
                    unsigned long v = strtoul(key, NULL, 10);
                    snprintf(new_key, sizeof(new_key), "0x%lX", v);
                    cJSON *detached = cJSON_DetachItemViaPointer(item, child);
                    cjson_hexify_matter_keys(detached);
                    cJSON_AddItemToObject(item, new_key, detached);
                } else {
                    cjson_hexify_matter_keys(child);
                }
            }
            child = next;
        }
    } else if (cJSON_IsArray(item)) {
        cJSON *el = NULL;
        cJSON_ArrayForEach(el, item) {
            cjson_hexify_matter_keys(el);
        }
    }
}

static cJSON *get_tree_item(const cJSON *root, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id)
{
    char ep_key[24];
    char cluster_key[24];
    char attr_key[24];
    snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
    snprintf(cluster_key, sizeof(cluster_key), "0x%" PRIX32, cluster_id);
    snprintf(attr_key, sizeof(attr_key), "0x%" PRIX32, attribute_id);

    const cJSON *ep_obj = cJSON_GetObjectItem(root, ep_key);
    const cJSON *clusters_obj = ep_obj ? cJSON_GetObjectItem(ep_obj, "clusters") : NULL;
    const cJSON *servers_obj = clusters_obj ? cJSON_GetObjectItem(clusters_obj, "servers") : NULL;
    const cJSON *cluster_wrap = servers_obj ? cJSON_GetObjectItem(servers_obj, cluster_key) : NULL;
    const cJSON *attr_obj = cluster_wrap ? cJSON_GetObjectItem(cluster_wrap, "attributes") : NULL;
    return attr_obj ? cJSON_GetObjectItem(attr_obj, attr_key) : NULL;
}

bool app_rmaker_matter_report_json_tree_item_matches(const cJSON *root, uint16_t endpoint_id, uint32_t cluster_id,
                                                     uint32_t attribute_id, const cJSON *value)
{
    const cJSON *old_item = get_tree_item(root, endpoint_id, cluster_id, attribute_id);
    if (!old_item) {
        return false;
    }
    cJSON *new_item = value ? cJSON_Duplicate(value, true) : cJSON_CreateNull();
    if (!new_item) {
        return false;
    }
    cjson_hexify_matter_keys(new_item);
    bool equal = cJSON_Compare(old_item, new_item, true);
    cJSON_Delete(new_item);
    return equal;
}

bool app_rmaker_matter_report_json_update_tree_item(cJSON *root, uint16_t endpoint_id, uint32_t cluster_id,
                                                  uint32_t attribute_id, const cJSON *value)
{
    char ep_key[24];
    char cluster_key[24];
    char attr_key[24];
    snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
    snprintf(cluster_key, sizeof(cluster_key), "0x%" PRIX32, cluster_id);
    snprintf(attr_key, sizeof(attr_key), "0x%" PRIX32, attribute_id);

    cJSON *ep_obj = cJSON_GetObjectItem(root, ep_key);
    if (!cJSON_IsObject(ep_obj)) {
        cJSON *new_ep_obj = cJSON_CreateObject();
        if (!new_ep_obj) {
            return false;
        }
        bool updated = ep_obj ? cJSON_ReplaceItemInObjectCaseSensitive(root, ep_key, new_ep_obj) :
                                cJSON_AddItemToObject(root, ep_key, new_ep_obj);
        if (!updated) {
            cJSON_Delete(new_ep_obj);
            return false;
        }
        ep_obj = new_ep_obj;
    }
    cJSON *clusters_obj = cJSON_GetObjectItem(ep_obj, "clusters");
    if (!clusters_obj) {
        clusters_obj = cJSON_CreateObject();
        if (!clusters_obj) {
            return false;
        }
        cJSON_AddItemToObject(ep_obj, "clusters", clusters_obj);
    }
    cJSON *servers_obj = cJSON_GetObjectItem(clusters_obj, "servers");
    if (!servers_obj) {
        servers_obj = cJSON_CreateObject();
        if (!servers_obj) {
            return false;
        }
        cJSON_AddItemToObject(clusters_obj, "servers", servers_obj);
    }
    cJSON *cluster_wrap = cJSON_GetObjectItem(servers_obj, cluster_key);
    if (!cluster_wrap) {
        cluster_wrap = cJSON_CreateObject();
        if (!cluster_wrap) {
            return false;
        }
        cJSON_AddItemToObject(servers_obj, cluster_key, cluster_wrap);
    }
    cJSON *attr_obj = cJSON_GetObjectItem(cluster_wrap, "attributes");
    if (!attr_obj) {
        attr_obj = cJSON_CreateObject();
        if (!attr_obj) {
            return false;
        }
        cJSON_AddItemToObject(cluster_wrap, "attributes", attr_obj);
    }

    cJSON *old_item = cJSON_DetachItemFromObject(attr_obj, attr_key);
    cJSON *new_item = value ? cJSON_Duplicate(value, true) : cJSON_CreateNull();
    if (!new_item) {
        if (old_item) {
            cJSON_AddItemToObject(attr_obj, attr_key, old_item);
        }
        return false;
    }
    cjson_hexify_matter_keys(new_item);

    bool changed = !old_item || !cJSON_Compare(old_item, new_item, true);
    if (!changed) {
        cJSON_Delete(new_item);
        cJSON_AddItemToObject(attr_obj, attr_key, old_item);
        return false;
    }

    cJSON_Delete(old_item);
    cJSON_AddItemToObject(attr_obj, attr_key, new_item);
    return true;
}

bool app_rmaker_matter_report_json_pending_is_empty(cJSON *pending_root)
{
    return !pending_root || !pending_root->child;
}

static cJSON *get_or_create_pending_endpoints(cJSON **pending_root, uint64_t node_id,
                                              const char *rainmaker_node_id)
{
    if (!pending_root) {
        return NULL;
    }
    if (!*pending_root) {
        *pending_root = cJSON_CreateObject();
        if (!*pending_root) {
            return NULL;
        }
    }

    char node_key[32];
    snprintf(node_key, sizeof(node_key), "%016llx", (unsigned long long)node_id);
    cJSON *wrapper = cJSON_GetObjectItem(*pending_root, node_key);
    if (!wrapper) {
        wrapper = cJSON_CreateObject();
        cJSON *rainmaker_id = cJSON_CreateString(rainmaker_node_id ? rainmaker_node_id : "");
        cJSON *endpoints = cJSON_CreateObject();
        if (!wrapper || !rainmaker_id || !endpoints) {
            cJSON_Delete(endpoints);
            cJSON_Delete(rainmaker_id);
            cJSON_Delete(wrapper);
            return NULL;
        }
        if (!cJSON_AddItemToObject(wrapper, "rainmaker_node_id", rainmaker_id)) {
            cJSON_Delete(endpoints);
            cJSON_Delete(rainmaker_id);
            cJSON_Delete(wrapper);
            return NULL;
        }
        if (!cJSON_AddItemToObject(wrapper, "endpoints", endpoints)) {
            cJSON_Delete(endpoints);
            cJSON_Delete(wrapper);
            return NULL;
        }
        if (!cJSON_AddItemToObject(*pending_root, node_key, wrapper)) {
            cJSON_Delete(wrapper);
            return NULL;
        }
        return endpoints;
    }
    cJSON *endpoints = cJSON_GetObjectItem(wrapper, "endpoints");
    if (!endpoints) {
        endpoints = cJSON_CreateObject();
        if (!endpoints || !cJSON_AddItemToObject(wrapper, "endpoints", endpoints)) {
            cJSON_Delete(endpoints);
            return NULL;
        }
    }
    return endpoints;
}

bool app_rmaker_matter_report_json_merge_pending_attr_item(cJSON **pending_root, uint64_t node_id,
                                                         const char *rainmaker_node_id, uint16_t endpoint_id,
                                                         uint32_t cluster_id, uint32_t attribute_id,
                                                         const cJSON *value)
{
    cJSON *endpoints = get_or_create_pending_endpoints(pending_root, node_id, rainmaker_node_id);
    if (!endpoints) {
        return false;
    }
    if (app_rmaker_matter_report_json_tree_item_matches(endpoints, endpoint_id, cluster_id, attribute_id, value)) {
        return true;
    }
    return app_rmaker_matter_report_json_update_tree_item(endpoints, endpoint_id, cluster_id, attribute_id, value);
}

bool app_rmaker_matter_report_json_merge_pending_endpoint_tombstone(cJSON **pending_root, uint64_t node_id,
                                                                    const char *rainmaker_node_id,
                                                                    uint16_t endpoint_id)
{
    cJSON *endpoints = get_or_create_pending_endpoints(pending_root, node_id, rainmaker_node_id);
    if (!endpoints) {
        return false;
    }

    char ep_key[24];
    snprintf(ep_key, sizeof(ep_key), "0x%X", (unsigned)endpoint_id);
    cJSON *old_item = cJSON_GetObjectItem(endpoints, ep_key);
    if (cJSON_IsNull(old_item)) {
        return true;
    }

    cJSON *new_item = cJSON_CreateNull();
    if (!new_item) {
        return false;
    }
    bool updated = old_item ? cJSON_ReplaceItemInObjectCaseSensitive(endpoints, ep_key, new_item) :
                              cJSON_AddItemToObject(endpoints, ep_key, new_item);
    if (!updated) {
        cJSON_Delete(new_item);
        return false;
    }
    return true;
}

cJSON *app_rmaker_matter_report_json_detach_pending_all(cJSON **pending_root)
{
    if (!pending_root || app_rmaker_matter_report_json_pending_is_empty(*pending_root)) {
        return NULL;
    }
    cJSON *payload = *pending_root;
    *pending_root = NULL;
    return payload;
}

cJSON *app_rmaker_matter_report_json_detach_pending_node(cJSON **pending_root, uint64_t node_id)
{
    if (!pending_root || app_rmaker_matter_report_json_pending_is_empty(*pending_root)) {
        return NULL;
    }
    char node_key[32];
    snprintf(node_key, sizeof(node_key), "%016llx", (unsigned long long)node_id);
    cJSON *node = cJSON_DetachItemFromObject(*pending_root, node_key);
    if (!node) {
        return NULL;
    }
    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        cJSON_Delete(node);
        return NULL;
    }
    cJSON_AddItemToObject(payload, node_key, node);
    if (app_rmaker_matter_report_json_pending_is_empty(*pending_root)) {
        cJSON_Delete(*pending_root);
        *pending_root = NULL;
    }
    return payload;
}
