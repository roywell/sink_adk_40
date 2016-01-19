/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2015

FILE NAME
    sink_gatt_client_ias.c

DESCRIPTION
    Routines to handle the GATT Imm Alert Service.
*/

#include <stdlib.h>
#include <connection.h>
#include <gatt.h>

#include "sink_gatt_client_ias.h"

#include "sink_debug.h"
#include "sink_gatt_client.h"
#include "sink_gatt_client_hid.h"
#include "sink_private.h"
#include "sink_ble_scanning.h"
#include "sink_gap_ad_types.h"

#ifdef GATT_IAS_CLIENT

static const uint8 ias_ble_advertising_filter[] = {GATT_SERVICE_UUID_IMMEDIATE_ALERT & 0xFF, GATT_SERVICE_UUID_IMMEDIATE_ALERT >> 8};

#ifdef DEBUG_GATT_IAS_CLIENT
#define GATT_IAS_CLIENT_DEBUG(x) DEBUG(x)
#else
#define GATT_IAS_CLIENT_DEBUG(x)
#endif

/* During alerting the phone, we are the peripheral device. Hence only IAS client is checked */
/* During alerting the remote, we are the central device and the alert needs to be sent on HID */

/* If the device to be alerted is a phone */
#define IS_PHONE_TO_BE_ALERTED(type) (type == sink_gatt_ias_alert_phone)

/* If the device to be alerted is a remote */
#define ALERT_FOR_REMOTE(type) (type == sink_gatt_ias_alert_remote)
/* Check for HID Client connection to alert */
#define HID_CONNECTED(services) ((services->hidc_instance1)||(services->hidc_instance2))
#define IS_REMOTE_TO_BE_ALERTED(type, services) (ALERT_FOR_REMOTE(type) && HID_CONNECTED(services))

/* Clearing the alert */
#define IS_CLEARING_ALERT(type) (type == sink_gatt_ias_alert_none)
/****************************STATIC FUNCTIONS************************************/

/*******************************************************************************
NAME
    gattIasClientFindConnection

DESCRIPTION
    Finds a client connection associated with a ias instance.

PARAMETERS
    giasc    The Imm Alert client instance pointer

RETURNS
    The client connection pointer associated with the ias instance. NULL if not found.

*/
static gatt_client_connection_t *gattIasClientFindConnection(const GIASC_T *giasc)
{
    uint16 index = 0;
    gatt_client_services_t *data = NULL;

    if (giasc == NULL)
    {
        return NULL;
    }

    for (index = 0; index < GATT_CLIENT.number_connections; index++)
    {
        data = gattClientGetServiceData(&GATT_CLIENT.connection[index]);
        if (data && (data->iasc == giasc))
        {
            return &GATT_CLIENT.connection[index];
        }
    }

    return NULL;
}

/*******************************************************************************
NAME
    gattIasClientServiceInitialised

DESCRIPTION
    Called when the Imm Alert service is initialised.

PARAMETERS
    giasc    The Imm Alert client instance pointer

*/
static void gattIasClientServiceInitialised(const GIASC_T *giasc)
{
    gatt_client_connection_t *conn = gattIasClientFindConnection(giasc);

    if (conn != NULL)
    {
        gattClientDiscoveredServiceInitialised(conn);
    }
}

/*******************************************************************************
NAME
    gattIasClientSetCachedLevel

DESCRIPTION
    Sets the cached remote alert level

PARAMETERS
    level    The alert level to cache

*/
static void gattIasClientSetCachedLevel(uint8 level)
{
    GATT_IAS_CLIENT_DEBUG(("GATT Set IAS alert level cache=[%u]\n", level));
    GATT_CLIENT.cache.iasc_alert_level = level;
}

