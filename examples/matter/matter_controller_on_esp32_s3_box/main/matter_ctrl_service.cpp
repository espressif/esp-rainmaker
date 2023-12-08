#include <matter_ctrl_service.h>
#include <controller_ctrl.h>
#include <iostream>

#include <json_generator.h>
#include <json_parser.h>

static const char *TAG = "esp_matter_controller_service";

#define ESP_MATTER_CONTROLLER_SERV_NAME    "matter-controller"
#define ESP_MATTER_CONTROLLER_SERV_TYPE    "esp.service.matter-controller"
#define ESP_MATTER_CONTROLLER_DATA_PARAM_NAME   "matter-controller-data"
#define ESP_MATTER_CONTROLLER_DATA_PARAM_TYPE   "esp.param.matter-controller-data"
#define ESP_MATTER_CONTROLLER_DATA_VERSION_PARAM_NAME   "matter-controller-data-version"
#define ESP_MATTER_CONTROLLER_DATA_VERSION_PARAM_TYPE   "esp.param.matter-controller-data-version"



static esp_err_t controller_parse_json_get_node_id(jparse_ctx_t* jctx, void *data, uint64_t* nodeid)

{
    int num_nodes=0;

    if(json_obj_get_array(jctx,"matter-nodes",&num_nodes)== OS_SUCCESS)
        ESP_LOGI(TAG,"\nnodes: %d\n",num_nodes);
    else
        ESP_LOGE(TAG,"\nmatter-node error.\n");
    for(int node_index=0;node_index<num_nodes;node_index++)
    {
        if(json_arr_get_object(jctx,node_index)==0)
        {
            int val_size = 0;
            if (json_obj_get_strlen(jctx, "matter-node-id", &val_size) == 0) 
            {
                val_size++; /* For NULL termination */
                char* node_id = (char*)calloc(1, val_size);
                if (!node_id) {
                    return ESP_ERR_NO_MEM;
                }
                if(json_obj_get_string(jctx, "matter-node-id", node_id, val_size)==0)
                {
                    ESP_LOGI(TAG,"\nnode-id: %s\n",node_id);

                    *nodeid = std::stoull(node_id, nullptr, 16);
    
                }
                else
                    ESP_LOGE(TAG,"\nError in matter-node-id retrieval.\n");

                

            }
            else
                ESP_LOGE(TAG,"\nError in matter-node-id size retrieval.\n");
        }
        else
            ESP_LOGE(TAG,"\nError in matter-nodes array.\n");


        
    }
    return ESP_OK;
}

static esp_err_t controller_parse_json_get_endpoint_id(jparse_ctx_t* jctx, void *data, uint16_t* endpoint_id)
{
    int num_endpoints=0;

        if(json_obj_get_array(jctx,"endpoints",&num_endpoints)== OS_SUCCESS)
            ESP_LOGI(TAG,"\nendpoints: %d\n",num_endpoints);
        else
            ESP_LOGE(TAG,"\nendpoint error.\n");
        
        for(int ep_index=0;ep_index<num_endpoints;ep_index++)
        {
            if(json_arr_get_object(jctx,ep_index)==0)
            {
                int val_size = 0;
                if (json_obj_get_strlen(jctx, "endpoint-id", &val_size) == 0) 
                {
                    val_size++; /* For NULL termination */
                    char* ep_id = (char*)calloc(1, val_size);
                    if (!ep_id) {
                        return ESP_ERR_NO_MEM;
                    }
                    if(json_obj_get_string(jctx, "endpoint-id", ep_id, val_size)==0)
                    {
                        ESP_LOGI(TAG,"\nendpoint-id: %s\n",ep_id);

                        *endpoint_id = static_cast<uint16_t>(std::stoull(ep_id, nullptr, 16));
                    }
                    else
                        ESP_LOGE(TAG,"\nError in endpoint-id retrieval.\n");

                    

                }
                else
                    ESP_LOGE(TAG,"\nError in endpoint-id size retrieval.\n");
            }
            else
                ESP_LOGE(TAG,"\nError in endpoints array.\n");
        }

        return ESP_OK;

}

