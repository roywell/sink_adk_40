/*******************************************************************************
Copyright Cambridge Silicon Radio Limited 2014 - 2015.
Part of ADK 4.0

FILE NAME
    sink_gatt_device.h

DESCRIPTION
    General GATT specific device functionality.
    
NOTES

*/

#ifndef _SINK_GATT_DEVICE_H_
#define _SINK_GATT_DEVICE_H_


#include "sink_gatt_client.h"
#include "sink_gatt_server.h"

#include <csrtypes.h>


#ifdef GATT_ENABLED

#define GATT theSink.rundata->ble.gatt


typedef struct __gatt_data_t
{
    gatt_client_t   client;             /* Client data */
    gatt_server_t   server;             /* Server data */
} gatt_data_t;

#endif /* GATT_ENABLED */


#endif /* _SINK_GATT_DEVICE_H_ */

