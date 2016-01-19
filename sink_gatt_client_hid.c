/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2015

FILE NAME
    sink_gatt_client_hid.c

DESCRIPTION
    Routines to handle the GATT HID Service.
*/

#include <stdlib.h>
#include <connection.h>
#include <gatt.h>

/* Include dependent Headers */
#include "sink_gatt_client.h"
#include "sink_private.h"
#include "sink_utils.h"
#include "sink_debug.h"
#include "sink_gap_ad_types.h"

/* Include Module Header */
#include "sink_gatt_client_hid.h"

#ifdef GATT_HID_CLIENT

static const uint8 hid_ble_advertising_filter[] = {GATT_SERVICE_UUID_HUMAN_INTERFACE_DEVICE & 0xFF, GATT_SERVICE_UUID_HUMAN_INTERFACE_DEVICE >> 8};

#ifdef DEBUG_GATT_HID_CLIENT
#define GATT_HID_CLIENT_DEBUG(x) DEBUG(x)
#else
#define GATT_HID_CLIENT_DEBUG(x) 
#endif


/*******************************************************************************
NAME
    gattHidFindConnection
    
DESCRIPTION
    Finds a client connection associated with a hid instance.
    
PARAMETERS
    ghidc    The hid client instance pointer
    
RETURNS    
    The client connection pointer associated with the hid instance. NULL if not found.    
    
*/
static gatt_client_connection_t *gattHidFindConnection(const GHIDC_T *ghidc)
{
    uint16 index = 0;
    gatt_client_services_t *data = NULL;
    
    if (ghidc == NULL)
    {
        return NULL;
    }
    
    for (index = 0; index < GATT_CLIENT.number_connections; index++)
    {
        data = gattClientGetServiceData(&GATT_CLIENT.connection[index]);
        if (data && ((data->hidc_instance1== ghidc) || (data->hidc_instance2 == ghidc)))
        {
            return &GATT_CLIENT.connection[index];
        }
    }
    
    return NULL;
}

/*******************************************************************************
NAME
    gattHIDServiceInitialised
    
DESCRIPTION
    Called when the hid service is initialised.
    
PARAMETERS
    ghidc    The hid client instance pointer
    
*/
static void gattHidServiceInitialised(const GHIDC_T *ghidc)
{
    gatt_client_connection_t *conn = gattHidFindConnection(ghidc);
    
    if (conn != NULL)
    {
        gattClientDiscoveredServiceInitialised(conn);
    }        
}

/*******************************************************************************
NAME
    gattHidInitCfm
    
DESCRIPTION
    Handle the GATT_HID_CLIENT_INIT_CFM message.
    
PARAMETERS
    cfm    The GATT_HID_CLIENT_INIT_CFM message
    
*/
static void gattHidInitCfm(const GATT_HID_CLIENT_INIT_CFM_T *cfm)
{

    GATT_HID_CLIENT_DEBUG(("GattHid: GATT_HID_CLIENT_INIT_CFM status[%u]\n",
                                       cfm->status));
    if ((cfm->status == gatt_hid_client_status_success) &&
        (gattHidFindConnection(cfm->hid_client) != NULL))
    {
        /*Reset the report map parser */
        sinkGattHidRcResetParser();
        /* Read Report Map descriptors */
        GattHidReadReportMap((GHIDC_T*)cfm->hid_client);
        /* Add Hid remote */
        sinkGattHidRcAddRemote(cfm->cid);
    }
    /* The service initialisation is complete */
    gattHidServiceInitialised(cfm->hid_client);
    
}

/*******************************************************************************
NAME
    gattHidSetNotificationReportIdRegCfm
    
DESCRIPTION
    Handle the GATT_HID_CLIENT_NOTIFICATION_REPROTID_REG_CFM message.
    
PARAMETERS
    cfm    The GATT_HID_CLIENT_NOTIFICATION_REPROTID_REG_CFM_T message
    
*/
static void  gattHidSetNotificationReportIdRegCfm(const GATT_HID_CLIENT_NOTIFICATION_REPROTID_REG_CFM_T* cfm)
{
    GATT_HID_CLIENT_DEBUG(("GattHid:GATT_HID_CLIENT_NOTIFICATION_REPROTID_REG_CFM_T status[%u]\n", cfm->status));
}

/*******************************************************************************
NAME
    gattHidCtrlReqCfm
    
DESCRIPTION
    Handle the GATT_HID_CLIENT_CONTROL_REQ_CFM message.
    
PARAMETERS
    cfm    The GATT_HID_CLIENT_NOTIFICATION_REG_CFM_T message
    
*/
static void gattHidCtrlReqCfm(const GATT_HID_CLIENT_CONTROL_REQ_CFM_T*cfm)
{
    GATT_HID_CLIENT_DEBUG(("GattHid:GATT_HID_CLIENT_CONTROL_REQ_CFM_T status[%u]\n", cfm->status));
}

