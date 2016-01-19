/****************************************************************************
Copyright Cambridge Silicon Radio Limited 2014 - 2015.

FILE NAME
    sink_ble.c

DESCRIPTION
    BLE functionality
*/

#include "sink_ble.h"

#include "sink_ble_advertising.h"
#include "sink_ble_scanning.h"
#include "sink_debug.h"
#include "sink_development.h"
#include "sink_gatt.h"
#include "sink_gatt_client_battery.h"
#include "sink_gatt_client_gatt.h"
#include "sink_gatt_client_hid.h"
#include "sink_gatt_client_dis.h"
#include "sink_gatt_init.h"
#include "sink_gatt_manager.h"
#include "sink_gatt_server_battery.h"
#include "sink_gatt_server_ias.h"
#include "sink_gatt_client_ias.h"
#include "sink_gatt_server_lls.h"
#include "sink_gatt_client_ancs.h"
#include "sink_gatt_client_spc.h"
#include "sink_gatt_server_gap.h"
#include "sink_gatt_server_gatt.h"
#include "sink_private.h"
#include "sink_statemanager.h"
#include "sink_powermanager.h"

#include <connection.h>
#include <gatt.h>
#include <gatt_battery_client.h>
#include <gatt_battery_server.h>
#include <gatt_gap_server.h>
#include <gatt_manager.h>
#include <gatt_server.h>

#include <string.h>
#include <vm.h>


#ifdef GATT_ENABLED


#ifdef DEBUG_BLE
#define BLE_INFO(x) DEBUG(x)
#define BLE_ERROR(x) DEBUG(x) TOLERATED_ERROR(x)
#else
#define BLE_INFO(x)
#define BLE_ERROR(x)
#endif


#define DEVICE_IS_ON                        (stateManagerGetState() != deviceLimbo)
#define CENTRAL_DISCONNECT_EVENT_DELAY_MS   800
#define ENCRYPTION_RETRY_EVENT_DELAY_MS     500

/* Default BLE configuration */
static const ble_configuration_t ble_config = {
                                10,     /* Bondable Pairing timeout (secs) */
                                60,     /* Bondable Connection Timeout (secs) */
                                128,    /* Fast scan interval */
                                12,     /* Fast scan window */
                                120,    /* Gap mode switch timer */
                                10,     /* Time to scan for whitelist devices before reverting to general scanning, 
                                            if a private device has been paired with */
                                30,     /* Fast scan timer */
                                2048,   /* Slow scan interval */
                                18,     /* Slow scan window */
                                32,     /* Fast adv interval min */
                                48,     /* Fast adv interval max */
                                30,     /* Fast adv timer */
                                1000,   /* Slow adv interval min */
                                1200    /* Slow adv interval max */
                                
};


/* Default connection parameters when Master and initiating connection (parameters to allow quick connection/discovery of database) */
static const ble_connection_initial_parameters_t ble_master_initial_conn_params = {
                                6,      /* connection interval min */
                                40,     /* connection interval max */
                                0,      /* connection latency */
                                400,    /* supervision timeout */
                                50,     /* connection attempt timeout */
                                64,     /* connection latency max - slave request */
                                400,    /* supervision timeout min - slave request */
                                400     /* supervision timeout max - slave request */
};


/* Default connection parameters when Master and the connection has been initiated (initial discovery has been performed,
   can relax parameters) */
static const ble_connection_update_parameters_t ble_master_update_conn_params = {
                                80,     /* connection interval min */
                                88,     /* connection interval max */
                                8,      /* connection latency */
                                400,    /* supervision timeout */
                                0,      /* ce_length_min */
                                160     /* ce_length_max */
};


/* Default connection update parameters when Slave and the connection has been established */
static const ble_connection_update_parameters_t ble_slave_update_conn_params = {
                                72,     /* connection interval min */
                                88,     /* connection interval max */
                                4,      /* connection latency */
                                400,    /* supervision timeout */
                                0,      /* ce_length_min */
                                160     /* ce_length_max */
};


/*******************************************************************************
NAME
    handleInitComplete
    
DESCRIPTION
    Handle when BLE_INTERNAL_MESSAGE_INIT_COMPLETE message was received
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void handleInitComplete(void)
{
    /* After initialisation decide if scanning/advertising needs to start */
    if (sinkBleIsActiveOnPowerOff() || DEVICE_IS_ON)
    {
        /* Power on BLE */
        sinkBlePowerOnEvent();     
    }
}

