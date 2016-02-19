/****************************************************************************
Copyright Cambridge Silicon Radio Limited 2014 - 2015.

FILE NAME
    sink_ble_gap.c

DESCRIPTION
    BLE GAP functionality
*/

#include "sink_ble_gap.h"

#include "sink_ble.h"
#include "sink_ble_advertising.h"
#include "sink_ble_scanning.h"
#include "sink_debug.h"
#include "sink_development.h"
#include "sink_gatt_client.h"
#include "sink_gatt_client_ancs.h"
#include "sink_gatt_client_hid.h"
#include "sink_gatt_client_spc.h"
#include "sink_gatt_manager.h"
#include "sink_gatt_server.h"
#include "sink_gatt_server_gap.h"
#include "sink_private.h"

#include <gatt_manager.h>
#include <vm.h>

#ifdef GATT_ENABLED


#ifdef DEBUG_BLE_GAP
#define BLE_GAP_INFO(x) DEBUG(x)
#define BLE_GAP_ERROR(x) DEBUG(x) TOLERATED_ERROR(x)
const char * const gap_states[ble_gap_state_last] = {
    "OFF",
    "ADV_CONN",
    "SCAN_CONN",
    "IDLE_P",
    "IDLE_C",
    "BOND_ADV",
    "BOND_SCAN",
    "CONNECTING",
    "BOND_CONNECTING",
    "BOND_CONNECTED"
};
const char * const gap_events[ble_gap_event_last] = {
    "POWER_ON",
    "BONDABLE",
    "SET_ADV_COMPLETE",
    "BOND_PAIR_TIMEOUT",
    "CONNECT_ATTEMPT",
    "CONNECT_COMPLETE",
    "CENTRAL_DISCONNECT",
    "LOCAL_CONNECT_SUCCESS",
    "SWITCH_CENTRAL",
    "SWITCH_PERIPHERAL",
    "REMOTE_CONNECT_SUCCESS",
    "REMOTE_CONNECT_FAIL",
    "PAIRING_COMPLETE",
    "SWITCH_TIMEOUT",
    "CANCEL_ADVERT",
    "NO_CONNECTIONS",
    "POWER_OFF",
    "WHITELIST_TIMEOUT",
    "FAST_SCAN_TIMEOUT",
    "BOND_CONNECTION_TIMEOUT",
    "FAST_ADV_TIMEOUT",
    "ENCRYPTION_RETRY"
};
#else
#define BLE_GAP_INFO(x)


#define BLE_GAP_ERROR(x)

#define BLE_INFO(x)

#endif /* DEBUG_BLE_GAP */
      

/*******************************************************************************
NAME
    sinkBleGetGapState
    
DESCRIPTION
    Gets the GAP state.
    
PARAMETERS
    None
    
RETURNS
    The GAP state.
*/
static ble_gap_state_t sinkBleGetGapState(void)
{
    return GAP.state;
}


/*******************************************************************************
NAME
    sinkBleSetGapState
    
DESCRIPTION
    Sets the GAP state.
    
PARAMETERS
    The GAP state.
    
RETURNS
    None
*/
static void sinkBleSetGapState(ble_gap_state_t state)
{
    GAP.state = state;
/*    BLE_GAP_INFO(("GAP new state=[%s]\n", gap_states[state]));*/
    BLE_GAP_INFO(("GAP new state=[%d]\n", state));
	/*MYDEBUG(("set state=%d\r\n", state));*/
}


/*******************************************************************************
NAME
    sinkBleGapSetBondedToPrivacyDevice
    
DESCRIPTION
    Sets if bonded to privacy enable device.
    
PARAMETERS
    is_bonded   TRUE if bonded to privacy enable device. FALSE otherwise.
    
RETURNS
    None
*/
static void sinkBleGapSetBondedToPrivacyDevice(bool is_bonded)
{
    GAP.bonded_to_private_device = is_bonded;
    
    BLE_GAP_INFO(("GAP Bonded to private device=[%u]\n", is_bonded));
}


/*******************************************************************************
NAME
    sinkBleGapSetDiscoveryInProgress
    
DESCRIPTION
    Sets if primary service discovery is in progress.
    
PARAMETERS
    in_progress   TRUE if discovery in progress. FALSE otherwise.
    
RETURNS
    None
*/
void sinkBleGapSetDiscoveryInProgress(bool in_progress)
{
    GAP.service_discovery_inprogress = in_progress;
    
    BLE_GAP_INFO(("GAP Set Discovery in progress=[%u]\n", in_progress));
}

/*******************************************************************************
NAME
    sinkBleGapIsDiscoveryInProgress
    
DESCRIPTION
    Returns the status of discovery in progress flag.
    
PARAMETERS
    None
    
RETURNS
    bool - TRUE if discovery in progress, otherwise FALSE
*/
bool sinkBleGapIsDiscoveryInProgress(void)
{
    BLE_GAP_INFO(("GAP Discovery in progress=[%u]\n", GAP.service_discovery_inprogress));
    return (GAP.service_discovery_inprogress ? TRUE : FALSE);
}

/*******************************************************************************
NAME
    sinkBleGapGetBondedToPrivacyDevice
    
DESCRIPTION
    Gets if bonded to privacy enable device.
    
PARAMETERS
    None
    
RETURNS
    TRUE if bonded to privacy enable device. FALSE otherwise.
*/
static bool sinkBleGapGetBondedToPrivacyDevice(void)
{
    return GAP.bonded_to_private_device;
}


/*******************************************************************************
NAME
    gapStopRoleSwitchTimer
    
DESCRIPTION
    Stops any pending role switch timer.
    
PARAMETERS
    None
    
RETURNS
    TRUE if the role switch timer was cancelled. FASLE otherwise.
*/
static bool gapStopRoleSwitchTimer(void)
{
    BLE_GAP_INFO(("GAP gapStopRoleSwitchTimer\n"));
    /* Cancel any role timeout messages */
    return MessageCancelFirst(sinkGetMainTask(), EventSysBleGapRoleTimeout);
}


/*******************************************************************************
NAME
    gapStartRoleSwitchTimer
    
DESCRIPTION
    Starts a timer when the GAP role is switched by the user. If the timer expires, 
    then the application can switch backs to its default role. 
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartRoleSwitchTimer(void)
{
    uint16 timeout_s = sinkBleGetConfiguration()->gap_role_switch_timer_s;
    
    BLE_GAP_INFO(("GAP gapStartRoleSwitchTimer timeout=[%u s]\n", timeout_s));
    
    if (timeout_s)
    {
        /* Make sure any pending messages are cancelled */
        MessageCancelFirst(sinkGetMainTask(), EventSysBleGapRoleTimeout);
        /* Start role timeout */
        MessageSendLater(sinkGetMainTask(), EventSysBleGapRoleTimeout, 0, D_SEC(timeout_s));
    }
}


/*******************************************************************************
NAME
    gapSetWhitelistScanActive
    
DESCRIPTION
    Sets the whitelist active state.
    
PARAMETERS
    active   The active state
    
RETURNS
    None
*/
static void gapSetWhitelistScanActive(bool active)
{
    GAP.scan.whitelist_active = active;
}


/*******************************************************************************
NAME
    gapGetWhitelistScanActive
    
DESCRIPTION
    Gets the whitelist active state.
    
PARAMETERS
    None
    
RETURNS
    The active state
*/
static ble_gap_scan_speed_t gapGetWhitelistScanActive(void)
{
    return GAP.scan.whitelist_active;
}


/*******************************************************************************
NAME
    gapStartWhitelistTimer
    
DESCRIPTION
    Start timer for Whitelist scanning.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartWhitelistTimer(void)
{
    if (sinkBleGapGetBondedToPrivacyDevice())
    {
        /* Whitelist scanning will not work if bonded to a private device.
           Revert to general scanning after a configurable time which can then attempt to find these private devices */
           
        MessageSendLater(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_WHITELIST_TIMER, 0, D_SEC(sinkBleGetConfiguration()->whitelist_scan_timeout_when_private_addr_s));
    }
    
    /* Keep track of when it is whitelist scanning */
    gapSetWhitelistScanActive(TRUE);
}
        
        
/*******************************************************************************
NAME
    gapStopWhitelistTimer
    
DESCRIPTION
    Stop timer for Whitelist scanning.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopWhitelistTimer(void)
{
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_WHITELIST_TIMER);
    
    /* Keep track of when it is whitelist scanning */
    gapSetWhitelistScanActive(FALSE);
}


