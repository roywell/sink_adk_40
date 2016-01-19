/****************************************************************************
Copyright Cambridge Silicon Radio Limited 2014 - 2015.
Part of ADK 4.0

FILE NAME
    sink_ble_gap.h

DESCRIPTION
    BLE GAP functionality
*/

#ifndef _SINK_BLE_GAP_H_
#define _SINK_BLE_GAP_H_


#include <connection.h>

#include <csrtypes.h>


#define GAP theSink.rundata->ble.gap


/* GAP events IDs */
typedef enum __ble_gap_event_id
{
    ble_gap_event_power_on,                 /* 0 */
    ble_gap_event_bondable,                 /* 1 */
    ble_gap_event_set_advertising_complete, /* 2 */
    ble_gap_event_bondable_pairing_timeout,         /* 3 */
    ble_gap_event_central_conn_attempt,     /* 4 */
    ble_gap_event_central_conn_complete,    /* 5 */
    ble_gap_event_central_disconn,          /* 6 */
    ble_gap_event_local_conn_success,       /* 7 */
    ble_gap_event_switch_central,           /* 8 */
    ble_gap_event_switch_peripheral,        /* 9 */
    ble_gap_event_remote_conn_success,      /* 10 */
    ble_gap_event_remote_conn_fail,         /* 11 */
    ble_gap_event_pairing_complete, /* 12 */
    ble_gap_event_switch_timeout,           /* 13 */
    ble_gap_event_cancel_advertising,       /* 14 */
    ble_gap_event_no_connections,           /* 15 */
    ble_gap_event_power_off,                /* 16 */
    ble_gap_event_whitelist_timeout,        /* 17 */
    ble_gap_event_fast_scan_timeout,        /* 18 */
    ble_gap_event_bondable_connection_timeout,        /* 19 */
    ble_gap_event_fast_adv_timeout,         /* 20 */
    ble_gap_event_retry_encryption,         /* 21 */
    /* Update gap_events[] array if adding new item */
    ble_gap_event_last                      /* Always leave as last item in enum */ 
} ble_gap_event_id_t;

/* GAP event args for ble_gap_event_central_conn_attempt */
typedef struct __ble_gap_args_central_conn_attempt
{
    typed_bdaddr current_taddr;
    typed_bdaddr permanent_taddr;
} ble_gap_args_central_conn_attempt_t;

/* GAP event args for ble_gap_event_central_conn_complete */
typedef struct __ble_gap_args_central_conn_complete
{
    typed_bdaddr taddr;
} ble_gap_args_central_conn_complete_t;

/* GAP event args for ble_gap_event_remote_conn_success */
typedef struct __ble_gap_args_remote_conn_success
{
    typed_bdaddr taddr;
} ble_gap_args_remote_conn_success_t;

/* GAP event args for ble_gap_event_retry_encryption */
typedef struct __ble_gap_args_retry_encryption
{
    typed_bdaddr taddr;
} ble_gap_args_retry_encryption_t;

/* Union of args used for GAP events */
typedef union __ble_gap_event_args
{
    ble_gap_args_central_conn_attempt_t central_conn_attempt;
    ble_gap_args_central_conn_complete_t central_conn_complete;
    ble_gap_args_remote_conn_success_t remote_conn_success;
    ble_gap_args_retry_encryption_t encryption_retry;
} ble_gap_event_args_t;

/* GAP event structure */
typedef struct __ble_gap_event
{
    ble_gap_event_id_t id;
    ble_gap_event_args_t *args;
} ble_gap_event_t;

/* GAP states */
typedef enum __ble_gap_state
{
    ble_gap_state_off,                      /* 0 */
    ble_gap_state_advertising_connected,    /* 1 */
    ble_gap_state_scanning_connected,       /* 2 */
    ble_gap_state_idle_peripheral,          /* 3 */
    ble_gap_state_idle_central,             /* 4 */
    ble_gap_state_bondable_advertising,     /* 5 */
    ble_gap_state_bondable_scanning,        /* 6 */
    ble_gap_state_connecting,               /* 7 */
    ble_gap_state_bondable_connecting,      /* 8 */
    ble_gap_state_bondable_connected, /* 9 */
    /* Update gap_states[] array if adding new item */
    ble_gap_state_last                      /* Always leave as last item in enum */
} ble_gap_state_t;