/*******************************************************************************
NAME
    handleEncryptionRetry
    
DESCRIPTION
    Handle when BLE_INTERNAL_MESSAGE_ENCRYPTION_RETRY_TIMER message was received
    
PARAMETERS
    payload The message payload
    
RETURNS
    None
*/
static void handleEncryptionRetry(Message message)
{
    ble_gap_event_t event;
    ble_gap_event_args_t args;
    uint16 cid = *(uint16 *) message;
    tp_bdaddr tpaddrt;
    bool active_conn = FALSE;

    /* Send GAP event to retry encryption */
    if (cid != 0)
        active_conn = VmGetBdAddrtFromCid(cid, &tpaddrt);
    
    if (active_conn)
    {
        event.id = ble_gap_event_retry_encryption;
        args.encryption_retry.taddr = tpaddrt.taddr;
        event.args = &args;
        sinkBleGapEvent(event);
    }
}

/*******************************************************************************
NAME
    sinkBleHandleCLMessage
    
DESCRIPTION
    Connection library messages that are sent to the BLE message handler.
    
PARAMETERS
    task    The task the message is delivered
    id      The ID for the GATT message
    payload The message payload
    
RETURNS
    None
*/
static void sinkBleHandleCLMessage(Task task, MessageId id, Message message)
{
    switch (id)
    {
        case CL_DM_LOCAL_NAME_COMPLETE:
        {
            sinkBleGapReadLocalNameComplete((CL_DM_LOCAL_NAME_COMPLETE_T*)message);
        }
        break;
        case CL_DM_BLE_SECURITY_CFM:
        {
            sinkBleGapSetSecurityCfm((CL_DM_BLE_SECURITY_CFM_T*)message);
        }
        break;
        case CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM:
        {
            BLE_INFO(("CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM [%x]\n", ((CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM_T*)message)->status));
        }
        break;
        default:
        {
            BLE_ERROR(("Unhandled BLE connection msg [0x%x]\n", id));
        }
        break;
    }
}


/*******************************************************************************
NAME
    bleInternalMsgHandler
    
DESCRIPTION
    Internal BLE messages that are sent to this message handler.
    
PARAMETERS
    task    The task the message is delivered
    id      The ID for the GATT message
    payload The message payload
    
RETURNS
    None
*/
static void bleInternalMsgHandler(Task task, MessageId id, Message message)
{
    ble_gap_event_t event;
    
    switch (id)
    {
        case BLE_INTERNAL_MESSAGE_INIT_COMPLETE:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_INIT_COMPLETE\n"));
            
            handleInitComplete();
        }
        break;
        case BLE_INTERNAL_MESSAGE_EVENT_NO_CONNECTIONS:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_EVENT_NO_CONNECTIONS\n"));
        
            /* Send GAP event to indicate there are no BLE connections */
            event.id = ble_gap_event_no_connections;
            event.args = NULL;
            sinkBleGapEvent(event);
        }
        break;
        case BLE_INTERNAL_MESSAGE_WHITELIST_TIMER:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_WHITELIST_TIMER\n"));
            
            /* Send GAP event to indicate whitelist timer */
            event.id = ble_gap_event_whitelist_timeout;
            event.args = NULL;
            sinkBleGapEvent(event);
        }
        break;
        case BLE_INTERNAL_MESSAGE_FAST_SCAN_TIMER:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_FAST_SCAN_TIMER\n"));
            
            /* Send GAP event to indicate the fast scan has timed out */
            event.id = ble_gap_event_fast_scan_timeout;
            event.args = NULL;
            sinkBleGapEvent(event);
        }
        break;
        case BLE_INTERNAL_MESSAGE_FAST_ADV_TIMER:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_FAST_ADV_TIMER\n"));
            
            /* Send GAP event to indicate the fast scan has timed out */
            event.id = ble_gap_event_fast_adv_timeout;
            event.args = NULL;
            sinkBleGapEvent(event);
        }
        break;
        case BLE_INTERNAL_MESSAGE_ENCRYPTION_RETRY_TIMER:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_ENCRYPTION_RETRY_TIMER\n"));
            handleEncryptionRetry(message);
        }
        break;
        case BLE_INTERNAL_MESSAGE_REDISCOVER_SERVER_SERVICES:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_REDISCOVER_SERVER_SERVICES\n"));
            gattClientRediscoverServices(((BLE_INTERNAL_MESSAGE_REDISCOVER_SERVER_SERVICES_T *)message)->cid);
        }
        break;
        case BLE_INTERNAL_MESSAGE_GATT_CLIENT_SERVICE_INITIALISED:
        {
            BLE_INFO(("BLE_INTERNAL_MESSAGE_GATT_CLIENT_SERVICE_INITIALISED\n"));
            gattClientDiscoveredServiceInitialised(gattClientFindByCid(((BLE_INTERNAL_MESSAGE_GATT_CLIENT_SERVICE_INITIALISED_T *)message)->cid));
        }
        break;
		case BLE_INTERNAL_MESSAGE_BATTERY_READ_TIMER:
		{
			BLE_INFO(("BLE_INTERNAL_MESSAGE_BATTERY_READ_TIMER\n"));
			sinkBleBatteryLevelReadSendAndRepeat();
		}
		break;
        default:
        {
            BLE_ERROR(("Unhandled BLE internal msg [0x%x]\n", id));
        }
        break;
    }
}


