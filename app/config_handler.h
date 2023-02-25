/**
 *
 * Microvisor Config Handler
 * Version 1.0.0
 * Copyright © 2022, Twilio
 * Licence: Apache 2.0
 *
 */


#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "azure_helper.h"


/*
 * MACROS
 */
#define STRING_ITEM(name, str) \
    . name = { .data = (uint8_t*)str, . length = strlen(str) }


/*
 * DEFINES
 */
#define TAG_CHANNEL_CONFIG 100

#define BUF_CLIENT_SIZE 34

#define BUF_READ_BUFFER 3*1024


#ifdef __cplusplus
extern "C" {
#endif


/*
 * PROTOTYPES
 */
void start_configuration_fetch();
void receive_configuration_items();
void finish_configuration_fetch();


/*
 * GLOBALS
 */
extern uint8_t  client[BUF_CLIENT_SIZE];
extern size_t   client_len;

extern struct AzureConnectionStringParams azure_params;


#ifdef __cplusplus
}
#endif


#endif /* CONFIG_HANDLER_H */