static esp_err_t controller_parse_json_get_cluster_id(jparse_ctx_t* jctx, void *data, char* &cl_id)
{
    int num_clusters=0;

        if(json_obj_get_array(jctx,"clusters",&num_clusters)== OS_SUCCESS)
            ESP_LOGI(TAG,"\nclusters: %d\n",num_clusters);
        else
            ESP_LOGE(TAG,"\nclusters error.\n");
        
        for(int cls_index=0;cls_index<num_clusters;cls_index++)
        {
            if(json_arr_get_object(jctx,cls_index)==0)
            {
                int val_size = 0;
                if (json_obj_get_strlen(jctx, "cluster-id", &val_size) == 0) 
                {
                    val_size++; /* For NULL termination */
                    char* cls_id = (char*)calloc(1, val_size);
                    if (!cls_id) {
                        return ESP_ERR_NO_MEM;
                    }
                    if(json_obj_get_string(jctx, "cluster-id", cls_id, val_size)==0)
                    {
                        ESP_LOGI(TAG,"\ncluster-id: %s\n",cls_id);
                        cl_id = new char[strlen(cls_id) + 1];
                        strcpy(cl_id,cls_id);
                        free(cls_id);
                    }
                    else
                        ESP_LOGE(TAG,"\nError in cluster-id retrieval.\n");

                    

                }
                else
                    ESP_LOGE(TAG,"\nError in cluster-id size retrieval.\n");
            }
            else
                ESP_LOGE(TAG,"\nError in cluster array.\n");
        }

        return ESP_OK;

}

static esp_err_t controller_parse_json_get_command_id(jparse_ctx_t* jctx, void *data, char* &cmdid)
{
    int num_commands=0;

        if(json_obj_get_array(jctx,"commands",&num_commands)== OS_SUCCESS)
            ESP_LOGI(TAG,"\ncommands: %d\n",num_commands);
        else
            ESP_LOGE(TAG,"\ncommands error.\n");
        
        for(int cmd_index=0;cmd_index<num_commands;cmd_index++)
        {
            if(json_arr_get_object(jctx,cmd_index)==0)
            {
                int val_size = 0;
                if (json_obj_get_strlen(jctx, "command-id", &val_size) == 0) 
                {
                    val_size++; /* For NULL termination */
                    char* cmd_id = (char*)calloc(1, val_size);
                    if (!cmd_id) {
                        return ESP_ERR_NO_MEM;
                    }
                    if(json_obj_get_string(jctx, "command-id", cmd_id, val_size)==0)
                    {
                        ESP_LOGI(TAG,"\ncommand-id: %s\n",cmd_id);
                        
                        cmdid = new char[strlen(cmd_id) + 1];
                        strcpy(cmdid,cmd_id);
                        free(cmd_id);
                    }
                    else
                        ESP_LOGE(TAG,"\nError in command-id retrieval.\n");

                    

                }
                else
                    ESP_LOGE(TAG,"\nError in command-id size retrieval.\n");
            }
            else
                ESP_LOGE(TAG,"\nError in command array.\n");
        }

    return ESP_OK;

}

static esp_err_t controller_parse_json_get_data(jparse_ctx_t* jctx, void *data, std::vector<std::string>& cmd_data)
{
    //data 

        if(json_obj_get_object(jctx,"data")== OS_SUCCESS)
            ESP_LOGI(TAG,"\ndata present\n");
        else
            ESP_LOGE(TAG,"\ndata absent\n");

        int data_id = 0;
        while(true)
        {
            
            int val_size = 0;
            
            if (json_obj_get_strlen(jctx, std::to_string(data_id).c_str(), &val_size) == 0) 
            {
                val_size++; /* For NULL termination */
                char* data_val = (char*)calloc(1, val_size);
                if (!data_val) {
                    return ESP_ERR_NO_MEM;
                }
                if(json_obj_get_string(jctx, std::to_string(data_id).c_str(), data_val, val_size)==0)
                {
                    ESP_LOGI(TAG,"\ndata val: %s\n",data_val);
                    std::string val(data_val);
                    cmd_data.push_back(val);

                    free(data_val);
                }
                else
                    ESP_LOGE(TAG,"\nError in data val retrieval.\n");

                

            }
            else
                {
                    ESP_LOGE(TAG,"\nError in data val retrieval or end of data object\n");
                    break;
                }

            data_id++;
        }

        return ESP_OK;
}