/* GAP roles */
typedef enum __ble_gap_role
{
    ble_gap_role_unknown,
    ble_gap_role_central,
    ble_gap_role_peripheral
} ble_gap_role_t;

/* Read name reason types (used for bitmask) */
typedef enum __ble_gap_read_name
{
    ble_gap_read_name_advertising = 1,
    ble_gap_read_name_gap_server = 2
} ble_gap_read_name_t;

/* Scan speed */
typedef enum __ble_gap_scan_speed
{
    ble_gap_scan_speed_fast,
    ble_gap_scan_speed_slow
} ble_gap_scan_speed_t;

/* GAP scan state structure */
typedef struct __ble_gap_scan_state
{
    ble_gap_scan_speed_t speed;             /* Stores current scan speed */
    unsigned whitelist_active:1;            /* Flag used to indicate if using whitelist */
} ble_gap_scan_state_t;

/* Advertising speed */
typedef enum __ble_gap_adv_speed
{
    ble_gap_adv_speed_fast,
    ble_gap_adv_speed_slow
} ble_gap_adv_speed_t;

/* GAP advertising state structure */
typedef struct __ble_gap_adv_state
{
    ble_gap_adv_speed_t speed;              /* Stores current advertising speed */
} ble_gap_adv_state_t;

/* GAP variables */
typedef struct __ble_gap
{
    ble_gap_state_t state;                  /* The current GAP state */
    ble_gap_role_t default_role;            /* The default GAP role of the application */
    ble_gap_read_name_t name_read;          /* Used to store why the local name is being read */
    typed_bdaddr  security_failed_device;   /* The remote device marked for deletion due to security failure */
    unsigned bonded_to_private_device:1;    /* Flag used to indicate if bonded to private device */
    unsigned service_discovery_inprogress:1;/* Flag used to indicated if there is client primary discovery in progress */
    ble_gap_scan_state_t scan;              /* Used to store current scan state */
    ble_gap_adv_state_t adv;                /* Used to store current advertising state */
} ble_gap_t;


/*******************************************************************************
NAME
    sinkBleGapInitialise
    
DESCRIPTION
    Initialises the GAP module.
    
PARAMETERS
    None
    
RETURNS
    None
*/
#ifdef GATT_ENABLED
void sinkBleGapInitialise(ble_gap_role_t role);
#else
#define sinkBleGapInitialise(role) ((void)(0))
#endif


/*******************************************************************************
NAME
    sinkBleGapEvent
    
DESCRIPTION
    Initiates a GAP event.
    
PARAMETERS
    event   The GAP event
    
RETURNS
    TRUE if the event was handled. FALSE otherwise.
*/
#ifdef GATT_ENABLED
void sinkBleGapEvent(ble_gap_event_t event);
#else
#define sinkBleGapEvent(event) ((void)(0))
#endif


/*******************************************************************************
NAME
    sinkBleGapStartReadLocalName
    
DESCRIPTION
    Read local name for BLE operations.
    
PARAMETERS
    None
    
RETURNS
    None
*/
#ifdef GATT_ENABLED
void sinkBleGapStartReadLocalName(ble_gap_read_name_t reason);
#else
#define sinkBleGapStartReadLocalName(reason) ((void)(0))
#endif


/*******************************************************************************
NAME
    sinkBleGapReadLocalNameComplete
    
DESCRIPTION
    Read local name complete for BLE operations.
    
PARAMETERS
    None
    
RETURNS
    None
*/
#ifdef GATT_ENABLED
void sinkBleGapReadLocalNameComplete(CL_DM_LOCAL_NAME_COMPLETE_T * cfm);
#else
#define sinkBleGapReadLocalNameComplete(cfm) ((void)(0))
#endif


