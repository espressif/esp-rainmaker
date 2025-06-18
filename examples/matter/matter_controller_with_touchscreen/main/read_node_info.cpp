#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <device.h>
#include <esp_check.h>
#include <esp_matter.h>
#include <led_driver.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <app_matter.h>
#include <matter_controller_device_mgr.h>
#include <esp_matter_controller_cluster_command.h>
#include <esp_matter_controller_read_command.h>
#include <esp_matter_controller_subscribe_command.h>
#include <commands/clusters/DataModelLogger.h>
#include <esp_matter_core.h>

#include "app_matter_ctrl.h"

#include <iostream>
#include <read_node_info.h>

#include <unordered_map>

#include "freertos/semphr.h"

#include <json_generator.h>

#include <matter_ctrl_service.h>


using namespace esp_matter;
using namespace esp_matter::controller;
using namespace esp_matter::attribute;
using namespace chip;
using namespace chip::app::Clusters;

static const char *TAG = "read_node_info";

std::unordered_map <uint16_t,data_model*> get_ep_ptr; //virtual device i.e., endpoint
std::unordered_map <uint64_t,dev_data*> get_dev_ptr; //actual physical device

std::vector<uint64_t> node_id_list;
int node_id_list_index=0;

std::unordered_map <uint64_t,bool> online_devices;

char * esp_controller_get_datamodel_json(void);

static esp_err_t json_gen(json_gen_str_t *);
static void _read_node_wild_info(uint64_t);

void clear_data_model()
{
    for(const auto &dev_ptr  : get_dev_ptr)
    {
        if(dev_ptr.second->get_endpoint_ptr.size()>0)
        {
            for(const auto &node_ptr  : dev_ptr.second->get_endpoint_ptr)
            {
                if(node_ptr.second->get_cluster_ptr.size()>0)
                {
                    for(const auto &cluster_ptr  : node_ptr.second->get_cluster_ptr)
                    {
                        if(cluster_ptr.second->get_attribute_ptr.size()>0)
                        {
                            for(const auto& attribute_ptr : cluster_ptr.second->get_attribute_ptr)
                            {
                                delete (attribute_ptr.second);
                            }
                            cluster_ptr.second->get_attribute_ptr.clear();
                        }
                        delete (cluster_ptr.second);
                    }
                    node_ptr.second->get_cluster_ptr.clear();
                }

                delete (node_ptr.second);
            }
            dev_ptr.second->get_endpoint_ptr.clear();
        }
        delete dev_ptr.second;
    }
    get_dev_ptr.clear();
    get_ep_ptr.clear();
}

void print_data_model()
{
    // printf("\nPrinting Data Model:\n");
    // printf("{\n");
    // printf("\"Devices\": \n\t{ \n");
    // for(const auto &dev_ptr  : get_dev_ptr)
    // {
    //     std::cout<<std::hex<<"“node-id”: "<< dev_ptr.second->node_id <<std::endl;
    //     if(dev_ptr.second->get_endpoint_ptr.size()>0)
    //     {
    //         printf("\"endpoints\": \n\t{ \n");
    //         for(const auto &node_ptr  : dev_ptr.second->get_endpoint_ptr)
    //         {
    //             std::cout<<std::hex<<"“endpoint-id”: "<< node_ptr.second->endpoint_id <<std::endl;
    //             std::cout<<std::hex<<"“device-type-id”: "<< node_ptr.second->device_type <<std::endl;

    //             if(node_ptr.second->get_cluster_ptr.size()>0)
    //             {
    //                 printf("\"clusters\": \n\t{ \n");
    //                 for(const auto &cluster_ptr  : node_ptr.second->get_cluster_ptr)
    //                 {
    //                     std::cout<<std::hex<<"\t“cluster-id”: "<< cluster_ptr.second->cluster_id <<std::endl;

    //                     if(cluster_ptr.second->get_attribute_ptr.size()>0)
    //                     {
    //                         printf("\t\t\t{\n");
    //                         for(const auto& attribute_ptr : cluster_ptr.second->get_attribute_ptr)
    //                         {
    //                             std::cout<<"\t\t\t\""<<std::hex<<attribute_ptr.second->attribute_id<<"\" : "<<std::hex<<attribute_ptr.second->value<<std::endl;
    //                         }
    //                         printf("\t\t\t}\n");
    //                     }
    //                 }
    //                 printf("\t}\n");

    //             }

    //         }
    //         printf("\n}");
    //     }
    //     printf("\n}");
    // }
    // printf("\n}");

    report_data_model();

}