/*******************************************************************************
NAME
    gapSetScanSpeed
    
DESCRIPTION
    Sets the scan speed state.
    
PARAMETERS
    speed   The scan speed
    
RETURNS
    None
*/
static void gapSetScanSpeed(ble_gap_scan_speed_t speed)
{
    GAP.scan.speed = speed;
}


/*******************************************************************************
NAME
    gapIsFastScanSpeed
    
DESCRIPTION
    Gets if fast scanning.
    
PARAMETERS
    None
    
RETURNS
    TRUE if scan speed is fast. FALSE otherwise.
*/
static bool gapIsFastScanSpeed(void)
{
    return (GAP.scan.speed == ble_gap_scan_speed_fast ? TRUE : FALSE);
}



/*******************************************************************************
NAME
    gapSetAdvSpeed
    
DESCRIPTION
    Sets the advertising speed state.
    
PARAMETERS
    speed   The advertising speed
    
RETURNS
    None
*/
static void gapSetAdvSpeed(ble_gap_adv_speed_t speed)
{
    GAP.adv.speed = speed;
}


/*******************************************************************************
NAME
    gapIsFastAdvSpeed
    
DESCRIPTION
    Gets if fast advertising.
    
PARAMETERS
    None
    
RETURNS
    TRUE if advertising speed is fast. FALSE otherwise.
*/
static bool gapIsFastAdvSpeed(void)
{
    return (GAP.adv.speed == ble_gap_adv_speed_fast ? TRUE : FALSE);
}


/*******************************************************************************
NAME
    gapStartFastScanTimer
    
DESCRIPTION
    Starts the timer for fast scanning.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartFastScanTimer(void)
{
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_FAST_SCAN_TIMER);
    MessageSendLater(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_FAST_SCAN_TIMER, 0, D_SEC(sinkBleGetConfiguration()->scan_timer_fast_s));

    /* Keep track of when it is fast scanning */
    gapSetScanSpeed(ble_gap_scan_speed_fast);
}


/*******************************************************************************
NAME
    gapStopFastScanTimer
    
DESCRIPTION
    Stops the timer for fast scanning.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopFastScanTimer(void)
{
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_FAST_SCAN_TIMER);
}


/*******************************************************************************
NAME
    gapStartFastAdvTimer
    
DESCRIPTION
    Starts the timer for fast advertising.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartFastAdvTimer(void)
{
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_FAST_ADV_TIMER);
    MessageSendLater(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_FAST_ADV_TIMER, 0, D_SEC(sinkBleGetConfiguration()->adv_timer_fast_s));
}


/*******************************************************************************
NAME
    gapStopFastAdvTimer
    
DESCRIPTION
    Stops the timer for fast advertising.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopFastAdvTimer(void)
{
    MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_FAST_ADV_TIMER);
}


/*******************************************************************************
NAME
    gapStartScanning
    
DESCRIPTION
    Start scanning for devices.
    
PARAMETERS
    new_scan If TRUE will start a new scan, eg. white list scanning first for non-bonding mode
    
RETURNS
    TRUE if the scanning was started. FALSE otherwise.
*/
static bool gapStartScanning(bool new_scan)
{
    bool white_list = TRUE;
    bool fast_scan = TRUE;

    if (gattClientHasMaxCentralClients())
    {
        /* Don't resume scanning if at connection limit */
        BLE_GAP_INFO((" GAP central fully connected, set IDLE\n"));
        sinkBleSetGapState(ble_gap_state_idle_central);
        gapStopWhitelistTimer();
        gapStopFastScanTimer();
        return FALSE;
    }
    
    if (sinkBleGetGapState() == ble_gap_state_bondable_scanning)
    {
        white_list = FALSE;
    }
    else
    {
        sinkBleSetGapState(ble_gap_state_scanning_connected);
        if (new_scan)
        {
            gapStartFastScanTimer();
            gapStartWhitelistTimer();
        }
        else
        {
            white_list = gapGetWhitelistScanActive();
            fast_scan = gapIsFastScanSpeed();
        }
    }
    bleStartScanning(white_list, fast_scan);
    /* Update the scan interval and window to remote scan server */
    sinkGattSpClientSetScanIntervalWindow(fast_scan);
    return TRUE;
}


/*******************************************************************************
NAME
    gapStopScanning
    
DESCRIPTION
    Stop scanning for devices.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopScanning(void)
{
    bleStopScanning();
    gapStopFastScanTimer();
    gapStopWhitelistTimer();
}


/*******************************************************************************
NAME
    gapSetConnectionParamsDefault
    
DESCRIPTION
    Sets the default connection paramaters when no connection active.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapSetConnectionParamsDefault(void)
{
    ble_scanning_parameters_t scan_params;
    
    BLE_GAP_INFO(("GAP Scanning fast=[%u]\n", gapIsFastScanSpeed()));
    
    /* Get the current scan parameters */
    sinkBleGetScanningParameters(gapIsFastScanSpeed(), &scan_params);

    /* Set connection parameters while connecting */
    sinkBleSetMasterConnectionParamsDefault(scan_params.interval, scan_params.window);
}

/*******************************************************************************
NAME
    gapSetAdvertisingParamsDefault
    
DESCRIPTION
    Sets the default advertising paramaters.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapSetAdvertisingParamsDefault(void)
{
    ble_advertising_parameters_t adv_params;

    BLE_GAP_INFO(("Adv fast=[%u]\n", gapIsFastAdvSpeed()));

    /* Get the current advertising parameters */
    sinkBleGetAdvertisingParameters(gapIsFastAdvSpeed(), &adv_params);

    /* Set advertising parameters */
    sinkBleSetAdvertisingParamsDefault(adv_params.interval_min, adv_params.interval_max);
}

/*******************************************************************************
NAME
    gapStartAdvertising
    
DESCRIPTION
    Start advertising to devices
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartAdvertising(void)
{
    /* Change advertising params */
    gapSetAdvertisingParamsDefault();
    
    /* Start sending advertisments */
    sinkGattManagerStartAdvertising();
    
    /* Update state and timers */    
    BLE_GAP_INFO(("GAP advertsising\n"));
    if (sinkBleGetGapState() != ble_gap_state_bondable_advertising)
    {
        /*sinkBleSetGapState(ble_gap_state_advertising_connected);*/
		sinkBleSetGapState(ble_gap_state_bondable_advertising);
        
        if (gapIsFastAdvSpeed())
            gapStartFastAdvTimer();
    }
}


/*******************************************************************************
NAME
    gapStopAdvertising
    
DESCRIPTION
    Stop advertising to devices
    
PARAMETERS
    new_speed       Set the speed for the next advertising event
    
RETURNS
    None
*/
static void gapStopAdvertising(ble_gap_adv_speed_t new_speed)
{
    gapStopFastAdvTimer();
    gapSetAdvSpeed(new_speed);
    sinkGattManagerStopAdvertising();
}


/*******************************************************************************
NAME
    gapStartBondablePairingTimer
    
DESCRIPTION
    Starts a timer when the GAP bonding role is entered.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartBondablePairingTimer(void)
{
    uint16 timeout_s = sinkBleGetConfiguration()->bonding_pairing_period_s;
    
    BLE_GAP_INFO(("GAP gapStartBondable Pairing Timer timeout=[%u s]\n", timeout_s));
    /* Make sure any pending messages are cancelled */
    MessageCancelFirst(sinkGetMainTask(), EventSysBleBondablePairingTimeout);
    /* Start bonding timeout */
    MessageSendLater(sinkGetMainTask(), EventSysBleBondablePairingTimeout, 0, D_SEC(timeout_s));   
}

/*******************************************************************************
NAME
    gapStartBondableConnectionTimer
    
DESCRIPTION
    Starts a connection timer when the GAP bonding role is entered.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartBondableConnectionTimer(void)
{
    uint16 timeout_s = sinkBleGetConfiguration()->bonding_connection_period_s;
    
    BLE_GAP_INFO(("GAP gapStartBondableConnectionTimer timeout=[%u s]\n", timeout_s));
    /* Make sure any pending messages are cancelled */
    MessageCancelFirst(sinkGetMainTask(), EventSysBleBondableConnectionTimeout);
    /* Start bonding timeout */
    MessageSendLater(sinkGetMainTask(), EventSysBleBondableConnectionTimeout, 0, D_SEC(timeout_s));
}


