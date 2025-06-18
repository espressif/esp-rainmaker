/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

// #include <esp_err.h>
// #include <esp_matter.h>
// #include <matter_controller_device_mgr.h>
#include <unordered_map>


// #ifdef __cplusplus
// extern "C" {
// #endif


typedef struct attributes
{
    uint32_t attribute_id;

    esp_matter_attr_val_t esp_value;

    attributes(uint32_t attributeid, esp_matter_attr_val_t& esp_val)
    {
        this->attribute_id = attributeid;
        this->esp_value = *(&esp_val);
    }
}cl_attribute;

typedef struct clusters //cluster details
{
    uint32_t cluster_id;
    std::unordered_map<uint32_t, cl_attribute* > get_attribute_ptr;
    std::vector<std::pair<uint32_t,bool>> events_list; //events is cluster specific
    clusters(uint32_t clusterid) : cluster_id(clusterid) {}
}ep_cluster;


typedef struct cloud_data_model //endpoint details
{
    uint64_t node_id;
    uint16_t endpoint_id;
    uint32_t device_type;
    std::unordered_map<uint32_t, ep_cluster* > get_cluster_ptr; //server_clusters
    std::vector<uint32_t> client_list; //client_clusters
    cloud_data_model(uint64_t remote_nodeid, uint16_t endpointid) : node_id(remote_nodeid),endpoint_id(endpointid){}

}data_model;

typedef struct dev_data
{
    uint64_t node_id;
    bool enabled = true;
    bool reachable = true;
    std::unordered_map<uint16_t, data_model*> get_endpoint_ptr;

    dev_data(uint64_t remote_nodeid): node_id(remote_nodeid){}

}dev_data;

typedef struct callback_data
{
    uint64_t node_id;
    chip::app::ConcreteDataAttributePath attr_path;
    chip::TLV::TLVReader* tlv_data;

    callback_data(uint64_t nodeid, chip::app::ConcreteDataAttributePath attrpath, chip::TLV::TLVReader * tlvdata): node_id(nodeid),attr_path(attrpath), tlv_data(tlvdata){}

}cb_data;

void print_data_model();

void clear_data_model();

void report_data_model();

esp_err_t change_data_model_attribute(uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t*);

esp_err_t change_node_reachability(uint64_t node_id, bool);

void read_node_info(std::vector<uint64_t>);

// #ifdef __cplusplus
// }
// #endif