void report_data_model()
{
    char* json_data=esp_controller_get_datamodel_json();

    // printf("\njson_data: %s\n",json_data);

    esp_rmaker_controller_report_status_using_params(json_data);
}

esp_err_t change_data_model_attribute(uint64_t node_id, uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t* updated_val)
{
    if(get_dev_ptr.find(node_id)!=get_dev_ptr.end())
    {
        ESP_LOGI(TAG,"\nmatch for node-id attribute change\n");

        dev_data* dev = get_dev_ptr[node_id];

        if(dev->get_endpoint_ptr.find(endpoint_id)!=dev->get_endpoint_ptr.end())
        {
            ESP_LOGI(TAG,"\nmatch for endpoint-id attribute change\n");

            data_model* ep = dev->get_endpoint_ptr[endpoint_id];

            if(ep->get_cluster_ptr.find(cluster_id)!=ep->get_cluster_ptr.end())
            {
                ESP_LOGI(TAG,"\nmatch for cluster-id attribute change\n");

                ep_cluster* cls = ep->get_cluster_ptr[cluster_id];

                if(cls->get_attribute_ptr.find(attribute_id)!=cls->get_attribute_ptr.end())
                {
                    ESP_LOGI(TAG,"\nmatch for attribute-id attribute change\n");

                    cl_attribute* attr = cls->get_attribute_ptr[attribute_id];

                    attr->esp_value = *updated_val;

                    ESP_LOGI(TAG,"\nValue set to updated value.\n");
                }
            }


        }

    }
    report_data_model();

    return ESP_OK;
}
esp_err_t change_node_reachability(uint64_t node_id, bool updated_val)
{
    if(get_dev_ptr.find(node_id)!=get_dev_ptr.end())
    {
        ESP_LOGI(TAG,"Reachability changed for nodeid: %016llx",node_id);
        dev_data* dev = get_dev_ptr[node_id];
        dev->reachable = updated_val;
    }

    report_data_model();

    return ESP_OK;
}

int get_data_model_json(char *buf, size_t buf_size)
{
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, buf, buf_size, NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen(&jstr);
    if (json_gen_end_object(&jstr) < 0) {
        return -1;
    }
    return json_gen_str_end(&jstr);
}

char * esp_controller_get_datamodel_json(void)
{
    /* Setting buffer to NULL and size to 0 just to get the required buffer size */
    int req_size = get_data_model_json(NULL, 0);
    if (req_size < 0) {
        ESP_LOGE(TAG, "Failed to get required size for Node config JSON.");
        return NULL;
    }

    ESP_LOGI(TAG,"\nRequired buffer size: %d\n",req_size);
    char *datamodel_json = (char *)calloc(1, req_size);
    if (!datamodel_json) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for node config", req_size);
        return NULL;
    }
    if (get_data_model_json(datamodel_json, req_size) < 0) {
        free(datamodel_json);
        ESP_LOGE(TAG, "Failed to generate Node config JSON.");
        return NULL;
    }
    ESP_LOGI(TAG, "Generated Node config of length %d", req_size);
    return datamodel_json;
}