/*******************************************************************************
NAME
    gapStopBondablePairingTimer
    
DESCRIPTION
    Stops a timer when the GAP bonding role is exited.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopBondablePairingTimer(void)
{
    BLE_GAP_INFO(("GAP gapStopBondable Pairing Timer\n"));
    /* End bondable mode */
    if (MessageCancelFirst(sinkGetMainTask(), EventSysBleBondablePairingTimeout))
    {
        /* Send system event if bonding mode was exited */
        MessageSend(sinkGetMainTask(), EventSysBleBondablePairingTimeout, 0);
    }
}

/*******************************************************************************
NAME
    gapStopBondableConnectionTimer
    
DESCRIPTION
    Stops a connection timer when the connection is successful or timer expired.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopBondableConnectionTimer(void)
{
    BLE_GAP_INFO(("GAP gapStopBondableConnectionTimer\n"));
    /* End bondable mode */
    if (MessageCancelFirst(sinkGetMainTask(), EventSysBleBondableConnectionTimeout))
    {
        /* Send system event if bonding mode was exited */
        MessageSend(sinkGetMainTask(), EventSysBleBondableConnectionTimeout, 0);
    }
}


/*******************************************************************************
NAME
    gapDisconnectFromPeripheral
    
DESCRIPTION
    Disconnect from peripheral device. 
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapDisconnectFromPeripheral(void)
{
    /* Try to disconnect devices */
    if (!gattClientDisconnectAll())
    {
        /* Check if all disconnected */
        sinkBleCheckNoConnectionsEvent();
    }
}


/*******************************************************************************
NAME
    gapDisconnectFromCentral
    
DESCRIPTION
    Disconnect from central device. 
    If no current connection exist then will stop advertising.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapDisconnectFromCentral(void)
{
    /* Disconnect current connections */
    if (!gattServerDisconnectAll())
    {
        /* Try to stop advertsting, if no current connections */
       gapStopAdvertising(ble_gap_adv_speed_fast);
    }
}


/*******************************************************************************
NAME
    gapStartPeripheralBonding
    
DESCRIPTION
    Start bonding mode for Peripheral role
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartPeripheralBonding(void)
{
    BLE_GAP_INFO(("GAP gapStartPeripheralBonding\n"));
    
    /* Stop any current advertising or disconnect */
    gapDisconnectFromCentral();
    
    /* Now in bonding state */
    sinkBleSetGapState(ble_gap_state_bondable_advertising);
        
    /* Start bondable Connection  timer */
    gapStartBondableConnectionTimer();
}


/*******************************************************************************
NAME
    gapRetryEncryption
    
DESCRIPTION
    Retry encryption by sending Security request
    
PARAMETERS
    BD address of the device link to be encrypted
    
RETURNS
    None
*/
static void gapRetryEncryption(const typed_bdaddr *taddr)
{
    if (taddr != NULL)
    {
        BLE_GAP_INFO(("Gatt Retry Encryption request \n"));
        
        ConnectionDmBleSecurityReq(sinkGetBleTask(), 
                                   taddr, 
                                   ble_security_encrypted_bonded,
                                   ble_connection_master_directed
                                   );
    }
}


/*******************************************************************************
NAME
    gapStartPeripheralEncryption
    
DESCRIPTION
    Start Encryption for Peripheral role
    
PARAMETERS
    paired_device encryption for paired device

RETURNS
     bool encryption has been started or not
*/
static bool gapStartPeripheralEncryption(bool paired_device)
{
    gatt_client_connection_t *connection = gattClientFindByCid(GATT_SERVER.cid);
    gatt_client_services_t *data = gattClientGetServiceData(connection);
    gatt_client_discovery_t *discover = gattClientGetDiscoveredServices(connection);
    tp_bdaddr current_bd_addr;
    uint16 service_count = 0;
    bool start_encryption = FALSE;

    /* If the remote device is already paired, then this request has come to after the remote device 
      * is successfully connected. We need to check if the remote device has any service which requires
      * encryption so that we can trigger it from our end */
    if(paired_device)
    {
        if((discover) && VmGetBdAddrtFromCid(GATT_SERVER.cid, &current_bd_addr))
        {
            if((connection) && (connection->role == ble_gap_role_peripheral) && (data))
            {
                for(service_count=0; service_count < data->number_discovered_services ; service_count++)
                {
                    discover += service_count;
                    /* Check any services required security */
                    if(discover->service == gatt_client_ancs || discover->service == gatt_client_ias )
                    {
                        start_encryption = TRUE;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        /* The bond timer has expired, if the remote device is still connected then trigger 
          * bonding. ideally the remote device has to trigger bonding checking that there 
          * is an encryption bit in the database. If it fails to do so, trigger it from our end */
        if(VmGetBdAddrtFromCid(GATT_SERVER.cid, &current_bd_addr))
        {
            start_encryption = TRUE;
        }
    }
    BLE_GAP_INFO(("GAP gapStartPeripheralEncryption Start Encryption : %d \n",start_encryption));

    if(start_encryption)
    {
        ConnectionDmBleSecurityReq(sinkGetBleTask(), 
                        (const typed_bdaddr *)&current_bd_addr.taddr, 
                        ble_security_encrypted_bonded,
                        ble_connection_master_directed
                        );

     }

    return start_encryption;
}

/*******************************************************************************
NAME
    gapStopPeripheralBonding
    
DESCRIPTION
    Stop bonding mode for Peripheral role and revert to standard advertising
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopPeripheralBonding(void)
{
    gatt_client_connection_t *connection = gattClientFindByCid(GATT_SERVER.cid);

    BLE_GAP_INFO(("GAP gapStopPeripheralBonding\n"));

    if(connection == NULL)
    {
        BLE_GAP_INFO(("GAP no client connection to this server hence disconnect central \n"));
        /* Stop any current advertising or disconnect */
        gapDisconnectFromCentral();
        sinkBleSetGapState(ble_gap_state_advertising_connected);
    }
    else
    {
        /* Bonding timeout has occured indicating remote device has not initiated bonding
            hence initate encryption to trigger bonding from remote device.
        */
        if(!gapStartPeripheralEncryption(FALSE))
        {
             sinkBleSetGapState(ble_gap_state_advertising_connected);
        }
    }
}


/*******************************************************************************
NAME
    gapStartCentralBonding
    
DESCRIPTION
    Start bonding mode for Central role
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStartCentralBonding(void)
{        
    BLE_GAP_INFO(("GAP gapStartCentralBonding\n"));
 
    if (sinkBleGetGapState() == ble_gap_state_scanning_connected)
    {
        /* Must stop scanning before it is restarted with new parameters */
        gapStopScanning();
    }
    
    /* Now in bonding state */
    sinkBleSetGapState(ble_gap_state_bondable_scanning);
    
    if (gapStartScanning(TRUE))
    {
        /* Start bondable connection timer */
        gapStartBondableConnectionTimer();
    }
}

/*******************************************************************************
NAME
    gapStopCentralBonding
    
DESCRIPTION
    Stop bonding mode for Central role and revert to standard scanning
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopCentralBonding(void)
{        
    BLE_GAP_INFO(("GAP gapStopCentralBonding\n"));
 
    /* Must stop scanning before it is restarted with new parameters */
    gapStopScanning();
        
    if (sinkBleGetGapState() == ble_gap_state_bondable_scanning)
    {   
        /* Revert to normal scan state */
        sinkBleSetGapState(ble_gap_state_scanning_connected);
        
        /* Start scanning in general mode */
        gapStartScanning(TRUE);
    }
}

/*******************************************************************************
NAME
    gapSetSecurityFailedDevice 
    
DESCRIPTION
    Called when failure in encryption has occurred with an already paired device
    due to missing link key reported by remote device.
    
PARAMETERS
    tp_addr   Typed address of the remote device.
    
RETURNS
    None
*/
static void gapSetSecurityFailedDevice (const typed_bdaddr * tp_addr)
{
    tp_bdaddr current_addr;
    tp_bdaddr public_addr;

    current_addr.transport = TRANSPORT_BLE_ACL;
    memcpy(&current_addr.taddr,tp_addr, sizeof(typed_bdaddr));

      /* Retreive permanent address if this is a random address */
    if(current_addr.taddr.type == TYPED_BDADDR_RANDOM)
    {
        VmGetPublicAddress(&current_addr, &public_addr);
    }
    else
    {
        public_addr.transport = TRANSPORT_BLE_ACL;
        memcpy(&public_addr.taddr,tp_addr, sizeof(typed_bdaddr));
    }

    /* Mark the device address to be deleted */
    memcpy(&GAP.security_failed_device, &public_addr.taddr, sizeof(typed_bdaddr));
    
    /* Generate user event to indicate encryption failure */
    BLE_GAP_INFO(("GAP gapEncyptionFailureKeyMissing: Encryption failed event\n"));
    MessageSend(&theSink.task, EventSysBleEncryptionFailed , 0);
}