/*******************************************************************************
NAME
    gattIasClientInitCfm

DESCRIPTION
    Handle the GATT_IMM_ALERT_CLIENT_INIT_CFM message

PARAMETERS
    cfm    The GATT_IMM_ALERT_CLIENT_INIT_CFM message
*/
static void gattIasClientInitCfm(const GATT_IMM_ALERT_CLIENT_INIT_CFM_T *cfm)
{
    GATT_IAS_CLIENT_DEBUG(("GATT_IMM_ALERT_CLIENT_INIT_CFM status[%u] cid=[0x%x] \n", cfm->status, cfm->cid));

    /* The service initialisation is complete */
    gattIasClientServiceInitialised(cfm->imm_alert_client);

    if ((cfm->status == gatt_imm_alert_client_status_success) &&
        (gattIasClientFindConnection(cfm->imm_alert_client) != NULL))
    {
        /* Initialisation - Set alert level */
        if (sinkGattIasClientGetCachedLevel() == IASC_INVALID_ALERT_LEVEL)
        {
            GATT_IAS_CLIENT_DEBUG(("IAS Client Initialisation done\n"));
        }
        else  /* Case where remote client (eg phone) wants to alert remote peripheral (eg remote control) - Set alert level */
                /* Case where user triggers alert on Soundbar and remote is not connected*/
        {
            GATT_IAS_CLIENT_DEBUG(("IAS Client Remote alert \n"));

            /* The cached alert must be stopped after timeout */
            MessageSendLater(&theSink.task, EventSysImmAlertTimeout, 0, D_SEC(theSink.conf1->timeouts.ImmediateAlertStopTimeout_s));

            /* Set alert level */
            GattImmAlertClientSetAlertLevel(cfm->imm_alert_client, (gatt_imm_alert_set_level)sinkGattIasClientGetCachedLevel());

            /* Reset alert level */
            gattIasClientSetCachedLevel(IASC_INVALID_ALERT_LEVEL);
        }
    }
}


/*******************************************************************************
NAME
    gattIasClientSetAlertCfm

DESCRIPTION
    Handle the GATT_IMM_ALERT_CLIENT_SET_ALERT_CFM message

PARAMETERS
    cfm    The GATT_IMM_ALERT_CLIENT_SET_ALERT_CFM message

*/
static void gattIasClientSetAlertCfm(const GATT_IMM_ALERT_CLIENT_SET_ALERT_CFM_T *cfm)
{
    GATT_IAS_CLIENT_DEBUG(("GATT_IMM_ALERT_CLIENT_SET_ALERT_CFM status[%u] \n",
                   cfm->status));
}


/****************************GLOBAL FUNCTIONS***********************************/

/****************************************************************************/
void sinkGattIasClientSetupAdvertisingFilter(void)
{

    GATT_IAS_CLIENT_DEBUG(("GattIas: Add IAS scan filter\n"));
    ConnectionBleAddAdvertisingReportFilter(AD_TYPE_SERVICE_UUID_16BIT_LIST, sizeof(ias_ble_advertising_filter), sizeof(ias_ble_advertising_filter), ias_ble_advertising_filter);
}