static esp_err_t json_gen(json_gen_str_t *jstr)
{
    ESP_LOGI(TAG,"\ngenerating data model JSON\n");

    for(const auto &dev_ptr  : get_dev_ptr)
    {
        char nodeid[17];
        std::snprintf(nodeid, sizeof(nodeid), "%016llx", static_cast<unsigned long long>(dev_ptr.second->node_id));
        json_gen_push_object(jstr,nodeid);

        json_gen_obj_set_bool(jstr, "enabled",dev_ptr.second->enabled);
        json_gen_obj_set_bool(jstr, "reachable",dev_ptr.second->reachable);

        json_gen_push_object(jstr,"endpoints");

        for(const auto &node_ptr  : dev_ptr.second->get_endpoint_ptr)
        {
            char epid[3];
            sprintf(epid, "%x", node_ptr.second->endpoint_id);
            std::string ep_str = "0x";
            ep_str += epid;
            json_gen_push_object(jstr,(char*)ep_str.c_str());
            // json_gen_push_object(jstr,epid);
            ep_str.clear();

            char dev_type[5];
            sprintf(dev_type, "%x", node_ptr.second->device_type);
            std::string dev_type_str = "0x";
            dev_type_str += dev_type;
            json_gen_obj_set_string(jstr, "device_type",(char*)dev_type_str.c_str());
            dev_type_str.clear();



            if(node_ptr.second->get_cluster_ptr.size()>0)
            {
                json_gen_push_object(jstr,"clusters");
                json_gen_push_object(jstr,"servers");
                for(const auto &cluster_ptr  : node_ptr.second->get_cluster_ptr)
                {

                    if(cluster_ptr.second->get_attribute_ptr.size()>0)
                    {
                        char cl_id[5];
                        sprintf(cl_id,"%x",cluster_ptr.second->cluster_id);
                        std::string val = "0x";
                        val+= cl_id;
                        json_gen_push_object(jstr,(char*)val.c_str());
                        val.clear();

                        json_gen_push_object(jstr,"attributes");
                        for(const auto& attribute_ptr : cluster_ptr.second->get_attribute_ptr)
                        {
                            char attr_id[9];
                            sprintf(attr_id, "%x", attribute_ptr.second->attribute_id);
                            std::string attr_id_str = "0x";
                            attr_id_str += attr_id;

                            if(attribute_ptr.second->esp_value.type == ESP_MATTER_VAL_TYPE_BOOLEAN)
                                json_gen_obj_set_bool(jstr,(char*)attr_id_str.c_str() ,attribute_ptr.second->esp_value.val.b);
                            else if(attribute_ptr.second->esp_value.type == ESP_MATTER_VAL_TYPE_UINT16)
                                json_gen_obj_set_int(jstr,(char*)attr_id_str.c_str() ,attribute_ptr.second->esp_value.val.u16);
                            else
                                // json_gen_obj_set_null(jstr, (char*)attr_id_str.c_str());
                                json_gen_obj_set_string(jstr,(char*)attr_id_str.c_str() ,(char*)attribute_ptr.second->esp_value.val.a.b);


                            attr_id_str.clear();
                            val.clear();
                        }
                        json_gen_pop_object(jstr); //close attributes object

                        //write logic for event list
                        if(cluster_ptr.second->events_list.size()>0)
                        {
                            json_gen_push_object(jstr,"events");

                            for(std::pair<uint32_t,bool> evt:cluster_ptr.second->events_list)
                            {
                                char evt_id[5];
                                sprintf(evt_id,"%x",evt.first);
                                std::string val = "0x";
                                val+= evt_id;
                                json_gen_obj_set_bool(jstr,(char*)val.c_str(),evt.second);
                                val.clear();

                            }

                            json_gen_pop_object(jstr);//close events object
                        }
                        json_gen_pop_object(jstr); //close particular cluster object

                    }

                }
                json_gen_pop_object(jstr); //close server object

                //put logic for client cluster
                if(node_ptr.second->client_list.size()>0)
                {
                    json_gen_push_array(jstr,"clients");

                    for(uint32_t client_id:node_ptr.second->client_list)
                    {
                        char clt_id[5];
                        sprintf(clt_id,"%x",client_id);
                        std::string val = "0x";
                        val+= clt_id;
                        json_gen_arr_set_string(jstr,(char*)val.c_str());
                        val.clear();

                    }

                    json_gen_pop_array(jstr);
                }

                json_gen_pop_object(jstr); //close cluster object
            }

            json_gen_pop_object(jstr); //close particular endpoint
        }

        json_gen_pop_object(jstr); //close endpoints object
        json_gen_pop_object(jstr); //close node object
    }

    ESP_LOGI(TAG,"\ndata model JSON generation done\n");
    return ESP_OK;
}