/*******************************************************************************
NAME
    gapClearSecurityFailedDevice
    
DESCRIPTION
    Check and clear tagged security failure device if encryption has succeeded.
    
PARAMETERS
    tp_addr   Typed address of the remote device.
    
RETURNS
    None
*/
static void gapClearSecurityFailedDevice(const typed_bdaddr * tp_addr)
{
    tp_bdaddr public_addr;

    /* Retreive permanent address if this is a random address */
    if(tp_addr->type == TYPED_BDADDR_RANDOM)
    {
        tp_bdaddr current_addr;
        current_addr.transport = TRANSPORT_BLE_ACL;
        memcpy(&current_addr.taddr,tp_addr, sizeof(typed_bdaddr));
        VmGetPublicAddress(&current_addr, &public_addr);
    }
    else
    {
        public_addr.transport = TRANSPORT_BLE_ACL;
        memcpy(&public_addr.taddr,tp_addr, sizeof(typed_bdaddr));
    }

    /* Check if this device is earlier marked for security failure */
    if(BdaddrIsSame(&public_addr.taddr.addr, &GAP.security_failed_device.addr))
    {
        /* clear the marked device */
        BdaddrTypedSetEmpty(&GAP.security_failed_device);
    }
}

/*******************************************************************************
NAME
    gapProcessEncryptionFailure
    
DESCRIPTION
    This utility function acts on encryption failure by removing the client.
    
PARAMETERS
    connection                  The GATT client connection

RETURNS
    None.
*/
static void gapProcessEncryptionFailure(gatt_client_connection_t *connection)
{
    /* Pairing or encryption failed, so disconnect GATT.
      * Basically either pairing/encryption failed, since in case of peripheral we require encryption
      * and in case of central we anyway require pairing. So, failure in either case
      * disconnect the link as we cannot proceed further */

    if(connection)
    {
        uint16 cid = connection->cid;
        /* Maybe gatt client service is added, so we need to remove it */
        gattClientRemoveServices(connection);
        gattClientRemove(cid);
        GattManagerDisconnectRequest(cid);
    }
}

/*******************************************************************************
NAME
    gapStartConnection
    
DESCRIPTION
    Start connection to remote device.
    
PARAMETERS
    taddr   Typed address of the remote device.
    
RETURNS
    None
*/
static void gapStartConnection(const typed_bdaddr *taddr)
{
    /* Set Connection params before connecting */
    gapSetConnectionParamsDefault();
    /* Start connection attempt */
    sinkGattManagerStartConnection(taddr);
}


/*******************************************************************************
NAME
    gapCentralConnectToBondedDevice
    
DESCRIPTION
    Connect to device if the permanent address is bonded.
    
PARAMETERS
    remote_taddr    Lists the current_taddr and permanent_taddr of the device to connect with
    
RETURNS
    None
*/
static void gapCentralConnectToBondedDevice(const typed_bdaddr *current_taddr, const typed_bdaddr *permanent_taddr)
{          
    bool connection_attempted = FALSE; 

    BLE_GAP_INFO(("GAP Connect to Bonded Device\n"));
    
    if (!BdaddrIsZero(&current_taddr->addr) && !BdaddrIsZero(&permanent_taddr->addr))
    {        
        /* Need to find if permanent_taddr matches an entry in the PDL */
        if (ConnectionSmGetAttributeNow(0, &permanent_taddr->addr, 0, NULL))
        {
            BLE_GAP_INFO(("GAP connect attempt connect_addr=[(%u) %x:%x:%lx]\n",
                      current_taddr->type, current_taddr->addr.nap, current_taddr->addr.uap, current_taddr->addr.lap
                      ));

            gapStartConnection(current_taddr);
            connection_attempted = TRUE;
        }
    }
    
    if (!connection_attempted)
    {
        BLE_GAP_INFO(("    No addr\n"));
        /* Restart scanning if connection wasn't initiated */
        gapStartScanning(FALSE);
    }
}


/*******************************************************************************
NAME
    gapCentralConnectToAnyDevice
    
DESCRIPTION
    Connects to device as a result of scanning for advertising devices.
    
PARAMETERS
    current_taddr       The current address of the device to connect with
    permanent_taddr     The permanent address of the device to connect with
    
RETURNS
    None
*/
static void gapCentralConnectToAnyDevice(const typed_bdaddr *current_taddr, const typed_bdaddr *permanent_taddr)
{          
    bool connection_attempted = FALSE;    
    typed_bdaddr connect_addr;
    
    BLE_GAP_INFO(("GAP Connect To Any Device\n"));
    
    /* Clear address that will be used for connection */
    BdaddrTypedSetEmpty(&connect_addr);
      
    /* Check if any addresses are set that can be used to connect */
    if (!BdaddrIsZero(&current_taddr->addr))
    {
        connect_addr = *current_taddr;
    }
    else if (!BdaddrIsZero(&permanent_taddr->addr))
    {
        connect_addr = *permanent_taddr;
    }
    
    if (!BdaddrTypedIsEmpty(&connect_addr))
    {
        BLE_GAP_INFO(("GAP connect attempt whitelist=[%u]:\n\tconnect_addr=[(%u) %x:%x:%lx]\n\tcurrent_addr=[(%u) %x:%x:%lx]\n\tpermanent_addr=[(%u) %x:%x:%lx]\n",
                      gapGetWhitelistScanActive(),
                      connect_addr.type, connect_addr.addr.nap, connect_addr.addr.uap, connect_addr.addr.lap,
                      current_taddr->type, current_taddr->addr.nap, current_taddr->addr.uap, current_taddr->addr.lap,
                      permanent_taddr->type, permanent_taddr->addr.nap, permanent_taddr->addr.uap, permanent_taddr->addr.lap
                      ));
        /* Connection address is set, start connection attempt */
        gapStartConnection(&connect_addr);
        connection_attempted = TRUE;
    }
    
    if (!connection_attempted)
    {
        /* Stop the bond timer if the state was bondable connecting */
        if (sinkBleGetGapState() == ble_gap_state_bondable_connecting)
        {
            gapStopBondablePairingTimer();
        }
        BLE_GAP_INFO(("    No addr\n"));
        /* Restart scanning if connection wasn't initiated */
        gapStartScanning(FALSE);
    }

}


/*******************************************************************************
NAME
    gapStopScanWhileConnecting
    
DESCRIPTION
    Stop scanning while attempting connection to a device
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStopScanWhileConnecting(void)
{

    /* Set state to connecting */
    if (sinkBleGetGapState() == ble_gap_state_bondable_scanning)
    {
        /* Stop the connection timeout, as there was connection attempt */
        gapStopBondableConnectionTimer();
        sinkBleSetGapState(ble_gap_state_bondable_connecting);
    }
    else
    {
        sinkBleSetGapState(ble_gap_state_connecting);
    }
    
    /* Stop any scanning while connecting */
    gapStopScanning();
}


/*******************************************************************************
NAME
    gapStorePersistentData

DESCRIPTION
    Stores GAP persistent store data.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapStorePersistentData(void)
{
    configManagerWriteSessionData();
}

        
/*******************************************************************************
NAME
    gapCentralPowerOff
    
DESCRIPTION
    Powers off BLE operation when in Central role.
    
PARAMETERS
    disconnect_immediately  If TRUE will disconnect from any peripheral devices.
    
RETURNS
    None
*/
static void gapCentralPowerOff(bool disconnect_immediately)
{
    /* Check if BLE can be powered off */
    if (!sinkBleIsActiveOnPowerOff())
    {
        if (gapStopRoleSwitchTimer())
        {
            /* Switch back to default role on power off */
            MessageSend(sinkGetMainTask(), EventSysBleGapRoleTimeout, 0);
        }

        gapStopBondableConnectionTimer();
        /* Stop bonding */
        gapStopBondablePairingTimer();
        /* Make sure any scanning is stopped immediately before powering off */
        gapStopScanning();
        /* Clear the encryption failure device marked for deletion */
        BdaddrTypedSetEmpty(&GAP.security_failed_device);

        if (disconnect_immediately)
        {
            /* If there are connections in Central role, these must be disconnected */
            gapDisconnectFromPeripheral();
        }
        /* Set state to OFF */
        sinkBleSetGapState(ble_gap_state_off);
    }
}


