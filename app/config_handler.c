/**
 *
 * Microvisor Config Handler 
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */

#include "config_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Microvisor includes
#include "stm32u5xx_hal.h"
#include "mv_syscalls.h"

#include "work.h"
#include "network_helper.h"
#include "log_helper.h"

uint8_t  client[BUF_CLIENT_SIZE];
size_t   client_len;

struct AzureConnectionStringParams azure_params = { 0 };

static MvChannelHandle configuration_channel = 0;

/**
 * CONFIGURATION OPERATIONS
 */

/*
 * @brief Open channel for configuration tasks
 */
void start_configuration_fetch() {
    mvGetDeviceId(client, BUF_CLIENT_SIZE);
    client_len = BUF_CLIENT_SIZE;

    MvNetworkHandle network_handle = get_network_handle();
    struct MvOpenChannelParams ch_params = {
        .version = 1,
        .v1 = {
            .notification_handle = work_notification_center_handle,
            .notification_tag = TAG_CHANNEL_CONFIG,
            .network_handle = network_handle,
            .receive_buffer = (uint8_t*)work_receive_buffer,
            .receive_buffer_len = sizeof(work_receive_buffer),
            .send_buffer = (uint8_t*)work_send_buffer,
            .send_buffer_len = sizeof(work_send_buffer),
            .channel_type = MV_CHANNELTYPE_CONFIGFETCH,
            STRING_ITEM(endpoint, "")
        }
    };

    enum MvStatus status;
    if ((status = mvOpenChannel(&ch_params, &configuration_channel)) != MV_STATUS_OKAY) {
        server_error("encountered error opening config channel: %x", status);
        pushWorkMessage(OnConfigFailed);
        return;
    }

    struct MvConfigKeyToFetch items[] = {
        {
            .scope = MV_CONFIGKEYFETCHSCOPE_DEVICE,
            .store = MV_CONFIGKEYFETCHSTORE_SECRET,
            STRING_ITEM(key, "azure-connection-string"),
        },
    };

    struct MvConfigKeyFetchParams request = {
        .num_items = sizeof(items)/sizeof(struct MvConfigKeyToFetch),
        .keys_to_fetch = items,
    };

    if ((status = mvSendConfigFetchRequest(configuration_channel, &request)) != MV_STATUS_OKAY) {
        server_error("encountered an error requesting config: %x", status);
        pushWorkMessage(OnConfigFailed);
    }
}

void receive_configuration_items() {
    server_log("receiving configuration results");

    uint8_t read_buffer[BUF_READ_BUFFER] __attribute__ ((aligned(512))); // needs to support largest config item, likely string encoded hex for private key (~2400 bytes)
    uint32_t read_buffer_used = 0;

    struct MvConfigResponseData response;

    enum MvStatus status;
    if ((status = mvReadConfigFetchResponseData(configuration_channel, &response)) != MV_STATUS_OKAY) {
        server_error("encountered an error fetching configuration response");
        pushWorkMessage(OnConfigFailed);
        return;
    }

    if (response.result != MV_CONFIGFETCHRESULT_OK) {
        server_error("received different result from config fetch than expected: %d", response.result);
        pushWorkMessage(OnConfigFailed);
        return;
    }

    if (response.num_items != 1) {
        server_error("received different number of items than expected 1 != %d", response.num_items);
        pushWorkMessage(OnConfigFailed);
        return;
    }

    enum MvConfigKeyFetchResult result;
    struct MvConfigResponseReadItemParams item = {
        .item_index = 0,
        .result = &result,
        .buf = {
            .data = read_buffer,
            .size = sizeof(read_buffer) - 1,
            .length = &read_buffer_used
        }
    };


    uint8_t  azure_connection_string[256] = {0};
    size_t   azure_connection_string_len = 0;

    item.item_index = 0;
    server_log("fetching item %d", item.item_index);
    if ((status = mvReadConfigResponseItem(configuration_channel, &item)) != MV_STATUS_OKAY) {
        server_error("error reading config item index %d - %d (MvConfigFetchResult)", item.item_index, status);
        pushWorkMessage(OnConfigFailed);
        return;
    }
    if (result != MV_CONFIGKEYFETCHRESULT_OK) {
        server_error("unexpected result reading config item index %d - %d (MvConfigKeyFetchResult)", item.item_index, result);
        pushWorkMessage(OnConfigFailed);
        return;
    }
    memcpy(azure_connection_string, read_buffer, read_buffer_used);
    azure_connection_string_len = read_buffer_used;
    azure_connection_string[azure_connection_string_len] = '\0'; // treat as char* for tokenizing

    if (!parse_azure_connection_string(
            azure_connection_string, azure_connection_string_len,
            &azure_params)) {
        server_error("failed to parse azure connection string");
        pushWorkMessage(OnConfigFailed);
        return;
    }


    pushWorkMessage(OnConfigObtained);
}

void finish_configuration_fetch() {
    server_log("closing configuration channel");
    mvCloseChannel(&configuration_channel);
}