/****************************************************************************/
bool sinkGattIasClientAddService(uint16 cid, uint16 start, uint16 end)
{
    gatt_client_services_t *client_services = NULL;
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    uint16 *service = gattClientAddService(connection, sizeof(GIASC_T));

    GATT_IAS_CLIENT_DEBUG(("Add Imm Alert Client Service cid=[0x%x] \n", cid));

    if (service)
    {
        GATT_IMM_ALERT_CLIENT_INIT_PARAMS_T params;
        GIASC_T *giasc = NULL;

        client_services = gattClientGetServiceData(connection);
        client_services->iasc = (GIASC_T *)service;
        giasc = client_services->iasc;

        params.cid = cid;
        params.start_handle = start;
        params.end_handle = end;

        if (GattImmAlertClientInit(sinkGetBleTask(), giasc, &params) == gatt_imm_alert_client_status_initiated)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/****************************************************************************/
void sinkGattIasClientRemoveService(GIASC_T *giasc, uint16 cid)
{
    GattImmAlertClientDestroy(giasc, cid);
}

/****************************************************************************/
void sinkGattIasClientInit(void)
{
    GATT_IAS_CLIENT_DEBUG(("GATT Immediate Alert Client Service initialised \n"));

    /* Initialise cached values */
    GATT_CLIENT.cache.iasc_alert_level = IASC_INVALID_ALERT_LEVEL;
}

/****************************************************************************/
uint16 sinkGattIasClientGetCachedLevel(void)
{
    GATT_IAS_CLIENT_DEBUG(("GATT Get IAS alert level cache=[%u]\n", GATT_CLIENT.cache.iasc_alert_level));

    return GATT_CLIENT.cache.iasc_alert_level;
}

/****************************************************************************/
void sinkGattIasClientSetAlert(gatt_imm_alert_set_level alert_level, sink_gatt_ias_dev_type dev_type)
{
    gatt_client_services_t *client_services = NULL;
    gatt_client_connection_t *connection = NULL;
    bool alert_sent = FALSE;
    uint16 id = 0;

    GATT_IAS_CLIENT_DEBUG(("Set Alert level on remote device \n"));

    /*Check if the remote device is connected*/
    for (id = 0; id < GATT_CLIENT.number_connections; id++)
    {
        if (GATT_CLIENT.connection[id].cid)
        {
            connection = gattClientFindByCid(GATT_CLIENT.connection[id].cid);

            /* Get Client services */
            client_services = gattClientGetServiceData(connection);

            if ((client_services != NULL) && (client_services->iasc))
            {
                /* Alert the phone/remote */
                if(IS_CLEARING_ALERT(dev_type) || IS_REMOTE_TO_BE_ALERTED(dev_type, client_services)
                    || IS_PHONE_TO_BE_ALERTED(dev_type))
                {
                    MessageCancelAll(&theSink.task, EventSysImmAlertTimeout);

                    GATT_IAS_CLIENT_DEBUG(("IASC Set Alert \n"));
                    /* Set Alert Level */
                    GattImmAlertClientSetAlertLevel((GIASC_T *)client_services->iasc, alert_level);

                    alert_sent = TRUE;
                    /*Set Timeout Event*/
                    if (alert_level != alert_level_no)
                    {
                        MessageSendLater(&theSink.task, EventSysImmAlertTimeout, 0, D_SEC(theSink.conf1->timeouts.ImmediateAlertStopTimeout_s));
                    }
                }
            }
        }
    }

    /* In case the remote device is not connected Switch to appropriate mode if the device type is known */
    if ((alert_sent == FALSE) && (dev_type != sink_gatt_ias_alert_none))
    {
        sinkGattIasClientSwitchMode((uint8)alert_level, dev_type);
    }
}


/*******************************************************************************
NAME
    sinkGattIasClientSwitchMode

DESCRIPTION
    In case of remote use case:
    Store the alert level and set an event to switch to central.
    This is required to connect to remote control when received a trigger from a remote client (eg phone).
    In case of phone use case:
    Store the alert level and set an event to switch to peripheral.
    This is required for soundbar to connect to phone and alert the phone.

PARAMETERS
    alert level

RETURNS
    ias_alert_status_t
*/
ias_alert_status_t sinkGattIasClientSwitchMode(uint8 alert_level, sink_gatt_ias_dev_type dev_type)
{

    if(!sinkGattHidClientEnabled())
        return ias_alert_not_supported;

    if(alert_level > alert_level_high)
        return ias_alert_param_invalid;

    /*Switch mode based on the use case*/
    switch(dev_type)
    {
    case sink_gatt_ias_alert_remote:
        /* Switch to central mode and begin scanning for connectable whitelisted devices */
        MessageSend(&theSink.task, EventSysBleSwitchCentral, 0);
        break;

    case sink_gatt_ias_alert_phone:
        /*Switch to peripheral mode for phone to connect and alert*/
        MessageSend(&theSink.task, EventSysBleSwitchPeripheral, 0);
        break;

    default:
        Panic();
    }

    /* Store alert level: This will be used init_cfm */
    gattIasClientSetCachedLevel(alert_level);

    return ias_alert_success;
}

/******************************************************************************/
void sinkGattIasClientMsgHandler(Task task, MessageId id, Message message)
{
    switch(id)
    {
        case GATT_IMM_ALERT_CLIENT_INIT_CFM:
        {
            gattIasClientInitCfm((GATT_IMM_ALERT_CLIENT_INIT_CFM_T*)message);
        }
        break;
        case GATT_IMM_ALERT_CLIENT_SET_ALERT_CFM:
        {
            gattIasClientSetAlertCfm((GATT_IMM_ALERT_CLIENT_SET_ALERT_CFM_T*)message);
        }
        break;
        default:
        {
            GATT_IAS_CLIENT_DEBUG(("Unhandled IAS Client msg[%x]\n", id));
        }
        break;
    }
}



#endif /* GATT_IAS_CLIENT */