/*******************************************************************************
NAME
    gapPeripheralPowerOff
    
DESCRIPTION
    Powers off BLE operation when in Peripheral role.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapPeripheralPowerOff(void)
{
    /* Check if BLE can be powered off */
    if (!sinkBleIsActiveOnPowerOff())
    {
        if (gapStopRoleSwitchTimer())
        {
            /* Switch back to default role on power off */
            MessageSend(sinkGetMainTask(), EventSysBleGapRoleTimeout, 0);
        }
        /* Stop Connection *Timer */
        gapStopBondableConnectionTimer();
        /* Stop Bonding Timer */
        gapStopBondablePairingTimer();
        /* Cancel any active role switch timers */
        gapStopRoleSwitchTimer();
        /* Stop any current advertising or disconnect */
        gapDisconnectFromCentral();
        /* Set state to OFF */
        sinkBleSetGapState(ble_gap_state_off);
    }
}


/*******************************************************************************
NAME
    gapSwitchToCentralRole
    
DESCRIPTION
    Called when the current role is switched to Central.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapSwitchToCentralRole(void)
{
    /* Cancel any active role switch timers */
    gapStopRoleSwitchTimer();
    /* End bondable pairing mode */
    gapStopBondablePairingTimer();
    /* End bondable connection mode */
    gapStopBondableConnectionTimer();
    /* Stop any current advertising or disconnect */
    gapDisconnectFromCentral();
    /* Enter scanning state, the GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM 
       will generate an ble_gap_event_cancel_advertising to restart scanning */
    sinkBleSetGapState(ble_gap_state_scanning_connected);
    
    if (sinkBleGapIsPersistentRole())
    {
        /* Store role in persistent mode */
        sinkBleSetGapDefaultRole(ble_gap_role_central);
        gapStorePersistentData();
    }
}


/*******************************************************************************
NAME
    gapSwitchToPeripheralRole
    
DESCRIPTION
    Called when the current role is switched to Peripheral.
    
PARAMETERS
    disconnect      TRUE if current connections should be removed. FALSE otherwise.
    stop_scan       TRUE if scanning should be stopped. FALSE otherwise.
    
RETURNS
    None
*/
static void gapSwitchToPeripheralRole(bool disconnect, bool stop_scan)
{
    /* Stop bondable Pairing Timer */
    gapStopBondablePairingTimer();
    /* Stop Bondable Connection Timer */
    gapStopBondableConnectionTimer();
    /* Cancel any active role switch timers */
    gapStopRoleSwitchTimer();
    if (stop_scan)
    {
        /* Make sure any scanning is stopped immediately before switching to peripheral role */
        gapStopScanning();    
    }
    if (disconnect)
    {
        /* If there are connections in Central role, these must be disconnected before advertising can start */
        gapDisconnectFromPeripheral();
    }
    /* Switch to peripheral state and wait for disconnect messages */
    sinkBleSetGapState(ble_gap_state_advertising_connected);
    
    if (sinkBleGapIsPersistentRole())
    {
        /* Store role in persistent mode */
        sinkBleSetGapDefaultRole(ble_gap_role_peripheral);
        gapStorePersistentData();
    }
}


/*******************************************************************************
NAME
    gapAdvertisingTimeout
    
DESCRIPTION
    Called when the advertising times out.
    
PARAMETERS
    None
    
RETURNS
    None
*/
static void gapAdvertisingTimeout(void)
{
    gapStopAdvertising(ble_gap_adv_speed_fast);
    sinkBleSetGapState(ble_gap_state_idle_peripheral);
}


/*******************************************************************************
NAME
    gapStateOffHandleEvent
    
DESCRIPTION
    Handles an event in the OFF state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateOffHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;


	/*BLE_INFO(("gapStateOffHandleEvent event.id=%d\r\n", event.id));*/

    switch (event.id)
    {
        case ble_gap_event_power_on:
        {
            if (sinkBleGetGapDefaultRole() == ble_gap_role_central)
            {
                sinkBleSetGapState(ble_gap_state_scanning_connected);
            }
            else if (sinkBleGetGapDefaultRole() == ble_gap_role_peripheral)
            {
                sinkBleSetGapState(ble_gap_state_advertising_connected);
            }
            /* Check for no connections before starting scanning/advertising */
            sinkBleCheckNoConnectionsEvent();
        }
        break;
        
        case ble_gap_event_central_conn_complete:
        case ble_gap_event_pairing_complete:
        {
            /* A central connection has just completed. As no longer in central mode,
               need to disconnect all connections. 
            */
            gapDisconnectFromPeripheral();
        }
        break;
        
        /* Events okay to ignore */
        case ble_gap_event_power_off:
        case ble_gap_event_bondable:
        case ble_gap_event_set_advertising_complete:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        case ble_gap_event_central_conn_attempt:
        case ble_gap_event_central_disconn:
        case ble_gap_event_local_conn_success:
        case ble_gap_event_switch_central:
        case ble_gap_event_switch_peripheral:
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        case ble_gap_event_switch_timeout:
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_fast_adv_timeout:
        case ble_gap_event_retry_encryption:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        default:
        {
            event_handled = FALSE;
            BLE_GAP_ERROR(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateAdvConnHandleEvent
    
DESCRIPTION
    Handles an event in the ADVERTISING/CONNECTED state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateAdvConnHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateAdvConnHandleEvent event.id=%d\r\n", event.id));*/

    switch (event.id)
    {
        case ble_gap_event_bondable:
        {
            gapStartPeripheralBonding();
        }
        break;
        
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        {
            /* Start advertising if no Central connections */
            /* Read local name to set new advertising data */
            sinkBleGapStartReadLocalName(ble_gap_read_name_advertising);
            /* Restart role switch timer */
            gapStartRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_set_advertising_complete:
        {
            /* Restart advertising after setting advertising data */
            gapStartAdvertising();
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_peripheral)
            {
                gapSwitchToCentralRole();
            }
            else
            {
                gapAdvertisingTimeout();
            }
        }
        break;
        
        case ble_gap_event_switch_central:
        {      
            gapSwitchToCentralRole();
        }
        break;
        
        case ble_gap_event_remote_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
            
            /* Stop advertising timers and set interval back to fast */
            gapStopFastAdvTimer();
            gapSetAdvSpeed(ble_gap_adv_speed_fast);

            /* Initiate encryption in case of ancs and ias server connections
                if device is previously bonded then encryption will be successful 
                if not bonded do not allow pairing as soundbar is not in bondable
                state.
            */

			(void)gapStartPeripheralEncryption(TRUE);/*roywell 20160120*/
/*			(void)gapStartPeripheralEncryption(FALSE);*/

			/*MessageSend(&theSink.task, EventUsrBleStartBonding, 0 );*/
			
        }
        break;
        
        case ble_gap_event_remote_conn_fail:
        {
            /* Ignore advertising failure */
        }
        break;
        
        case ble_gap_event_central_conn_complete:
        {
            /* A central connection has just completed. As no longer in central mode,
               need to disconnect all central connections. 
            */
            gapDisconnectFromPeripheral();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role */
            gapPeripheralPowerOff();
        }
        break;
        
        case ble_gap_event_fast_adv_timeout:
        {
            /* Stop advertising */
            gapStopAdvertising(ble_gap_adv_speed_slow);
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            /* Restart fast advertising */
            gapStopAdvertising(ble_gap_adv_speed_fast);
        }
        break;

        case ble_gap_event_central_disconn:
        {
            /* Maybe ANCS or IAS client for which the discovery is in progress, just reset the flag */
            sinkBleGapSetDiscoveryInProgress(FALSE);
         }
         break;

        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        case ble_gap_event_local_conn_success:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_retry_encryption:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_central_conn_attempt:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateScanConnHandleEvent
    
DESCRIPTION
    Handles an event in the SCANNING/CONNECTED state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateScanConnHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

    switch (event.id)
    {
        case ble_gap_event_bondable:
        {
            gapStartCentralBonding();
        }
        break;
        
        case ble_gap_event_central_conn_attempt:
        {
            /* Stop scanning while connection is made */
            gapStopScanWhileConnecting();
            /* Attempt connection to bonded device */
            gapCentralConnectToBondedDevice(&event.args->central_conn_attempt.current_taddr,
                                         &event.args->central_conn_attempt.permanent_taddr);
        }
        break;
        
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_central_disconn:
        case ble_gap_event_no_connections:
        {
            /* Restart role switch timer */
            gapStartRoleSwitchTimer();
            /* Restart the scanning if advertising has been cancelled, or a device has disconnected */
            gapStartScanning(TRUE);
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_central)
            {
                gapSwitchToPeripheralRole(TRUE, TRUE);
            }
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapSwitchToPeripheralRole(TRUE, TRUE);
        }
        break;
        
        case ble_gap_event_set_advertising_complete:
        {
            /* Nothing to do after setting advertising data, this will be the inital setup */
        }
        break;
        
        case ble_gap_event_local_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role and disconnect from any devices */
            gapCentralPowerOff(TRUE);
        }
        break;
        
        case ble_gap_event_whitelist_timeout:
        {
            /* Switch from whitelist scanning to general scanning to look for private devices */
            gapSetWhitelistScanActive(FALSE);
            gapStopScanning();
            gapStartScanning(FALSE);
        }
        break;
        
        case ble_gap_event_fast_scan_timeout:
        {
            /* Store new scan speed */
            gapSetScanSpeed(ble_gap_scan_speed_slow);
            /* Stop current fast scan and fall back to slower scan rate */
            gapStopScanning();
            gapStartScanning(FALSE);
        }
        break;
        
        case ble_gap_event_switch_central:
        {
            /* Start new scan */
            gapStopScanning();
            gapStartScanning(TRUE);
            gapStartRoleSwitchTimer();
        }
        break;

        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        case ble_gap_event_central_conn_complete:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_fast_adv_timeout:
        case ble_gap_event_retry_encryption:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_ERROR(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateIdleCentralHandleEvent
    
DESCRIPTION
    Handles an event in the IDLE_CENTRAL state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateIdleCentralHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

    switch (event.id)
    {
        case ble_gap_event_bondable:
        {
            gapStartCentralBonding();
        }
        break;
        
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_central_disconn:
        case ble_gap_event_no_connections:
        {
            /* Restart role switch timer */
            gapStartRoleSwitchTimer();
            /* Restart the scanning if advertising has been cancelled, or a device has disconnected */
            gapStartScanning(TRUE);
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_central)
            {
                gapSwitchToPeripheralRole(TRUE, FALSE);
            }
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapSwitchToPeripheralRole(TRUE, FALSE);
        }
        break;
        
        case ble_gap_event_local_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role and disconnect from any devices */
            gapCentralPowerOff(TRUE);
        }
        break;
        
        case ble_gap_event_switch_central:
        {
            /* Start new scan */
            gapStopScanning();
            gapStartScanning(TRUE);
            gapStartRoleSwitchTimer();
        }
        break;
        
        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_set_advertising_complete:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        case ble_gap_event_central_conn_complete:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_fast_adv_timeout:
        case ble_gap_event_retry_encryption:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_central_conn_attempt:
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateIdlePeripheralHandleEvent
    
DESCRIPTION
    Handles an event in the IDLE_PERIPHERAL state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateIdlePeripheralHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateIdlePeripheralHandleEvent event.id=%d\r\n", event.id));*/

	MYDEBUG(("gapStateIdlePeripheralHandleEvent event.id=%d\r\n", event.id));
    switch (event.id)
    {
        case ble_gap_event_bondable:
        {
            gapStartPeripheralBonding();
        }
        break;

        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_peripheral)
            {
                gapSwitchToCentralRole();
            }
            else
            {
                gapAdvertisingTimeout();
            }
        }
        break;
        
        case ble_gap_event_switch_central:
        {
            gapSwitchToCentralRole();
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapSwitchToPeripheralRole(FALSE, FALSE);
            /* Read local name to set new advertising data */
            sinkBleGapStartReadLocalName(ble_gap_read_name_advertising);
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role */
            gapPeripheralPowerOff();
        }
        break;
        
        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        case ble_gap_event_central_disconn:
        case ble_gap_event_local_conn_success:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_fast_adv_timeout:
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        case ble_gap_event_set_advertising_complete:
        case ble_gap_event_retry_encryption:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_central_conn_attempt:
        case ble_gap_event_central_conn_complete:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateBondableScanningHandleEvent
    
DESCRIPTION
    Handles an event in the BONDABLE_SCANNING state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateBondableScanningHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateBondableScanningHandleEvent event.id=%d\r\n", event.id));*/

    switch (event.id)
    {
        case ble_gap_event_bondable_connection_timeout:
        {
            gapStopCentralBonding();
        }
        break;
        
        case ble_gap_event_central_conn_attempt:
        {
            /* Stop scanning while connection is made */
            gapStopScanWhileConnecting();
            /* Attempt connection to advertising device */
            gapCentralConnectToAnyDevice(&event.args->central_conn_attempt.current_taddr,
                                         &event.args->central_conn_attempt.permanent_taddr);
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_central)
            {
                gapSwitchToPeripheralRole(TRUE, TRUE);
            }
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapSwitchToPeripheralRole(TRUE, TRUE);
        }
        break;
        
        case ble_gap_event_local_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role and disconnect from any devices */
            gapCentralPowerOff(TRUE);
        }
        break;
        
        case ble_gap_event_switch_central:
        {
            /* A switch to current role exits bonding mode */
            gapStopCentralBonding();
            gapStartRoleSwitchTimer();
        }
        break;
        
        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_bondable:
        case ble_gap_event_set_advertising_complete:
        case ble_gap_event_central_disconn:
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        case ble_gap_event_central_conn_complete:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_fast_adv_timeout:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_retry_encryption:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateBondableAdvertisingHandleEvent
    
