#pragma once
#include <esp_log.h>
#include <string>
#include <vector>
#include <string.h>
#include <esp_system.h>
#include <esp_rmaker_work_queue.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
typedef struct send_command_format
{
    uint64_t node_id;
    uint16_t endpoint_id;
    std::vector<std::string> cmd_data;

    send_command_format(uint64_t remote_nodeid, uint16_t endpointid, std::vector<std::string> cmd)
    {
        this->node_id = remote_nodeid;
        this->endpoint_id = endpointid;
        for(std::string str: cmd)
        {
            this->cmd_data.push_back(str);
        }
    }

}send_cmd_format;


void send_command(intptr_t arg);