/******************************************************************************/
void sinkBleInitialiseDevice(void)
{
    ble_gap_role_t gap_role = sinkBleGetGapDefaultRole();
    bool persistent_role = sinkBleGapIsPersistentRole();
            
    BLE_INFO(("Initialise BLE...\n"));
    
    /* Setup BLE Message handler */
    memset(&BLE, 0, sizeof(ble_data_t));
    BLE.task.handler = sinkBleMsgHandler;
    
    /* Setup whitelist from Paired Device List on initialisation */
    ConnectionDmBleAddTdlDevicesToWhiteListReq(TRUE);
    
    /* Initialise GATT */
    if (!sinkGattInitInitialiseDevice())
    {
        FATAL_ERROR(("GATT failed to initialise!\n"));
    }
    
    if ((persistent_role && sinkBleGapIsRoleUnknown(gap_role)) || !persistent_role)
    {
        /* Set initial GAP role */
        if (sinkGattBatteryClientEnabled() || sinkGattHidClientEnabled())
        {
            /* If any services with default role Central are supported, the default role of the headset shall be Central. */
            gap_role = ble_gap_role_central;
        }
        else
        {
            /* If only services with default role Peripheral are supported, the default role of the headset shall be Peripheral. */
            gap_role = ble_gap_role_peripheral;
        }
    }
    
    /* Initialise GAP */
    sinkBleGapInitialise(gap_role);

    /* Setup advertising filters for supported services */
    if(theSink.features.BleAdvertisingFilter & (1<<ANCS_AD_BIT))
    {
        sinkGattAncsClientSetupAdvertisingFilter();
    }
    if(theSink.features.BleAdvertisingFilter & (1<<BATTERY_AD_BIT))
    {
        gattBatteryClientSetupAdvertisingFilter();
    }
    if(theSink.features.BleAdvertisingFilter & (1<<DIS_AD_BIT))
    {
        sinkGattDisClientSetupAdvertisingFilter();
    }
    if(theSink.features.BleAdvertisingFilter & (1<<HID_AD_BIT))
    {
        sinkGattHidClientSetupAdvertisingFilter();
    }
    if(theSink.features.BleAdvertisingFilter & (1<<IAS_AD_BIT))
    {
        sinkGattIasClientSetupAdvertisingFilter();
    }
    if(theSink.features.BleAdvertisingFilter & (1<<SPC_AD_BIT))
    {
        sinkGattSpClientSetupAdvertisingFilter();
    }
}


