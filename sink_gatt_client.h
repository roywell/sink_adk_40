/*******************************************************************************
 Copyright Cambridge Silicon Radio Limited 2015
 Part of ADK 4.0
*******************************************************************************/



#ifndef _SINK_GATT_CLIENT_H_
#define _SINK_GATT_CLIENT_H_

#include "sink_ble_gap.h"
#include "sink_gatt_hid_remote_control.h"

#include <gatt_client.h>
#include <gatt_battery_client.h>
#include <gatt_apple_notification_client.h>
#include <gatt_imm_alert_client.h>
#include <gatt_hid_client.h>
#include <gatt_device_info_client.h>
#include <gatt_scan_params_client.h>
#include <gatt_gap_server.h>
#include <bdaddr.h>
#include <csrtypes.h>


/* GATT Client Configuration attributes that need to be stored per client */
typedef struct __gatt_client_attributes_t
{
    uint16 gatt;
    uint16 battery_local;
    uint16 battery_remote;
    uint16 battery_peer;
} gatt_client_attributes_t;


#ifdef GATT_ENABLED


/* Define for Client variables */
#define GATT_CLIENT theSink.rundata->ble.gatt.client


#define MAX_GATT_CLIENT_CONNECTIONS 3


/*
    Defines the bitfield used to identify which client tasks are used for the device.
*/
typedef enum
{
    gatt_client_battery   = 1,
    gatt_client_ancs = 2,
    gatt_client_ias = 3,
    gatt_client_hid = 4,
    gatt_client_dis = 5,
    gatt_client_gatt = 6,
    gatt_client_spc = 7
    /* Add more client services here */
    
} gatt_client_tasks_t;
#define MAX_GATT_CLIENT_SERVICES   7 /* Increase as number of gatt_client_tasks_t increases */


/* Cache of remote values */
typedef struct __gatt_cache_t
{
    BITFIELD basc_remote_level:8;
    BITFIELD iasc_alert_level:8;
} gatt_cache_t;

/* Connection data maintained for each of the connected device irrespective of a 
   client/server connection.The information specific to a connected device is stored here.
*/
typedef enum __gatt_attribute_service_t
{
    gatt_attr_service_battery_local,
    gatt_attr_service_battery_remote,
    gatt_attr_service_battery_peer,
    gatt_attr_service_gatt,
    gatt_attr_service_all
} gatt_attribute_service_t;

typedef struct __gatt_server_read_name_t
{
    bool        requested;  /* Flag to indicate if client requested read of local name */
    uint16      offset;     /* Offset for local name repsonse */   
} gatt_client_gap_read_name_t;

typedef struct __gatt_client_connection
{
    void *data;                                 /* Connection specific data */
    uint16 cid;                                 /* The ID of the client connection */
    gatt_client_attributes_t client_config;     /* Client Service Configuration set by by the client */
    gatt_client_gap_read_name_t gap_read_name;  /* Records the clients read of the local name using GAP */
    BITFIELD role:2;                            /* Records the GAP role for the client connection */
    BITFIELD service_changed:1;                 /* Records if the service definitions have changed  */
} gatt_client_connection_t;

/* Discovery data */
typedef struct __gatt_client_discovery
{
    gatt_client_tasks_t service;
    uint16 start_handle;
    uint16 end_handle;
} gatt_client_discovery_t;

/* List of remote services */
typedef struct __gatt_client_services
{    
    gatt_client_discovery_t *discovery;
    gattHidRcDevData_t *remote;
    BITFIELD number_discovered_services:8;
    BITFIELD current_discovered_service:8;
    GBASC *basc;
    GIASC_T *iasc;
    /* Insert more device service pointers here */
    GANCS *ancs;
    GHIDC_T *hidc_instance1;
    GHIDC_T *hidc_instance2;
    GDISC    *disc;
    GGATTC *gattc;
    GSPC_T *spc;
    uint16 size_client_data;
    uint16 client_data[1];
} gatt_client_services_t;

/* Client data */
typedef struct __gatt_client
{
    gatt_client_connection_t connection[MAX_GATT_CLIENT_CONNECTIONS];
    uint16 number_connections;
    gatt_cache_t cache;
} gatt_client_t;


/****************************************************************************
NAME    
    gattClientAdd
    
DESCRIPTION
    Stores connection details when a remote server device connects.
    
PARAMETERS
    cid             The connection ID
    client_taddr    Pointer to the connecting LE device
    client_gap_role GAP Role for the client connection
    
RETURNS    
    TRUE if the connection was successfully added, FALSE otherwise.
*/
bool gattClientAdd(uint16 cid, const typed_bdaddr *client_taddr, ble_gap_role_t client_gap_role);