/*******************************************************************************
NAME
    gattHidReadReportMapCfm
    
DESCRIPTION
    Handle the GATT_HID_CLIENT_READ_REPORT_MAP_CFM message.
    
PARAMETERS
    cfm    The GATT_HID_CLIENT_READ_REPORT_MAP_CFM message
    
*/
static void gattHidReadReportMapCfm(const GATT_HID_CLIENT_READ_REPORT_MAP_CFM_T* cfm)
{
    GATT_HID_CLIENT_DEBUG(("GattHid:GATT_HID_CLIENT_READ_REPORT_MAP_CFM status[%u]\n", cfm->status));
    if(cfm->status == gatt_hid_client_status_success)
    {
        /* Invoke remote control module interface for parsing the report map descriptor */
        sinkGattHidRcProcessReportMapDescriptor(cfm->cid,cfm->size_value,(uint8*)cfm->value,cfm->more_to_come);
    }
}

/*******************************************************************************
NAME
    gattHidNotificationInd
    
DESCRIPTION
    Handle the GATT_HID_CLIENT_NOTIFICATION_IND message.
    
PARAMETERS
    ind    The GATT_HID_CLIENT_NOTIFICATION_IND_T message
    
*/
static void gattHidNotificationInd(const GATT_HID_CLIENT_NOTIFICATION_IND_T *ind)
{

#ifdef DEBUG_GATT_HID_CLIENT
    uint16 index = 0;
#endif
    GATT_HID_CLIENT_DEBUG(("GattHid:GATT_HID_CLIENT_NOTIFICATION_IND_T level[%u]\n", ind->report_id));
#ifdef DEBUG_GATT_HID_CLIENT
    for(index = 0;index< ind->size_value;index++)
    {
        GATT_HID_CLIENT_DEBUG(("GattHid:GATT_HID_CLIENT_NOTIFICATION_IND_T level[%u]\n", ind->value[index]));
    }
#endif
    /* Call remote control module to process the button presses */
    sinkGattHidRcProcessButtonPress(ind->size_value,(uint8*)ind->value,ind->cid);
}

/*******************************************************************************
NAME
    gattHidSetInstance
    
DESCRIPTION
    Sets the appropriate HID client instance
    
PARAMETERS
    cid             The connection ID
    
RETURNS    
    The HID Client Instance if set successfully, NULL otherwise.
    
*/
static GHIDC_T* gattHidSetInstance(uint16 cid)
{
    gatt_client_services_t *client_services = NULL;
    gatt_client_connection_t *connection;
    uint16 *service;
    
    connection = gattClientFindByCid(cid);
    service = gattClientAddService(connection, sizeof(GHIDC_T));

    if(service)
    {
        client_services = gattClientGetServiceData(connection);

        /* Check for free instance */
        if(!client_services->hidc_instance1)
        {
             client_services->hidc_instance1 = (GHIDC_T *)service;
             return client_services->hidc_instance1;
        }
        else if(!client_services->hidc_instance2)
        {
             client_services->hidc_instance2 = (GHIDC_T *)service;
              return client_services->hidc_instance2;
        }
    }
    return NULL;
}

/*******************************************************************************
NAME
    gattHidEnableInputReportNotifications
    
DESCRIPTION
    Enables Report Characterictic- Input Reports  notifications
    this is done for qualification purpose
    
PARAMETERS
    cid             The connection ID
    
RETURNS    
    None
    
*/
static void gattHidEnableInputReportNotifications(uint16 cid)
{
    gatt_client_connection_t *connection;
    gatt_client_services_t *service;

    GATT_HID_CLIENT_DEBUG(("GattHid:gattHidEnableInputReportNotifications \n"));
    /*Get connection from cid */
    connection = gattClientFindByCid(cid);
    service = gattClientGetServiceData(connection);
    
    if(service && service->hidc_instance1)
    {
      /* Enable Input Report Notifications for 1st instance*/
      GattHidRegisterForNotification(service->hidc_instance1, TRUE, hid_client_report_mode);
    }
    if(service && service->hidc_instance2)
    {
      /* Enable Input Report Notifications for 2nd instance if available */
      GattHidRegisterForNotification(service->hidc_instance2, TRUE, hid_client_report_mode);
    }
}

/****************************************************************************/
/* Interface Functions                                                      */     
/****************************************************************************/

/****************************************************************************/
void sinkGattHidClientSetupAdvertisingFilter(void)
{
    GATT_HID_CLIENT_DEBUG(("GattHid: Add HID scan filter\n"));
    ConnectionBleAddAdvertisingReportFilter(AD_TYPE_SERVICE_UUID_16BIT_LIST, sizeof(hid_ble_advertising_filter), sizeof(hid_ble_advertising_filter), hid_ble_advertising_filter);
}


/****************************************************************************/

/****************************************************************************/
void sinkGattHidClientNotificationReqForReportId(uint16 cid,uint16 report_id,bool enable)
{
    gatt_client_connection_t *connection;
    gatt_client_services_t *service;

    GATT_HID_CLIENT_DEBUG(("GattHid:GattHidClient: Func(),sinkGattHidClientNotificationReqForReportId reportId[%d]\n",report_id));
    /*Get connection from cid */
    connection = gattClientFindByCid(cid);
    service = gattClientGetServiceData(connection);
    
    if(service && service->hidc_instance1)
    {
      /* Enable Notification for 1st instance*/
      GattHidRegisterNotificationForReportID(service->hidc_instance1,enable,report_id,hid_client_report_mode);
    }
    if(service && service->hidc_instance2)
    {
      /* Enable Notification for 2nd instance if available */
      GattHidRegisterNotificationForReportID(service->hidc_instance2,enable,report_id,hid_client_report_mode);
    }
}