/******************************************************************************/
void sinkBleMsgHandler(Task task, MessageId id, Message message)
{
    if ( (id >= CL_MESSAGE_BASE) && (id <= CL_MESSAGE_TOP) )
    {
        sinkBleHandleCLMessage(task, id,  message); 
    }
    else if ( (id >= GATT_MESSAGE_BASE) && (id <= GATT_MESSAGE_TOP))
    {
        sinkGattMsgHandler(task, id, message);
    }
    else if ((id >= GATT_MANAGER_MESSAGE_BASE) && (id <= GATT_MANAGER_MESSAGE_TOP))
    {
        sinkGattManagerMsgHandler(task, id, message);
    }
    else if ( (id >= GATT_SERVER_MESSAGE_BASE) && (id <= GATT_SERVER_MESSAGE_TOP))
    {
        sinkGattServerMsgHandler(task, id, message);
    }
    else if ( (id >= GATT_GAP_SERVER_MESSAGE_BASE) && (id <= GATT_GAP_SERVER_MESSAGE_TOP))
    {
        if (sinkGattGapServerMsgHandler(task, id, message) == gap_msg_read_name_required)
        {
            sinkBleGapStartReadLocalName(ble_gap_read_name_gap_server);
        }
    }
    else if ( (id >= GATT_BATTERY_SERVER_MESSAGE_BASE) && (id <= GATT_BATTERY_SERVER_MESSAGE_TOP))
    {
        sinkGattBatteryServerMsgHandler(task, id, message);
    }
    else if ( (id >= GATT_BATTERY_CLIENT_MESSAGE_BASE) && (id <= GATT_BATTERY_CLIENT_MESSAGE_TOP))
    {
        gattBatteryClientMsgHandler(task, id, message);
    }
    else if ( (id >= GATT_IMM_ALERT_SERVER_MESSAGE_BASE) && (id <= GATT_IMM_ALERT_SERVER_MESSAGE_TOP))
    {
        sinkGattImmAlertServerMsgHandler(task, id, message);
    }
    else if( (id >= GATT_LINK_LOSS_SERVER_MESSAGE_BASE) && (id <= GATT_LLS_ALERT_SERVER_MESSAGE_TOP))
    {
        sinkGattLinkLossServerMsgHandler(task, id, message);
    }
    else if ( (id >= GATT_IMM_ALERT_CLIENT_MESSAGE_BASE) && (id <= GATT_IMM_ALERT_CLIENT_MESSAGE_TOP))
    {
        sinkGattIasClientMsgHandler(task, id, message);
    }
    else if( (id >= GATT_ANCS_MESSAGE_BASE) && (id <= GATT_ANCS_MESSAGE_TOP))
    {
        sinkGattAncsClientMsgHandler(task, id, message);
    }    
    else if((id >= GATT_HID_CLIENT_MESSAGE_BASE) && (id<= GATT_HID_CLIENT_MESSAGE_TOP))
    {
        sinkGattHidClientMsgHandler(task, id, message);
    }
    else if((id >= GATT_DEVICE_INFO_CLIENT_MESSAGE_BASE) && (id<= GATT_DEVICE_INFO_CLIENT_MESSAGE_TOP))
    {
        sinkGattDisClientMsgHandler(task, id, message);
    }
    else if((id >= GATT_SCAN_PARAMS_CLIENT_MESSAGE_BASE) && (id<= GATT_SCAN_PARAMS_CLIENT_MESSAGE_TOP))
    {
        sinkGattSpClientMsgHandler(task, id, message);
    }
    else if((id >= GATT_CLIENT_MESSAGE_BASE) && (id<= GATT_CLIENT_MESSAGE_TOP))
    {
        sinkGattClientServiceMsgHandler(task, id, message);
    }
    else if ( (id >= BLE_INTERNAL_MESSAGE_BASE) && (id <= BLE_INTERNAL_MESSAGE_TOP))
    {
        bleInternalMsgHandler(task, id, message);
    }
    else
    {
        BLE_ERROR(("Unhandled BLE msg [0x%x]\n", id));
    }
}


/******************************************************************************/
void sinkBleBondableEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event to become bondable */
    event.id = ble_gap_event_bondable;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleBondablePairingTimeoutEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event to exit bondable mode */
    event.id = ble_gap_event_bondable_pairing_timeout;
    event.args = NULL;
    sinkBleGapEvent(event);
}

/******************************************************************************/
void sinkBleBondableConnectionTimeoutEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event to exit bondable connection mode */
    event.id = ble_gap_event_bondable_connection_timeout;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleCentralConnCompleteEvent(uint16 cid)
{
    gatt_client_connection_t *connection = NULL;
    ble_gap_event_t event;
    ble_gap_event_args_t args;
    tp_bdaddr tpaddrt;
    bool active_conn = FALSE;

    connection = gattClientFindByCid(cid);
    
    if (connection != NULL)
        active_conn = VmGetBdAddrtFromCid(cid, &tpaddrt);
    
    /* Send GAP event when central connection has complete, which can be used to restart scanning */
    event.id = ble_gap_event_central_conn_complete;
    if (!active_conn)
    {
        event.args = NULL;
    }
    else
    {
        args.central_conn_complete.taddr = tpaddrt.taddr;
        event.args = &args;
    }
    sinkBleGapEvent(event);
}

/******************************************************************************/
void sinkBlePairingCompleteEvent (void)
{
    ble_gap_event_t event;
        
    /* Send GAP event when central connection has complete while in peripheral role */
    event.id = ble_gap_event_pairing_complete;
    event.args = NULL;
    sinkBleGapEvent(event);
}