/*******************************************************************************
NAME
    sinkBleGapSetSecurityCfm
    
DESCRIPTION
    Set security confirm for BLE operations.
    
PARAMETERS
    None
    
RETURNS
    None
*/
#ifdef GATT_ENABLED
void sinkBleGapSetSecurityCfm(CL_DM_BLE_SECURITY_CFM_T * cfm);
#else
#define sinkBleGapSetSecurityCfm(cfm) ((void)(0))
#endif

/*******************************************************************************
NAME
    sinkBleGapAddDeviceWhiteListCfm
    
DESCRIPTION
    Handle Add Device whitelist cfm. 
    Added to check for privacy devices been bonded.
    
PARAMETERS
    None
    
RETURNS
    None
*/
#ifdef GATT_ENABLED
void sinkBleGapAddDeviceWhiteListCfm(CL_DM_BLE_ADD_DEVICE_TO_WHITE_LIST_CFM_T * cfm);
#else
#define sinkBleGapAddDeviceWhiteListCfm(cfm) ((void)(0))
#endif




/*******************************************************************************
NAME
    sinkBleGapIsBondable
    
DESCRIPTION
    Finds if BLE is in bondable mode.
    
PARAMETERS
    None
    
RETURNS
    TRUE if BLE is in bondable mode. FALSE otherwise.
*/
#ifdef GATT_ENABLED
bool sinkBleGapIsBondable(void);
#else
#define sinkBleGapIsBondable() (FALSE)
#endif


/*******************************************************************************
NAME
    sinkBleSetGapDefaultRole
    
DESCRIPTION
    Sets the default GAP role for the application.
    
PARAMETERS
    role    The GAP role
    
RETURNS
    None
*/
#ifdef GATT_ENABLED
void sinkBleSetGapDefaultRole(ble_gap_role_t role);
#else
#define sinkBleSetGapDefaultRole(role) ((void)(0))
#endif


/*******************************************************************************
NAME
    sinkBleGetGapDefaultRole
    
DESCRIPTION
    Gets the default GAP role for the application.
    
PARAMETERS
    None
    
RETURNS
    The default GAP Role.
*/
#ifdef GATT_ENABLED
ble_gap_role_t sinkBleGetGapDefaultRole(void);
#else
#define sinkBleGetGapDefaultRole() (ble_gap_role_unknown)
#endif


/*******************************************************************************
NAME
    sinkBleGapIsPersistentRole
    
DESCRIPTION
    Returns if persistent BLE role is enabled.
    
PARAMETERS
    None
    
RETURNS
    TRUE if persistent BLE role is enabled. FALSE otherwise.
*/
#ifdef GATT_ENABLED
bool sinkBleGapIsPersistentRole(void);
#else
#define sinkBleGapIsPersistentRole() (FALSE)
#endif


/*******************************************************************************
NAME
    sinkBleGapIsRoleUnknown
    
DESCRIPTION
    Returns if BLE role is unknown/invalid.
    
PARAMETERS
    None
    
RETURNS
    TRUE if persistent BLE role is unknown/invalid. FALSE otherwise.
*/
#ifdef GATT_ENABLED
bool sinkBleGapIsRoleUnknown(ble_gap_role_t role);
#else
#define sinkBleGapIsRoleUnknown(role) (TRUE)
#endif


/****************************************************************************
NAME    
    sinkBleGapEmptyWhiteList
    
DESCRIPTION
    Function to ensure the BLE whitelist will be cleared
*/
#ifdef GATT_ENABLED
void sinkBleGapEmptyWhiteList(void);
#else
#define sinkBleGapEmptyWhiteList() ((void)(0))
#endif


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
#ifdef GATT_ENABLED
void sinkBleGapSetDiscoveryInProgress(bool in_progress);
#else
#define sinkBleGapSetDiscoveryInProgress(in_progress)
#endif


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
#ifdef GATT_ENABLED
bool sinkBleGapIsDiscoveryInProgress(void);
#else
#define sinkBleGapIsDiscoveryInProgress(void)
#endif

#endif /* _SINK_BLE_GAP_H_ */