static void parse_cb_response(cb_data* _data)
{
    // printf("\nin parse_cb_response\n");

    if(_data->attr_path.mEndpointId==0x0 && _data->attr_path.mClusterId== Descriptor::Id && _data->attr_path.mAttributeId == Descriptor::Attributes::PartsList::Id)
    {
        // printf("\ncb_response Node_id: %016llx  Endpoint: %u Cluster: " ChipLogFormatMEI " Attribute " ChipLogFormatMEI "\n",
                // _data->node_id,_data->attr_path.mEndpointId, ChipLogValueMEI(_data->attr_path.mClusterId), ChipLogValueMEI(_data->attr_path.mAttributeId));

        chip::app::DataModel::DecodableList<chip::EndpointId> value;
        CHIP_ERROR err = chip::app::DataModel::Decode(*_data->tlv_data, value);
        if (err !=CHIP_NO_ERROR)
            ESP_LOGE(TAG,"\nparts Decode not OK err %d\n",err);

        auto iter = value.begin();

        while (iter.Next())
        {

            chip::EndpointId ep = iter.GetValue();
            data_model *node_info = new data_model(_data->node_id,ep);
            get_ep_ptr[ep]=node_info;
            get_dev_ptr[_data->node_id]->get_endpoint_ptr[ep]=node_info;


        }

    }
    else if(_data->attr_path.mEndpointId!=0x0 && _data->attr_path.mClusterId== Descriptor::Id && _data->attr_path.mAttributeId == Descriptor::Attributes::DeviceTypeList::Id)
    {
        chip::app::DataModel::DecodableList<chip::app::Clusters::Descriptor::Structs::DeviceTypeStruct::DecodableType> value;

        if(chip::app::DataModel::Decode(*_data->tlv_data, value)!=CHIP_NO_ERROR)
        {
            ESP_LOGE(TAG,"\ndevtype Decode OK\n");
        }

        auto iter = value.begin();

        while(iter.Next())
        {
            chip::app::Clusters::Descriptor::Structs::DeviceTypeStruct::DecodableType val = iter.GetValue();

            get_ep_ptr[_data->attr_path.mEndpointId]->device_type = val.deviceType;

        }

    }

    else if(_data->attr_path.mEndpointId!=0x0 && _data->attr_path.mClusterId== Descriptor::Id && _data->attr_path.mAttributeId == Descriptor::Attributes::ServerList::Id)
    {
        chip::app::DataModel::DecodableList<chip::ClusterId> value;
        if(chip::app::DataModel::Decode(*_data->tlv_data, value)==CHIP_NO_ERROR){};


        auto iter = value.begin();
        // size_t i  = 0;
        while (iter.Next())
        {

            chip::ClusterId val = iter.GetValue();
            ep_cluster* ncluster = new ep_cluster(val);
            get_ep_ptr[_data->attr_path.mEndpointId]->get_cluster_ptr[val] = ncluster;

        }


    }
    else if(_data->attr_path.mEndpointId!=0x0 && _data->attr_path.mClusterId== Descriptor::Id && _data->attr_path.mAttributeId == Descriptor::Attributes::ClientList::Id)
    {
        chip::app::DataModel::DecodableList<chip::ClusterId> value;
        if(chip::app::DataModel::Decode(*_data->tlv_data, value)==CHIP_NO_ERROR){};


        auto iter = value.begin();
        // size_t i  = 0;
        while (iter.Next())
        {

            chip::ClusterId val = iter.GetValue();
            // ep_cluster* ncluster = new ep_cluster(val);
            get_ep_ptr[_data->attr_path.mEndpointId]->client_list.push_back(val);

        }


    }
    else if(_data->attr_path.mEndpointId!=0x0 && _data->attr_path.mAttributeId == Globals::Attributes::EventList::Id)
    {
        chip::app::DataModel::DecodableList<chip::EventId> value;
        CHIP_ERROR err = chip::app::DataModel::Decode(*_data->tlv_data, value);
        if (err !=CHIP_NO_ERROR)
            ESP_LOGE(TAG,"\nevents Decode not OK err %d\n",err);

        auto iter = value.begin();

        while (iter.Next())
        {

            chip::EventId ev = iter.GetValue();
            get_ep_ptr[_data->attr_path.mEndpointId]->get_cluster_ptr[_data->attr_path.mClusterId]->events_list.push_back({ev,false});

        }
    }
    else if(_data->attr_path.mEndpointId!=0x0 && _data->attr_path.mClusterId!=0x1d  && _data->attr_path.mAttributeId != 0xFFF8 &&
            _data->attr_path.mAttributeId != 0xFFF9 && _data->attr_path.mAttributeId != 0xFFFA && _data->attr_path.mAttributeId != 0xFFFB && _data->attr_path.mAttributeId != 0xFFFC && _data->attr_path.mAttributeId != 0xFFFD)
    {
        data_model* node_ptr = get_ep_ptr[_data->attr_path.mEndpointId];

        int type = _data->tlv_data->GetType();

        esp_matter_attr_val_t esp_val;
        switch(type)
        {
            // case 0x0:
            // {
            //     int16_t attribute_value;
            //     if(chip::app::DataModel::Decode(*_data->tlv_data, attribute_value)==CHIP_NO_ERROR)
            //     {
            //         printf("\nattribute decode OK.\n");
            //         // val = std::to_string(attribute_value);
            //         esp_val = esp_matter_int16 (attribute_value);

            //     }
            //     else
            //         ESP_LOGE(TAG,"\nattribute decode not OK.\n");
            //     break;
            // }

            case 0x4:
            {
                u_int16_t attribute_value;
                if(chip::app::DataModel::Decode(*_data->tlv_data, attribute_value)==CHIP_NO_ERROR)
                {
                    esp_val = esp_matter_uint16 (attribute_value);
                }
                else
                    ESP_LOGE(TAG,"\nattribute decode not OK.\n");
                break;
            }
            case 0x8:
            {
                bool attribute_value;
                if(chip::app::DataModel::Decode(*_data->tlv_data, attribute_value)==CHIP_NO_ERROR)
                {
                    esp_val = esp_matter_bool(attribute_value);
                }
                else
                    ESP_LOGE(TAG,"\nattribute decode not OK.\n");
                break;
            }
            default:
                // esp_val = esp_matter_invalid(NULL);
                esp_val = esp_matter_char_str("Unhandled",sizeof("Unhandled"));
                break;
        }

        // std::cout<<std::hex<<"\""<<_data->attr_path.mAttributeId<<"\" : "<<val<<std::endl;
        if(node_ptr->get_cluster_ptr.find(_data->attr_path.mClusterId)!=node_ptr->get_cluster_ptr.end())
        {

            ep_cluster* curr_cluster = node_ptr->get_cluster_ptr[_data->attr_path.mClusterId];

            cl_attribute* nattribute = new cl_attribute(_data->attr_path.mAttributeId,esp_val);

            curr_cluster->get_attribute_ptr[_data->attr_path.mAttributeId] = nattribute;


        }
    }

    delete _data;

}