/******************************************************************************/
void sinkBleSwitchPeripheralEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event when user switches to peripheral mode */
    event.id = ble_gap_event_switch_peripheral;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleSwitchCentralEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event when user switches to central mode */
    event.id = ble_gap_event_switch_central;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleCentralDisconnectionEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event for disconnection linked to central role */
    event.id = ble_gap_event_central_disconn;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleCancelAdvertisingEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event when advertising is cancelled */
    event.id = ble_gap_event_cancel_advertising;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleRemoteConnectionSuccessEvent(uint16 cid)
{
    ble_gap_event_t event;
    ble_gap_event_args_t args;
    tp_bdaddr tpaddrt;
    bool active_conn = FALSE;
    
    /* Remove no connections message from queue, it no longer applies */
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_EVENT_NO_CONNECTIONS);
        
    if (cid != 0)
        active_conn = VmGetBdAddrtFromCid(cid, &tpaddrt);
    
    /* Send GAP event when remote connection succeeded */
    event.id = ble_gap_event_remote_conn_success;
    
    if (!active_conn)
    {
        event.args = NULL;
    }
    else
    {
        args.remote_conn_success.taddr = tpaddrt.taddr;
        event.args = &args;
    }
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleRemoteConnectionFailEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event when connect failed in peripheral role */
    event.id = ble_gap_event_remote_conn_fail;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleLocalConnectionSuccessEvent(void)
{
    ble_gap_event_t event;
    
    /* Remove no connections message from queue, it no longer applies */
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_EVENT_NO_CONNECTIONS);
        
    /* Send GAP event when local connection succeeded */
    event.id = ble_gap_event_local_conn_success;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleRoleTimeoutEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event when role times out */
    event.id = ble_gap_event_switch_timeout;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
void sinkBleCheckNoConnectionsEvent(void)
{
    if (gattClientHasNoClients() && gattServerIsFullyDisconnected())
    {
        /* If no connections exist, send an event to report this.
           Add in a delay to allow the ACL to close before reporting it.
           This is used for the scenario where a user switches from Central<->Peripheral 
           role with devices currently connected.
           The ACL must be closed before the application can start advertising/scanning.
        */
        MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_EVENT_NO_CONNECTIONS);
        MessageSendLater(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_EVENT_NO_CONNECTIONS, 0, CENTRAL_DISCONNECT_EVENT_DELAY_MS);
    }
}

/******************************************************************************/
void sinkBleRetryEncryptionEvent(uint16 cid)
{
    /* If there is pairing in progress then encryption request needs to be retried as Bluestack returns host busy status 
    */
    uint16 *message = PanicUnlessNew(uint16);
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_ENCRYPTION_RETRY_TIMER);
    *message = cid;
    MessageSendLater(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_ENCRYPTION_RETRY_TIMER, message, ENCRYPTION_RETRY_EVENT_DELAY_MS);
}

/******************************************************************************/
void sinkBlePowerOnEvent(void)
{
    ble_gap_event_t event;
        
    /* Send GAP event to power on */
    event.id = ble_gap_event_power_on;
    event.args = NULL;
    sinkBleGapEvent(event);    
}


/******************************************************************************/
void sinkBlePowerOffEvent(void)
{
    ble_gap_event_t event;

    /* Send GAP event to power off */
    event.id = ble_gap_event_power_off;
    event.args = NULL;
    sinkBleGapEvent(event);
}


/******************************************************************************/
const ble_configuration_t *sinkBleGetConfiguration(void)
{
    return &ble_config;
}

/******************************************************************************/
void sinkBleSetAdvertisingParamsDefault(uint16 adv_interval_min, uint16 adv_interval_max)
{
    ble_adv_params_t params;

    params.undirect_adv.adv_interval_min = adv_interval_min;
    params.undirect_adv.adv_interval_max = adv_interval_max;
    params.undirect_adv.filter_policy = ble_filter_none;

    BLE_INFO(("Set BLE Default Advertising Params\n"));

    ConnectionDmBleSetAdvertisingParamsReq(ble_adv_ind, FALSE, BLE_ADV_CHANNEL_ALL, &params);
}

