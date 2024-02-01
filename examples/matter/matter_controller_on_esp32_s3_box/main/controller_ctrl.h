#pragma once
#include <esp_log.h>
#include <string>
#include <vector>
#include <string.h>
#include <esp_system.h>
#include <esp_matter.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
typedef struct send_command_format
{
    uint64_t node_id;
    uint16_t endpoint_id;
    uint32_t cluster_id;
    uint32_t command_id;
    char* cmd_data;

    send_command_format(uint64_t remote_nodeid, uint16_t endpointid,uint32_t clusterid, uint32_t commandid, char* cmd)
    {
        this->node_id = remote_nodeid;
        this->endpoint_id = endpointid;
        this->cluster_id = clusterid;
        this->command_id = commandid;
        this->cmd_data = NULL;
        if(cmd!= NULL)
        {
            this->cmd_data = new char[strlen(cmd) + 1];
            strcpy(this->cmd_data, cmd);
        }

    }

    ~send_command_format()
    {
        delete[] this->cmd_data;
    }

}send_cmd_format;


CHIP_ERROR send_command(intptr_t arg);