static void attribute_data_read_done(uint64_t remote_node_id, const ScopedMemoryBufferWithSize<AttributePathParams> &attr_path, const ScopedMemoryBufferWithSize<EventPathParams> &event_path)
{
     ESP_LOGI(TAG,"\nRead Info done for Nodeid: %016llx  Endpoint: %u Cluster: " ChipLogFormatMEI " Attribute " ChipLogFormatMEI "\n",
                remote_node_id,attr_path[0].mEndpointId, ChipLogValueMEI(attr_path[0].mClusterId), ChipLogValueMEI(attr_path[0].mAttributeId));

    node_id_list_index++;

    if(node_id_list_index<node_id_list.size())
        _read_node_wild_info(node_id_list[node_id_list_index]);
    else
        print_data_model();
}

static void attribute_data_cb(uint64_t remote_node_id, const chip::app::ConcreteDataAttributePath &path, chip::TLV::TLVReader *data)
{
    ChipLogProgress(chipTool, "Nodeid: %016llx  Endpoint: %u Cluster: " ChipLogFormatMEI " Attribute " ChipLogFormatMEI " DataVersion: %" PRIu32,
                    remote_node_id,path.mEndpointId, ChipLogValueMEI(path.mClusterId), ChipLogValueMEI(path.mAttributeId),
                    path.mDataVersion.ValueOr(0));


    if(path.mEndpointId==0x0 && path.mClusterId== Descriptor::Id && path.mAttributeId == Descriptor::Attributes::PartsList::Id)
    {
        cb_data* _data = new callback_data(remote_node_id,path,data);
        parse_cb_response(_data);

    }

    else if (path.mEndpointId!=0x0 )
    {
        callback_data* _data = new callback_data(remote_node_id,path,data);
        parse_cb_response(_data);

    }

}