/******************************************************************************/
void sinkBleSetMasterConnectionParamsDefault(uint16 scan_interval, uint16 scan_window)
{
    ble_connection_params params;

    params.scan_interval = scan_interval;
    params.scan_window = scan_window;
    params.conn_interval_min = ble_master_initial_conn_params.conn_interval_min;
    params.conn_interval_max = ble_master_initial_conn_params.conn_interval_max;
    params.conn_latency = ble_master_initial_conn_params.conn_latency;
    params.supervision_timeout = ble_master_initial_conn_params.supervision_timeout;
    params.conn_attempt_timeout = ble_master_initial_conn_params.conn_attempt_timeout;
    params.conn_latency_max = ble_master_initial_conn_params.conn_latency_max;
    params.supervision_timeout_min = ble_master_initial_conn_params.supervision_timeout_min;
    params.supervision_timeout_max = ble_master_initial_conn_params.supervision_timeout_max;
    params.own_address_type = TYPED_BDADDR_PUBLIC;
    
    BLE_INFO(("Set BLE Default Connection Params\n"));
    
    ConnectionDmBleSetConnectionParametersReq(&params);
}


/******************************************************************************/
void sinkBleSetMasterConnectionParamsUpdate(typed_bdaddr *taddr)
{
    BLE_INFO(("Set BLE Updated Master Connection Params\n"));
    
    ConnectionDmBleConnectionParametersUpdateReq(
                sinkGetBleTask(),
                taddr,
                ble_master_update_conn_params.conn_interval_min,
                ble_master_update_conn_params.conn_interval_max,
                ble_master_update_conn_params.conn_latency,
                ble_master_update_conn_params.supervision_timeout,
                ble_master_update_conn_params.ce_length_min,
                ble_master_update_conn_params.ce_length_max
                );
}


/******************************************************************************/
void sinkBleSetSlaveConnectionParamsUpdate(typed_bdaddr *taddr)
{
    BLE_INFO(("Set BLE Updated Slave Connection Params\n"));
    
    ConnectionDmBleConnectionParametersUpdateReq(
                sinkGetBleTask(),
                taddr,
                ble_slave_update_conn_params.conn_interval_min,
                ble_slave_update_conn_params.conn_interval_max,
                ble_slave_update_conn_params.conn_latency,
                ble_slave_update_conn_params.supervision_timeout,
                ble_slave_update_conn_params.ce_length_min,
                ble_slave_update_conn_params.ce_length_max
                );
}


/******************************************************************************/
void sinkBleGetAdvertisingParameters(bool fast_adv, ble_advertising_parameters_t *adv_params)
{
    if (fast_adv)
    {
        adv_params->interval_min = sinkBleGetConfiguration()->adv_interval_min_fast;
        adv_params->interval_max = sinkBleGetConfiguration()->adv_interval_max_fast;
    }
    else
    {
        adv_params->interval_min = sinkBleGetConfiguration()->adv_interval_min_slow;
        adv_params->interval_max = sinkBleGetConfiguration()->adv_interval_max_slow;
    }
}


/******************************************************************************/
void sinkBleGetScanningParameters(bool fast_scan, ble_scanning_parameters_t *scan_params)
{
    if (fast_scan)
    {
        scan_params->interval = sinkBleGetConfiguration()->scan_interval_fast;
        scan_params->window = sinkBleGetConfiguration()->scan_window_fast;
    }
    else
    {
        scan_params->interval = sinkBleGetConfiguration()->scan_interval_slow;
        scan_params->window = sinkBleGetConfiguration()->scan_window_slow;
    }
}

/******************************************************************************/
void sinkBleDeleteMarkedDevices(void)
{
    if (BdaddrIsZero(&GAP.security_failed_device.addr))
    {
        /* No device is marked for deletion */
        return;
    }
    /* Delete the device from TDL */
    ConnectionSmDeleteAuthDeviceReq(GAP.security_failed_device.type, &GAP.security_failed_device.addr);
    BLE_INFO(("Marked LE Device deleted...\n"));
    /* clear the marked device */
    BdaddrTypedSetEmpty(&GAP.security_failed_device);
}

/******************************************************************************/
void sinkBleSimplePairingCompleteInd(const CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T *ind)
{
    uint16 cid = GattGetCidForBdaddr(&ind->tpaddr.taddr);

    /* A client might have written Client Characteristic Configuration data before bonding.
       If the device is now bonded, this configuration should persist for future connections. */
    if (cid && (ind->status == success))
    {
        gattClientStoreConfigAttributes(cid, gatt_attr_service_all);

        /* Make sure that priority devices are looked after in the PDL.*/
        deviceManagerUpdatePriorityDevices();        
    }
}


#endif /* GATT_ENABLED */