/****************************************************************************/
void sinkGattHidClientInit(void)
{
    /* Init remote task */
    sinkGattHidRcInit();
}

/****************************************************************************/
bool sinkGattHidClientAddService(uint16 cid, uint16 start, uint16 end)
{
    GHIDC_T *ghidc = NULL;
    GATT_HID_CLIENT_INIT_PARAMS_T ghidc_init;
    /* Configure Remote control timers */
    sinkGattHidRcConfigTimers();
    GATT_HID_CLIENT_DEBUG(("GattHid:sinkGattHidClientAddService() num connection [%u]",GATT_CLIENT.number_connections));
    GATT_HID_CLIENT_DEBUG(("Max Remote Supported [%u]\n",GATT_HID_RC_MAX_REMOTE_SUPORTED));

    /* Check any more new remote can be supported */
    if(GATT_CLIENT.number_connections <= GATT_HID_RC_MAX_REMOTE_SUPORTED)
    {
        GATT_HID_CLIENT_DEBUG(("GattHid:sinkGattHidClientAddService\n"));

        ghidc = gattHidSetInstance(cid);
        if (ghidc)
        {
            ghidc_init.cid = cid;
            ghidc_init.start_handle = start;
            ghidc_init.end_handle = end;
            if (GattHidClientInit(sinkGetBleTask(),ghidc, &ghidc_init, NULL,TRUE) == gatt_hid_client_status_initiated)
            {    
                GATT_HID_CLIENT_DEBUG(("GattHid:Init Success\n"));           
                return TRUE;
            }
        }
    }
    GATT_HID_CLIENT_DEBUG(("GattHid:Init Failed\n"));
    return FALSE;
}

/****************************************************************************/
void sinkGattHIDClientRemoveService(GHIDC_T *ghidc, uint16 cid)
{
    /* Deinit GATT HID client */
    GattHidClientDeInit(ghidc);
}

/****************************************************************************/
void sinkGattHIDClientExtraConfig(void)
{
    uint16 index = 0;
    gatt_client_services_t *data = NULL;
    
    for (index = 0; index < GATT_CLIENT.number_connections; index++)
    {
        data = gattClientGetServiceData(&GATT_CLIENT.connection[index]);
        if (data )
        {
            /* Read external report reference characteristic value for each of the discovered instance, 
                this is required for qualification purpose.
            */
            if(data->hidc_instance1)
            {
                GattHidReadExternalReportReference(data->hidc_instance1);
            }
            if (data->hidc_instance2)
            {
                GattHidReadExternalReportReference(data->hidc_instance2);
            }
        }
    }
}

/******************************************************************************/
void sinkGattHidClientMsgHandler (Task task, MessageId id, Message message)
{
    GATT_HID_CLIENT_DEBUG(("GattHid:sinkGattHidClientMsgHandler %x\n",id));
    switch(id)
    {
        case GATT_HID_CLIENT_INIT_CFM:
        {
            gattHidInitCfm((GATT_HID_CLIENT_INIT_CFM_T*)message);
        }
        break;
        case GATT_HID_CLIENT_NOTIFICATION_IND:
        {
            gattHidNotificationInd((GATT_HID_CLIENT_NOTIFICATION_IND_T*)message);
        }
        break;
        case GATT_HID_CLIENT_CONTROL_REQ_CFM:
        {
            gattHidCtrlReqCfm((GATT_HID_CLIENT_CONTROL_REQ_CFM_T*)message);
        }
        break;
        case GATT_HID_CLIENT_READ_REPORT_MAP_CFM:
        {   
            gattHidReadReportMapCfm((GATT_HID_CLIENT_READ_REPORT_MAP_CFM_T*)message);
        }
        break;
        case GATT_HID_CLIENT_NOTIFICATION_REPROTID_REG_CFM:
        {
            gattHidSetNotificationReportIdRegCfm((GATT_HID_CLIENT_NOTIFICATION_REPROTID_REG_CFM_T*)message);
        }
        break;
        case GATT_HID_CLIENT_READ_EXT_REF_CFM:
        {
            GATT_HID_CLIENT_DEBUG(("GattHid:GATT_HID_CLIENT_READ_EXT_REF_CFM status[%x]\n", ((GATT_HID_CLIENT_READ_EXT_REF_CFM_T*)message)->status));
            /* Enable notifications for Input reports for qualification purpose */
            gattHidEnableInputReportNotifications(((GATT_HID_CLIENT_READ_EXT_REF_CFM_T*)message)->cid);
        }
        break;
        default:
        {
            GATT_HID_CLIENT_DEBUG(("GattHid:Unhandled HIDC msg[%x]\n", id));
        }
        break;
    }
}


#endif /* GATT_HID_CLIENT */