DESCRIPTION
    Handles an event in the BONDABLE_ADVERTISING state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateBondableAdvertisingHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateBondableAdvertisingHandleEvent event.id=%d\r\n", event.id));*/

    switch (event.id)
    {
        case ble_gap_event_set_advertising_complete:
        {
            /* Restart advertising after setting advertising data */
            gapStartAdvertising();
        }
        break;
        
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        {
            /* Read local name to set new advertising data */
            sinkBleGapStartReadLocalName(ble_gap_read_name_advertising);
        }
        break;
        
        case ble_gap_event_remote_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
            /* Stop advertising timer and set interval back to fast */
            gapStopFastAdvTimer();
            gapSetAdvSpeed(ble_gap_adv_speed_fast);

            /* Stop connection timeout */
            gapStopBondableConnectionTimer();
            /* Start the bondable timeout */
            gapStartBondablePairingTimer();
            /* Changing the state */
            sinkBleSetGapState(ble_gap_state_bondable_connected);
        }
        break;
        
        case ble_gap_event_remote_conn_fail:
        {
            /* Ignore advertising failure */
        }
        break;

        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        {
            gapStopPeripheralBonding();
        }
        break;
        
        case ble_gap_event_central_conn_complete:
        {
            /* A central connection has just completed. As no longer in central mode,
               need to disconnect all central connections. 
            */
            gapDisconnectFromPeripheral();
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_central)
            {
                gapSwitchToCentralRole();
            }
            else
            {
                gapAdvertisingTimeout();
            }
        }
        break;
        
        case ble_gap_event_switch_central:
        {
            gapSwitchToCentralRole();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role */
            gapPeripheralPowerOff();
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapStopPeripheralBonding();
        }
        break;

        case ble_gap_event_central_disconn:
        {
            /* Maybe ANCS or IAS client for which the discovery is in progress, just reset the flag */
            sinkBleGapSetDiscoveryInProgress(FALSE);
        }
        break;
        
        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_bondable:
        case ble_gap_event_local_conn_success:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_fast_adv_timeout:
        case ble_gap_event_retry_encryption:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_central_conn_attempt:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}