/****************************************************************************
NAME    
    gattClientFindByCid
    
DESCRIPTION
    Finds the GATT client connection by connection ID.
    
PARAMETERS
    cid             The connection ID
    
RETURNS    
    The GATT connection instance if found, NULL otherwise.
*/
gatt_client_connection_t *gattClientFindByCid(uint16 cid);


/****************************************************************************
NAME    
    gattClientRemove
    
DESCRIPTION
    Frees resource for a GATT client connection.
    
PARAMETERS
    cid             The connection ID

RETURNS    
    TRUE if the connection was found and removed. FALSE otherwise.
*/
bool gattClientRemove(uint16 cid);


/*******************************************************************************
NAME
    gattClientAddService
    
DESCRIPTION
    Allocates memory that can be used for a new client GATT service, for the specified connection.
    
PARAMETERS
    connection      The GATT client connection instance.
    size_service    The size of the service to be added.
    
RETURNS
    Pointer to the memory that can be used by the new service if successsful.
    Will return NULL if the service can't be added. 
*/
uint16 *gattClientAddService(gatt_client_connection_t *connection, uint16 size_service);


/*******************************************************************************
NAME
    gattClientStoreDiscoveredService
    
DESCRIPTION
    Store services discovered during the "Discover All Primary Services" procedure.
    
PARAMETERS
    cid             The connection ID.
    uuid_type    To indicate if UUID is 16/32/64/128 bit
    uuid            Pointer to 16 bit/128 UUID of the discovered service.
    start           The start handle of the service.
    end             The end handle of the service.
    more            Indicates if discovery is still in progress.
    
*/
void gattClientStoreDiscoveredService(uint16 cid, uint16 uuid_type, uint32 *uuid, uint16 start, uint16 end, bool more);


/*******************************************************************************
NAME
    gattClientGetServiceData
    
DESCRIPTION
    Returns the service data for the specified connection.
    
PARAMETERS
    connection      The GATT client connection instance.
    
RETURNS
    Pointer to the service data for the client connection. 
*/
gatt_client_services_t *gattClientGetServiceData(gatt_client_connection_t *connection);


/****************************************************************************
NAME    
    gattClientDiscoveredServiceInitialised
    
DESCRIPTION
    Called when a primary service has been fully initialised.
    The next primary service can then be initialised, or the procedure ends.
    
PARAMETERS
    connection             The GATT connection

*/
void gattClientDiscoveredServiceInitialised(gatt_client_connection_t *connection);

/****************************************************************************
NAME    
    gattClientRemoveDiscoveredService
    
DESCRIPTION
    Called when a discovered service library failed to initialize.
    
PARAMETERS
    connection             The GATT connection
    service                  The client service that needs to be de-removed

 NOTE: DO NOT call this function other than the case where client library init fails. If called in
other situation then the entire client_data shall be corrupted.

*/
bool gattClientRemoveDiscoveredService(gatt_client_connection_t *connection, gatt_client_tasks_t service);

/****************************************************************************
NAME    
    gattClientInitialiseDiscoveredServices
    
DESCRIPTION
    Initialise known primary services after being discovered.
    
PARAMETERS
    connection             The GATT connection

*/
void gattClientInitialiseDiscoveredServices(gatt_client_connection_t *connection);

/****************************************************************************
NAME    
    gattDiscoverPrimaryServices
    
DESCRIPTION
    Discovers GATT primary services supported on the remote device.
    
PARAMETERS
    connection             The GATT connection

*/
void gattDiscoverPrimaryServices(gatt_client_connection_t *connection);

/****************************************************************************
NAME    
    gattClientGetDiscoveredServices
    
DESCRIPTION
    Gets a list of the discovered primary services.
    
PARAMETERS
    connection                  The GATT connection
    number_discovered_services  Set by the function to the current number of discovered services

*/
gatt_client_discovery_t *gattClientGetDiscoveredServices(gatt_client_connection_t *connection);

#endif /* GATT_ENABLED */


/****************************************************************************
NAME    
    gattClientDisconnect
    
DESCRIPTION
    Disconnects the client role connection identified by the CID. 
    
PARAMETERS
    cid                     The connection ID
    
RETURNS    
    None
*/
#ifdef GATT_ENABLED
void gattClientDisconnect(uint16 cid);
#else
#define gattClientDisconnect(cid) ((void)(0))
#endif


/****************************************************************************
NAME    
    gattClientDisconnectAll
    
DESCRIPTION
    Disconnects all client role connections.
    
PARAMETERS
    None
    
RETURNS    
    TRUE if a disconnect request was sent. FALSE otherwise.
*/
#ifdef GATT_ENABLED
bool gattClientDisconnectAll(void);
#else
#define gattClientDisconnectAll() (FALSE)
#endif


