/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2015

FILE NAME
    sink_gatt_client_ancs.c

DESCRIPTION
    Routines to handle the GATT ANCS Client.
*/

#include <stdlib.h>
#include <connection.h>

#include "sink_gatt_client_ancs.h"

#include "sink_debug.h"
#include "sink_gatt_client.h"
#include "sink_private.h"
#include "sink_gap_ad_types.h"


#ifdef GATT_ANCS_CLIENT

static const uint8 ancs_ble_advertising_filter[] = {0x05, 0x79, 0x31, 0xf4, 0xce, 0xb5, 0x99, 0x4e, 0x0f, 0xa4, 0x1e, 0x4b, 0x2d, 0x12, 0xd0, 0x00};

#ifdef DEBUG_GATT_ANCS_CLIENT
#define GATT_ANCS_CLIENT_DEBUG(x) DEBUG(x)
#else
#define GATT_ANCS_CLIENT_DEBUG(x) 
#endif


/*******************************************************************************
NAME
    gattAncsFindConnection
    
DESCRIPTION
    Finds a client connection associated with an Ancs instance.
    
PARAMETERS
    gancs    The Ancs client instance pointer
    
RETURNS    
    The client connection pointer associated with the Ancs instance. NULL if not found.   
    
*/
static gatt_client_connection_t *gattAncsFindConnection(const GANCS *gancs)
{
    uint16 index = 0;
    gatt_client_services_t *data = NULL;
    
    if (gancs == NULL)
    {
        return NULL;
    }
    
    for (index = 0; index < GATT_CLIENT.number_connections; index++)
    {
        data = gattClientGetServiceData(&GATT_CLIENT.connection[index]);
        if (data && (data->ancs == gancs))
        {
            return &GATT_CLIENT.connection[index];
        }
    }
    
    return NULL;
}


/*******************************************************************************
NAME
    gattAncsServiceInitialised
    
DESCRIPTION
    Called when the Ancs service is initialised.
    
PARAMETERS
    gancs    The Ancs client instance pointer
    
*/
static void gattAncsServiceInitialised(const GANCS *gancs, gatt_ancs_status_t status)
{
    gatt_client_connection_t *conn = gattAncsFindConnection(gancs);
 
    if (conn != NULL)
    {
        /* ANCS library was not able to successfully initialize 
         * Remove the ANCS client from client_data structure */
        if(status == gatt_ancs_status_failed)
        {
            gattClientRemoveDiscoveredService(conn, gatt_client_ancs);
        }
        /* Done with this ANCS client */
        gattClientDiscoveredServiceInitialised(conn);
    }
}

/*******************************************************************************
NAME
    gattAncsSetNSNotificationCfm
    
DESCRIPTION
    Handle the GATT_ANCS_SET_NS_NOTIFICATION_CFM message.
    
PARAMETERS
    cfm    The GATT_ANCS_SET_NS_NOTIFICATION_CFM message
    
*/
static void gattAncsSetNSNotificationCfm(const GATT_ANCS_SET_NS_NOTIFICATION_CFM_T *cfm)
{
    GATT_ANCS_CLIENT_DEBUG(("Ancs Set NS Notification Cfm status[%u]\n", cfm->status));
}

/*******************************************************************************
NAME
    gattAncsInitCfm
    
DESCRIPTION
    Handle the GATT_ANCS_INIT_CFM message.
    
PARAMETERS
    cfm    The GATT_ANCS_INIT_CFM message
    
*/
static void gattAncsInitCfm(const GATT_ANCS_INIT_CFM_T *cfm)
{
    GATT_ANCS_CLIENT_DEBUG(("Ancs Init Cfm status[%u]\n", cfm->status));

    /* Update the service as initialized */
    gattAncsServiceInitialised(cfm->ancs, cfm->status);

    if((cfm->status == gatt_ancs_status_success) && (gattAncsFindConnection(cfm->ancs) != NULL))
    {
        GattAncsSetNSNotificationEnableRequest(cfm->ancs, TRUE, ANCS_EMAIL_CATEGORY);
    }
}


/*******************************************************************************
NAME
    gattAncsNSNotificationInd
    
DESCRIPTION
    Handle the GATT_ANCS_NS_IND message.
    
PARAMETERS
    ind    The GATT_ANCS_NS_IND message
    
*/
static void gattAncsNSNotificationInd(const GATT_ANCS_NS_IND_T *ind)
{
     /*  Notification Source Data format
     * -------------------------------------------------------
     * |  Event  |  Event  |  Cat  |  Cat   |  Notification  |
     * |  ID     |  Flag   |  ID   |  Count |  UUID          |
     * |---------------------------------------------------- |
     * |   1B    |   1B    |   1B  |   1B   |   4B           |
     * -------------------------------------------------------
     */
    GATT_ANCS_CLIENT_DEBUG(("Recieved ANCS NS Notification for category[%d]\n", ind->category_id));
    
    /* Since the app has request notification for only email, no need to verify the category id */
    MessageCancelAll(&theSink.task, EventSysAncsEmailAlert);
    MessageSend(&theSink.task, EventSysAncsEmailAlert , 0);
}


/****************************************************************************/
void sinkGattAncsClientSetupAdvertisingFilter(void)
{
    GATT_ANCS_CLIENT_DEBUG(("GattAncs: Add ANCS scan filter\n"));
    ConnectionBleAddAdvertisingReportFilter(AD_TYPE_SERVICE_UUID_128BIT_LIST, sizeof(ancs_ble_advertising_filter), sizeof(ancs_ble_advertising_filter), ancs_ble_advertising_filter);
}
    
/****************************************************************************/
bool sinkGattAncsClientAddService(uint16 cid, uint16 start, uint16 end)
{
    GANCS *gancs = NULL;
    gatt_client_services_t *client_services = NULL;
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    uint16 *service = gattClientAddService(connection, sizeof(GANCS));
    
    if (service)
    {
        client_services = gattClientGetServiceData(connection);
        client_services->ancs = (GANCS *)service;
        gancs = client_services->ancs;
        if (GattAncsInit(gancs, sinkGetBleTask(), cid, start, end) == gatt_ancs_status_initiating)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/****************************************************************************/
void sinkGattAncsClientRemoveService(GANCS *gancs)
{
    GattAncsDestroy(gancs);
}


/******************************************************************************/
void sinkGattAncsClientMsgHandler (Task task, MessageId id, Message message)
{
    switch(id)
    {
        case GATT_ANCS_SET_NS_NOTIFICATION_CFM:
        {
            gattAncsSetNSNotificationCfm((GATT_ANCS_SET_NS_NOTIFICATION_CFM_T*)message);
        }
        break;
        case GATT_ANCS_INIT_CFM:
        {
            gattAncsInitCfm((GATT_ANCS_INIT_CFM_T*)message);
        }
        break;
        case GATT_ANCS_NS_IND:
        {
            gattAncsNSNotificationInd((GATT_ANCS_NS_IND_T*)message);
        }
        break;
        default:
        {
            GATT_ANCS_CLIENT_DEBUG(("Unhandled ANCS msg[%x]\n", id));
        }
        break;
    }
}


#endif /* GATT_ANCS_CLIENT */