/*******************************************************************************
NAME
    gapStateBondableConnectedHandleEvent
    
DESCRIPTION
    Handles an event in the BONDABLE_CONNECTED state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateBondableConnectedHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateBondableConnectedHandleEvent event.id=%d\r\n", event.id));*/

	MYDEBUG(("gapStateBondableConnectedHandleEvent id = %d\r\n", event.id));
	
    switch (event.id)
    {
        case ble_gap_event_bondable_pairing_timeout:
        {
            /* If we are still trying to do discovery, then wait for it to complete,
                after that we shall trigger the pairing */
            if(!sinkBleGapIsDiscoveryInProgress())
            {
                if(!gapStartPeripheralEncryption(FALSE))
                {
                    gapStopPeripheralBonding();
                }
            }
            else
            {
               /* Restart pairing timer since we are still in discovery process */
               gapStartBondablePairingTimer();
            }
        }
        break;

        case ble_gap_event_retry_encryption:
        {
              if (event.args != NULL)
              {
                  gapRetryEncryption((const typed_bdaddr *)&event.args->encryption_retry.taddr);
              }
        }
        break;
        
        case ble_gap_event_pairing_complete:
        {
            /*sinkBleSetGapState(ble_gap_state_advertising_connected);*//*20160128*/

			
			/*gattDiscoverPrimaryServices( &theSink.rundata->ble.gatt.client.connection[0]);*/ /*20160128*/
			
        }
        break;

        case ble_gap_event_central_disconn:
        {
            /* Maybe ANCS or IAS client for which the discovery is in progress, just reset the flag */
            sinkBleGapSetDiscoveryInProgress(FALSE);
        }
        break;
        
        case ble_gap_event_no_connections:
        {
            /* Stop the bond timer */
            gapStopBondablePairingTimer();
            /* There was a disconnect, so change the state */
            sinkBleSetGapState(ble_gap_state_advertising_connected);
            /* Start advertising if no Central connections */
            /* Read local name to set new advertising data */
            sinkBleGapStartReadLocalName(ble_gap_read_name_advertising);
            /* Restart role switch timer if not in default role */
            gapStartRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_set_advertising_complete:
        {
            /* Restart advertising after setting advertising data */
            gapStartAdvertising();
        }
        break;
        case ble_gap_event_central_conn_complete:
        {
            /* A central connection has just completed. As no longer in central mode,
               need to disconnect all central connections. 
            */
            gapDisconnectFromPeripheral();
        }
        break;
        case ble_gap_event_switch_central:
        case ble_gap_event_switch_timeout:
        {
            gapSwitchToCentralRole();
        }
        break;

        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role */
            gapPeripheralPowerOff();
        }
        break;

        case ble_gap_event_power_on:
        case ble_gap_event_switch_peripheral:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_local_conn_success:
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_central_conn_attempt:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;

        case ble_gap_event_remote_conn_fail:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateConnectingHandleEvent
    
DESCRIPTION
    Handles an event in the CONNECTING state. Could also mean it is connected to a device, 
    but still discovering services.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateConnectingHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateConnectingHandleEvent event.id=%d\r\n", event.id));*/

    switch (event.id)
    {
        case ble_gap_event_central_conn_complete:
        {
            if (event.args != NULL)
            {
                /* Update connection parameters after successful connection */
                sinkBleSetMasterConnectionParamsUpdate(&event.args->central_conn_complete.taddr);
            }
            /* Restart scanning after connection attempt */
            gapStartScanning(TRUE);
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_central)
            {
                gapSwitchToPeripheralRole(FALSE, FALSE);
            }
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapSwitchToPeripheralRole(FALSE, FALSE);
        }
        break;
        
        case ble_gap_event_bondable: 
        {
            /* Move to bondable connecting on the bondable event */
            sinkBleSetGapState(ble_gap_state_bondable_connecting);
        }
        break;
        
        case ble_gap_event_central_disconn:
        {
            /* If at all discovery was in progresss, reset the flag */
            sinkBleGapSetDiscoveryInProgress(FALSE);
            /* A device can be connected and discovery taking place.
               Revert back to scanning on a disconnection */
            sinkBleSetGapState(ble_gap_state_scanning_connected);
            gapStartScanning(TRUE);
        }
        break;
        
        case ble_gap_event_local_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role but wait for current connection
                to complete before disconnecting */
            gapCentralPowerOff(FALSE);
        }
        break;
        
        case ble_gap_event_retry_encryption:
        {
              if (event.args != NULL)
              {
                  gapRetryEncryption((const typed_bdaddr *)&event.args->encryption_retry.taddr);
              }
        }
        break;

        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_set_advertising_complete:
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        case ble_gap_event_switch_central:
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        case ble_gap_event_central_conn_attempt:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_fast_adv_timeout:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/*******************************************************************************
NAME
    gapStateBondableConnectingHandleEvent
    
DESCRIPTION
    Handles an event in the BOND_CONN state.
    
PARAMETERS
    event   The event details.
    
RETURNS
    None
*/
static bool gapStateBondableConnectingHandleEvent(ble_gap_event_t event)
{
    /* Assume we handle the event below until proved otherwise */
    bool event_handled = TRUE;

	/*BLE_INFO(("gapStateBondableConnectingHandleEvent event.id=%d\r\n", event.id));*/

MYDEBUG(("gapStateBondableConnectingHandleEvent event.id=%d\r\n", event.id));
	
    switch (event.id)
    {
        case ble_gap_event_central_conn_complete:
        {
            /* Restart scanning after bondable connection attempt */
            gapStartScanning(TRUE);
        }
        break;
        
        case ble_gap_event_central_disconn:
        {
            /* If at all discovery was in progresss, reset the flag */
            sinkBleGapSetDiscoveryInProgress(FALSE);
            /* A device can be connected and discovery taking place.
               Revert back to bondable scanning on a disconnection */
            sinkBleSetGapState(ble_gap_state_bondable_scanning);
            gapStartScanning(TRUE);
        }
        break;
        
        case ble_gap_event_switch_timeout:
        {
            if (sinkBleGetGapDefaultRole() != ble_gap_role_central)
            {
                gapSwitchToPeripheralRole(FALSE, FALSE);
            }
        }
        break;

        case ble_gap_event_retry_encryption:
        {
              if (event.args != NULL)
              {
                  gapRetryEncryption((const typed_bdaddr *)&event.args->encryption_retry.taddr);
              }
        }
        break;
        
        case ble_gap_event_switch_peripheral:
        {
            gapSwitchToPeripheralRole(FALSE, FALSE);
        }
        break;
        
        case ble_gap_event_local_conn_success:
        {
            /* Stop GAP role switch timer when a successful connection is made */
            gapStopRoleSwitchTimer();
        }
        break;
        
        case ble_gap_event_power_off:
        {
            /* Power off in Peripheral role but wait for current connection
                to complete before disconnecting */
            gapCentralPowerOff(FALSE);
        }
        break;
        
        case ble_gap_event_switch_central:
        {
             /* A switch to current role exits bonding mode */
             sinkBleSetGapState(ble_gap_state_connecting);
             gapStartRoleSwitchTimer();
        }
        break;
        
        /* Events okay to ignore */
        case ble_gap_event_power_on:
        case ble_gap_event_bondable:
        case ble_gap_event_set_advertising_complete:
        case ble_gap_event_remote_conn_success:
        case ble_gap_event_remote_conn_fail:
        case ble_gap_event_cancel_advertising:
        case ble_gap_event_no_connections:
        case ble_gap_event_central_conn_attempt:
        case ble_gap_event_whitelist_timeout:
        case ble_gap_event_fast_scan_timeout:
        case ble_gap_event_pairing_complete:
        case ble_gap_event_fast_adv_timeout:
        {
            BLE_GAP_INFO(("GAP event ignored in state\n"));
        }
        break;
        
        /* Events not expected or handled */
        case ble_gap_event_bondable_connection_timeout:
        case ble_gap_event_bondable_pairing_timeout:
        default:
        {
            event_handled = FALSE;
            BLE_GAP_INFO(("GAP event not handled in state\n"));
        }
        break;
    }
    
    return event_handled;
}


/******************************************************************************/
void sinkBleGapInitialise(ble_gap_role_t role)
{
	/*BLE_INFO(("sinkBleGapInitialise role=%d\r\n", role));*/
	
    /* Set initial GAP state */
    sinkBleSetGapState(ble_gap_state_off);
    
    /* Set default role */
    sinkBleSetGapDefaultRole(role);

    /* Set if initially bonded to private device */
    sinkBleGapSetBondedToPrivacyDevice(ConnectionBondedToPrivacyEnabledDevice());
    
    /* Set initial advertising speed */
    gapSetAdvSpeed(ble_gap_adv_speed_fast);
}