/****************************************************************************
NAME    
    gattClientDiscoveryError
    
DESCRIPTION
    Capture discovery errors.
    
PARAMETERS
    connection      The GATT connection

*/
#ifdef GATT_ENABLED
void gattClientDiscoveryError(gatt_client_connection_t *connection);
#else
#define gattClientDiscoveryError(connection) ((void)(0))
#endif


/****************************************************************************
NAME    
    gattClientHasMaxCentralClients
    
DESCRIPTION
    Checks if the maximum central device connections has been reached.
    
PARAMETERS
    None
    
RETURNS
    TRUE if reached the maximum connections. FALSE otherwise.

*/
#ifdef GATT_ENABLED
bool gattClientHasMaxCentralClients(void);
#else
#define gattClientHasMaxCentralClients() (TRUE)
#endif

/****************************************************************************
NAME    
    gattClientHasNoClients
    
DESCRIPTION
    Gets if there are no connections active.
    
PARAMETERS
    connection      The GATT connection
    
RETURNS
    TRUE if no connections. FALSE otherwise.

*/
#ifdef GATT_ENABLED
bool gattClientHasNoClients(void);
#else
#define gattClientHasNoClients() (TRUE)
#endif


/****************************************************************************
NAME    
    gattClientRemoveServices
    
DESCRIPTION
    Remove all services associated with the supplied connection.
    
PARAMETERS
    None
    
RETURNS
    None

*/
#ifdef GATT_ENABLED
void gattClientRemoveServices(gatt_client_connection_t *client_connection);
#else
#define gattClientRemoveServices(client_connection) ((void)(0))
#endif


/****************************************************************************
NAME    
    gattClientServiceChanged
    
DESCRIPTION
    Notifies the GATT client that a Service Changed indication has been received.
    
PARAMETERS
    None
    
RETURNS
    None

*/
#ifdef GATT_ENABLED
void gattClientServiceChanged(uint16 cid);
#else
#define gattClientServiceChanged(cid) ((void)(0))
#endif


/****************************************************************************
NAME    
    gattClientRediscoverServices
    
DESCRIPTION
    Called to rediscover all services on the supplied connection.
    
PARAMETERS
    connection  The client connection

*/
#ifdef GATT_ENABLED
void gattClientRediscoverServices(uint16 cid);
#else
#define gattClientRediscoverServices(cid) ((void)(0))
#endif


/****************************************************************************
NAME    
    gattClientProcessSecurityRequirements
    
DESCRIPTION
    Process security requirement for this client connection.
    
PARAMETERS
    connection pointer to this client connection
    data pointer to client services

*/
#ifdef GATT_ENABLED
void gattClientProcessSecurityRequirements(gatt_client_connection_t *connection, gatt_client_services_t *data);
#else
#define gattClientProcessSecurityRequirements(connection, data) ((void)(0))
#endif    


/****************************************************************************
NAME    
    gattClientStoreConfigAttributes
    
DESCRIPTION
    Stores Client Characteristic Configuration attributes for bonded devices.
    
PARAMETERS
    cid The connection ID.
    client_service The service to store attributes for.

*/
#ifdef GATT_ENABLED
void gattClientStoreConfigAttributes(uint16 cid, gatt_attribute_service_t client_service);
#else
#define gattClientStoreConfigAttributes(cid, client_service) ((void)(0))
#endif


/****************************************************************************
NAME    
    gattClientGapNameReadRequested
    
DESCRIPTION
    When a client has requested a read of the device name using the GAP server, this function will be called.
    The server is expected to respond to this request.
    
PARAMETERS
    cid The connection ID.
    offset The device name offset to use in the response.

*/
#ifdef GATT_ENABLED
bool gattClientGapNameReadRequested(uint16 cid, uint16 offset);
#else
#define gattClientGapNameReadRequested(cid, offset) (FALSE)
#endif


/****************************************************************************
NAME    
    gattClientGapNameReadResponse
    
DESCRIPTION
    When a client has requested a read of the device name using the GAP server, this function
    is used to send the response.
    
PARAMETERS
    gap_server The local GAP server
    size_local_name The size of the device name sent in the response
    local_name The device name sent in the response

*/
#ifdef GATT_ENABLED
void gattClientGapNameReadResponse(GGAPS *gap_server, uint16 size_local_name, uint8 *local_name);
#else
#define gattClientGapNameReadResponse(gap_server, size_local_name, local_name) ((void)(0))
#endif


#endif /* _SINK_GATT_CLIENT_H_ */

