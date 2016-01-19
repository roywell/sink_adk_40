/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2015

FILE NAME
    sink_gatt_client_dis.c

DESCRIPTION
    Routines to handle the GATT Device Information Service
*/

#include <stdlib.h>
#include <connection.h>
#include <gatt.h>

#include "sink_gatt_client_dis.h"

#include "sink_debug.h"
#include "sink_gatt_client.h"
#include "sink_private.h"
#include "sink_gap_ad_types.h"

#ifdef GATT_DIS_CLIENT

static const uint8 dis_ble_advertising_filter[] = {GATT_SERVICE_UUID_DEVICE_INFORMATION & 0xFF, GATT_SERVICE_UUID_DEVICE_INFORMATION >> 8};

#ifdef DEBUG_GATT_DIS_CLIENT
#define GATT_DIS_CLIENT_DEBUG(x) DEBUG(x)
#else
#define GATT_DIS_CLIENT_DEBUG(x) 
#endif

/****************************STATIC FUNCTIONS************************************/


/*******************************************************************************
NAME
    gattDisClientFindConnection
    
DESCRIPTION
    Finds a client connection associated with a Device Information Service instance.
    
PARAMETERS
    gdisc    The Device Information Service client
    
RETURNS    
    The client connection pointer associated with the Device Information Service instance. NULL if not found.   
    
*/
static gatt_client_connection_t *gattDisClientFindConnection(const GDISC *gdisc)
{
    uint16 index = 0;
    gatt_client_services_t *data = NULL;
    
    if (gdisc == NULL)
    {
        return NULL;
    }
    
    for (index = 0; index < GATT_CLIENT.number_connections; index++)
    {
        data = gattClientGetServiceData(&GATT_CLIENT.connection[index]);
        if (data && (data->disc == gdisc))
        {
            return &GATT_CLIENT.connection[index];
        }
    }
    
    return NULL;
}


/*******************************************************************************
NAME
    gattDisClientServiceInitialised
    
DESCRIPTION
    Called when the Device Information Service is initialised.
    
PARAMETERS
    gdisc    The Device Information Service client instance pointer
    
*/
static void gattDisClientServiceInitialised(const GDISC *gdisc)
{
    gatt_client_connection_t *conn = gattDisClientFindConnection(gdisc);
                                     
    if (conn != NULL)
    {
        gattClientDiscoveredServiceInitialised(conn);
    }        
}


/*******************************************************************************
NAME
    gattDisClientInitCfm
    
DESCRIPTION
    Handle the GATT_DEVICE_INFO_CLIENT_INIT_CFM message
    
PARAMETERS
    cfm    The GATT_DEVICE_INFO_CLIENT_INIT_CFM message
*/
static void gattDisClientInitCfm(const GATT_DEVICE_INFO_CLIENT_INIT_CFM_T *cfm)
{
    GATT_DIS_CLIENT_DEBUG(("GATT_DEVICE_INFO_CLIENT_INIT_CFM status[%u]\n", cfm->status));

    /* The service initialisation is complete */
    gattDisClientServiceInitialised(cfm->device_info_client);

    if ((cfm->status == gatt_device_info_client_status_success) &&
        (gattDisClientFindConnection(cfm->device_info_client) != NULL))
    {       
        if ( (cfm->supported_char_mask & PNP_ID_CHAR) == PNP_ID_CHAR)
        {
            GattDeviceInfoClientReadCharRequest((GDISC*)cfm->device_info_client, gatt_device_info_client_pnp_id);
        }
    }
}


/*******************************************************************************
NAME
    gattDisClientReadCharCfm
    
DESCRIPTION
    Handle the GATT_DEVICE_INFO_CLIENT_READ_CHAR_CFM message
    
PARAMETERS
    cfm    The GATT_DEVICE_INFO_CLIENT_READ_CHAR_CFM message
    
*/
static void gattDisClientReadCharCfm(const GATT_DEVICE_INFO_CLIENT_READ_CHAR_CFM_T *cfm)
{
    GATT_DIS_CLIENT_DEBUG(("GATT_DEVICE_INFO_CLIENT_READ_CHAR_CFM status[%u] \n", 
                   cfm->status)); 

#ifdef DEBUG_GATT_DIS_CLIENT
    if ((cfm->status == gatt_device_info_client_status_success) && (cfm->device_info_type == gatt_device_info_client_pnp_id))
    {
        uint16 index = 0;
        GATT_DIS_CLIENT_DEBUG(("PNP Size [0x%x] \n", cfm->size));

        for (index=0;index < cfm->size;index++)
        {
            GATT_DIS_CLIENT_DEBUG(("PNP Value [0x%x] \n", cfm->value[index]));
        }
    }
#endif
}


/****************************GLOBAL FUNCTIONS***********************************/

/****************************************************************************/
void sinkGattDisClientSetupAdvertisingFilter(void)
{
    GATT_DIS_CLIENT_DEBUG(("GattDis: Add DIS scan filter\n"));
    ConnectionBleAddAdvertisingReportFilter(AD_TYPE_SERVICE_UUID_16BIT_LIST, sizeof(dis_ble_advertising_filter), sizeof(dis_ble_advertising_filter), dis_ble_advertising_filter);
}

/****************************************************************************/
bool sinkGattDisClientAddService(uint16 cid, uint16 start, uint16 end)
{
    gatt_client_services_t *client_services = NULL;
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    uint16 *service = gattClientAddService(connection, sizeof(GDISC));

    GATT_DIS_CLIENT_DEBUG(("Add Device Info Client Service\n"));
    if (service)
    {
        GATT_DEVICE_INFO_CLIENT_INIT_PARAMS_T params;
        GDISC *gdisc = NULL;

        client_services = gattClientGetServiceData(connection);
        client_services->disc = (GDISC *)service;
        gdisc = client_services->disc;

        params.cid = cid;
        params.start_handle = start;
        params.end_handle = end;
        
        if (GattDeviceInfoClientInit(sinkGetBleTask(), gdisc, &params) == gatt_device_info_client_status_initiated)
        {            
            return TRUE;
        }
    }
    
    return FALSE;
}

/****************************************************************************/
void sinkGattDISClientRemoveService(GDISC *gdisc, uint16 cid)
{
    /* Deinit GATT DIS client */
    GattDeviceInfoClientDestroy(gdisc);
}

/******************************************************************************/
void sinkGattDisClientMsgHandler(Task task, MessageId id, Message message)
{
    switch(id)
    {
        case GATT_DEVICE_INFO_CLIENT_INIT_CFM:
        {
            gattDisClientInitCfm((GATT_DEVICE_INFO_CLIENT_INIT_CFM_T*)message);
        }
        break;
        case GATT_DEVICE_INFO_CLIENT_READ_CHAR_CFM:
        {
            gattDisClientReadCharCfm((GATT_DEVICE_INFO_CLIENT_READ_CHAR_CFM_T*)message);
        }
        break;
        default:
        {
            GATT_DIS_CLIENT_DEBUG(("Unhandled Device Info Client msg [%x]\n", id));
        }
        break;
    }
}



#endif /* GATT_DIS_CLIENT */