static esp_err_t controller_parse_json(void *data, size_t data_len, esp_rmaker_req_src_t src)
{


    /* Get details from JSON */
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)data, data_len) != 0) {
        ESP_LOGE(TAG, "Json parse start failed");
        return ESP_FAIL;
    }

    uint64_t node_id;
    controller_parse_json_get_node_id(&jctx,data,&node_id);
    uint16_t endpoint_id;
    controller_parse_json_get_endpoint_id(&jctx,data,&endpoint_id);

    


    std::vector<std::string> cmd_data;

    char* cl_id;
    controller_parse_json_get_cluster_id(&jctx,data,cl_id);

    char* cmd_id;
    controller_parse_json_get_command_id(&jctx,data,cmd_id);

    cmd_data.push_back(cl_id);
    cmd_data.push_back(cmd_id);

    controller_parse_json_get_data(&jctx,data,cmd_data);
    
    send_cmd_format* cmd = new send_command_format(node_id,endpoint_id,cmd_data);

    cmd_data.clear();

    send_command((intptr_t)cmd);

    json_parse_end(&jctx);
    return ESP_OK;
}


static esp_err_t controller_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
         const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    /* This ctx check is just to find if the request was received via Cloud, Local network or Schedule.
     * Having this is not required, but there could be some cases wherein specific operations may be allowed
     * only via specific channels (like only Local network), where this would be useful.
     */
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
  
    /* Check if the write is on the "Trigger" parameter. We aren't really checking true/false as that
     * is not much of a concern in this context. But you can add checks on the values too.
     */
    // printf("\n%s\n",esp_rmaker_param_get_name(param));
    if (strcmp(esp_rmaker_param_get_name(param), ESP_MATTER_CONTROLLER_DATA_PARAM_NAME) == 0) 
    {
        /* Here we start some dummy diagnostics and populate the appropriate values to be passed
         * to "Timestamp" and "Data".
         */
        
        
        /* The values are reported by updating appropriate parameters */
        
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.s, esp_rmaker_device_get_name(device), esp_rmaker_param_get_name(param));
        // esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(device, "controller-data"),
        //             esp_rmaker_obj(buf));
        controller_parse_json(val.val.s, strlen(val.val.s), ctx->src);
    }

    return ESP_OK;
}

/* Register the write callback. Read callback would normally be NULL */

esp_rmaker_device_t *esp_rmaker_controller_service_create(const char *serv_name, void *priv_data)
{
    esp_rmaker_device_t *matter_ctrl_service = esp_rmaker_service_create(serv_name, ESP_MATTER_CONTROLLER_SERV_TYPE,priv_data);
    if (matter_ctrl_service) 
    {
        esp_rmaker_device_add_cb(matter_ctrl_service,controller_write_cb, NULL);


        esp_rmaker_device_add_param(matter_ctrl_service, esp_rmaker_param_create(ESP_MATTER_CONTROLLER_DATA_PARAM_NAME, ESP_MATTER_CONTROLLER_DATA_PARAM_TYPE, esp_rmaker_obj("{}"), PROP_FLAG_READ));

        esp_rmaker_device_add_param(matter_ctrl_service, esp_rmaker_param_create(ESP_MATTER_CONTROLLER_DATA_VERSION_PARAM_NAME, ESP_MATTER_CONTROLLER_DATA_VERSION_PARAM_TYPE, esp_rmaker_str("1.0.0"), PROP_FLAG_READ));

        esp_err_t err = esp_rmaker_node_add_device(esp_rmaker_get_node(), matter_ctrl_service);
        if (err == ESP_OK) 
        {
            ESP_LOGI(TAG, "Matter Controller service enabled.");
        } else 
        {
            esp_rmaker_device_delete(matter_ctrl_service);
        }
    }
    return matter_ctrl_service;
}


esp_err_t esp_rmaker_controller_service_enable()
{
    
    esp_rmaker_device_t *service = esp_rmaker_controller_service_create(ESP_MATTER_CONTROLLER_SERV_NAME, NULL);
    if (service)
        return ESP_OK;
    else
        return ESP_ERR_NO_MEM;

}

esp_err_t esp_rmaker_controller_report_status_using_params(char *additional_info)
{
    const esp_rmaker_device_t *device = esp_rmaker_node_get_device_by_name(esp_rmaker_get_node(), ESP_MATTER_CONTROLLER_SERV_NAME);
    if (!device) {
        return ESP_FAIL;
    }
    esp_rmaker_param_t *controller_param = esp_rmaker_device_get_param_by_type(device, ESP_MATTER_CONTROLLER_DATA_PARAM_TYPE);
    
    esp_rmaker_param_update_and_report(controller_param, esp_rmaker_obj(additional_info));

    // esp_err_t err = esp_rmaker_param_update(info_param, esp_rmaker_obj(additional_info));
    
    return ESP_OK;
}