static void _read_node_info(intptr_t arg)
{
    dev_data *ptr = (dev_data *)arg;
    if (!ptr) {
        ESP_LOGE(TAG, "Read device state with null ptr");
        return;
    }

    _read_node_wild_info(node_id_list[node_id_list_index]);

}

static void _read_node_wild_info(uint64_t nodeid)
{
    esp_matter::controller::read_command* cmd = chip::Platform::New<read_command>(nodeid, 0xFFFF , 0xFFFFFFFF, 0xFFFFFFFF,
                                                          esp_matter::controller::READ_ATTRIBUTE, attribute_data_cb,attribute_data_read_done, nullptr);

    if (!cmd) {
        ESP_LOGE(TAG, "Failed to alloc memory for read_command");
        return;
    }

    cmd->send_command();
    ESP_LOGI(TAG,"\nfetching info of node_id: %016llx\n",node_id_list[node_id_list_index]);
}

void update_and_report_reachability(std::vector<uint64_t> nodeid_list)
{
    for(auto &node_id  : online_devices)
        node_id.second=false;
    for(auto &node_id : nodeid_list)
        online_devices[node_id]=true;
    for(auto &node_id  : online_devices)
    {
        if (node_id.second==false)
        {
            change_node_reachability(node_id.first,false);
        }
    }
}

void read_node_info(std::vector<uint64_t> nodeid_list)
{
    update_and_report_reachability(nodeid_list);
    clear_data_model();
    node_id_list.clear();
    node_id_list_index=0;

    for(uint64_t nodeid:nodeid_list)
    {
        node_id_list.push_back(nodeid);
        dev_data *dev_info = new dev_data(nodeid);
        get_dev_ptr[nodeid]=dev_info;
    }
    nodeid_list.clear();
    dev_data* dev_info = get_dev_ptr[node_id_list[node_id_list_index]];
    chip::DeviceLayer::PlatformMgr().ScheduleWork(_read_node_info,(intptr_t)dev_info);



}