/******************************************************************************/
void sinkBleGapEvent(ble_gap_event_t event)
{
    /* Indication if event handled by current state */
    bool event_handled = FALSE;
    ble_gap_state_t state = sinkBleGetGapState();

    /*BLE_GAP_INFO(("GAP new event=[%s] state=[%s]\n", gap_events[event.id], gap_states[state]));*/
    /*MYDEBUG(("GAP event=[%d] state=[%d]\n", event.id, state));*/
	
    switch (state)
    {
        case ble_gap_state_off:
        {
            event_handled = gapStateOffHandleEvent(event);
        }
        break;
        case ble_gap_state_advertising_connected:
        {
            event_handled = gapStateAdvConnHandleEvent(event);
        }
        break;
        case ble_gap_state_scanning_connected:
        {
            event_handled = gapStateScanConnHandleEvent(event);
        }
        break;
        case ble_gap_state_idle_central:
        {
            event_handled = gapStateIdleCentralHandleEvent(event);
        }
        break;
        case ble_gap_state_idle_peripheral:
        {
            event_handled = gapStateIdlePeripheralHandleEvent(event);
        }
        break;
        case ble_gap_state_bondable_scanning:
        {
            event_handled = gapStateBondableScanningHandleEvent(event);
        }
        break;
        case ble_gap_state_bondable_advertising:
        {
            event_handled = gapStateBondableAdvertisingHandleEvent(event);
        }
        break;
        case ble_gap_state_connecting:
        {
            event_handled = gapStateConnectingHandleEvent(event);
        }
        break;
        case ble_gap_state_bondable_connecting:
        {
            event_handled = gapStateBondableConnectingHandleEvent(event);
        }
        break;
        case ble_gap_state_bondable_connected:
        {
            event_handled = gapStateBondableConnectedHandleEvent(event);
        }
        break;
        default:
        {
            BLE_GAP_ERROR(("GAP event in unknown state\n"));
        }
        break;
    }
    
    if (!event_handled)
    {
        BLE_GAP_ERROR(("Check behaviour of GAP event=[%s] in state=[%s]!\n", gap_events[event.id], gap_states[state]));
    }
}


/******************************************************************************/
void sinkBleGapStartReadLocalName(ble_gap_read_name_t reason)
{
    if (!(GAP.name_read & reason))
    {
        GAP.name_read |= reason;
    
        ConnectionReadLocalName(sinkGetBleTask());
    }
}


/******************************************************************************/
void sinkBleGapReadLocalNameComplete(CL_DM_LOCAL_NAME_COMPLETE_T * cfm)
{      
    BLE_GAP_INFO(("CL_DM_LOCAL_NAME_COMPLETE\n"));
    
    if (cfm->status != success)
    {
        BLE_GAP_ERROR(("  Failed!\n"));
    }
    
    if (GAP.name_read & ble_gap_read_name_gap_server)
    {
        /* Use local name to respond as GAP server */
        BLE_GAP_INFO(("    Gap server response\n"));
        sinkGattGapServerSendLocalNameResponse(cfm);
    }
    if (GAP.name_read & ble_gap_read_name_advertising)
    {
        /* Use local name to setup advertising data */
        BLE_GAP_INFO(("    Set advertising data bondable=[%u]\n", sinkBleGetGapState() == ble_gap_state_bondable_advertising ? TRUE : FALSE));
        bleSetupAdvertisingData(cfm->size_local_name, 
                                cfm->local_name, 
                                sinkBleGetGapState() == ble_gap_state_bondable_advertising ? adv_discoverable_mode_limited : adv_discoverable_mode_general);
    }
    
    GAP.name_read = 0;
}


/******************************************************************************/
void sinkBleGapSetSecurityCfm(CL_DM_BLE_SECURITY_CFM_T * cfm)
{
    gatt_client_connection_t *connection = NULL;
    ble_gap_role_t connection_role = ble_gap_role_unknown;
    uint16 cid = GattGetCidForBdaddr(&cfm->taddr);
    connection = gattClientFindByCid(cid);
    
    BLE_GAP_INFO(("CL_DM_BLE_SECURITY_CFM status=[0x%x]\n", cfm->status));
    
    if(connection != NULL)
    {
        connection_role = connection->role;

        if (cfm->status == ble_security_success)
        {
            /* Add device to whitelist */
            ConnectionDmBleAddDeviceToWhiteListReq(cfm->taddr.type, 
                                                    &cfm->taddr.addr);
            gattClientInitialiseDiscoveredServices(connection);
            /* if this success is for tagged security failure device then untag the device */
            gapClearSecurityFailedDevice(&cfm->taddr);

            /* Send Connection Parameters Update Request now instead of
               just after connection is established.
               This is needed to avoid sending this request too early
               i.e. in the first connection event, which seems to cause
               issues while pairing with iOS devices.
             */
            if(connection_role == ble_gap_role_peripheral)
            {
                sinkBleSetSlaveConnectionParamsUpdate(&cfm->taddr);
            }
        }
        else if(cfm->status == ble_security_pairing_in_progress)
        {
            /* LE pairing is inprogress re schedule encryption */
            sinkBleRetryEncryptionEvent(cid);
        }
        else
        {
            /* If encryption failed due to link key missing tag the device */
            if(cfm->status == ble_security_link_key_missing)
                gapSetSecurityFailedDevice(&cfm->taddr);

             /* Remove the connection */
             gapProcessEncryptionFailure(connection);

            /* Restart scanning if in central mode */
            if(connection_role == ble_gap_role_central)
            {
                /* Connection is now complete after setting security */
                sinkBleCentralConnCompleteEvent(cid);
            }
        }
     }

    sinkBlePairingCompleteEvent ();
    gapStopBondablePairingTimer();

}

/******************************************************************************/
void sinkBleGapAddDeviceWhiteListCfm(CL_DM_BLE_ADD_DEVICE_TO_WHITE_LIST_CFM_T * cfm)
{
    if ((cfm->status==success) && (sinkBleGapGetBondedToPrivacyDevice()!=TRUE))
    {
        /* Set bonded with a privacy enabled device*/
        sinkBleGapSetBondedToPrivacyDevice(ConnectionBondedToPrivacyEnabledDevice());
    }
}

/******************************************************************************/
bool sinkBleGapIsBondable(void)
{
    ble_gap_state_t state = sinkBleGetGapState();
    bool bondable = FALSE;


	
    switch (state)
    {
        case ble_gap_state_bondable_advertising:
        case ble_gap_state_bondable_scanning:
        case ble_gap_state_bondable_connecting:
        case ble_gap_state_bondable_connected:
            bondable = TRUE;
            break;
         default:
            bondable = FALSE;
            break;
    }

	/*BLE_INFO(("sinkBleGapIsBondable=[%d]\n", bondable));*/

	return bondable;
}



/******************************************************************************/
void sinkBleSetGapDefaultRole(ble_gap_role_t role)
{
    GAP.default_role = role;

	/*BLE_INFO(("GAP Default Role=[%u]\n", role));*/
    BLE_GAP_INFO(("GAP Default Role=[%u]\n", role));
}


/******************************************************************************/
ble_gap_role_t sinkBleGetGapDefaultRole(void)
{
    return GAP.default_role;
}


/******************************************************************************/
bool sinkBleGapIsPersistentRole(void)
{
    if (sinkBleGetConfiguration()->gap_role_switch_timer_s == 0)
        return TRUE;
    
    return FALSE;
}


/******************************************************************************/
bool sinkBleGapIsRoleUnknown(ble_gap_role_t role)
{
    switch (role)
    {
        case ble_gap_role_central:
        case ble_gap_role_peripheral:
            return FALSE;
            
        case ble_gap_role_unknown:
        default:
            return TRUE;
    }
    
    return TRUE;
}

/*******************************************************************************/
void sinkBleGapEmptyWhiteList(void)
{
    BLE_GAP_INFO(("BLE : Clear the BLE whitelist\n"));

    if (sinkBleGetGapState() != ble_gap_state_off)
    {
        /*Stop timers*/
        gapStopRoleSwitchTimer();
        gapStopBondableConnectionTimer();
        gapStopBondablePairingTimer();
        /* clear the marked device */
        BdaddrTypedSetEmpty(&GAP.security_failed_device);

        /* Disconnect current connections */
        gattClientDisconnectAll();
        gattServerDisconnectAll();
        
        /*In case nothing to be disconnected, send NoConnectionsEvent to start scanning/advertising in respective state*/
        sinkBleCheckNoConnectionsEvent();
        
        /*If the sink device is scanning, it must be turned off before clearing the whitelist*/
        gapStopScanning();

        /*If the sink device is advertising, it must be turned off before clearing the whitelist*/
        gapStopAdvertising(ble_gap_adv_speed_fast);
        
        /* It's now safe to clear the BLE white list */
        ConnectionDmBleClearWhiteListReq();

        /*Switch to appropriate role*/
        /*Clearing whitelist would be similar to power on, hence check based on default state can be done*/
        if (sinkBleGetGapDefaultRole() == ble_gap_role_central)
        {
            sinkBleSetGapState(ble_gap_state_scanning_connected);
        }
        else if (sinkBleGetGapDefaultRole() == ble_gap_role_peripheral)
        {
            sinkBleSetGapState(ble_gap_state_advertising_connected);
        } 
    }
    else
    {
        ConnectionDmBleClearWhiteListReq();
    }

    /* Reset bonded with a privacy enabled device*/
    sinkBleGapSetBondedToPrivacyDevice(FALSE);
}

#endif /* GATT_ENABLED */

