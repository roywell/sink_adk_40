/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.

FILE NAME
    sink_gatt_client.c

DESCRIPTION
    Routines to handle the GATT Clients.
*/

#include <vm.h>

#include "sink_gatt_client.h"

#include "sink_gatt.h"
#include "sink_ble.h"
#include "sink_debug.h"
#include "sink_gatt_client_battery.h"
#include "sink_gatt_client_ancs.h"
#include "sink_gatt_client_ias.h"
#include "sink_gatt_client_hid.h"
#include "sink_gatt_client_dis.h"
#include "sink_gatt_client_gatt.h"
#include "sink_gatt_client_spc.h"
#include "sink_gatt_device.h"
#include "sink_private.h"
#include "sink_gatt_server_battery.h"

#include <gatt_manager.h>

#include <message.h>


#ifdef GATT_ENABLED

#ifdef DEBUG_GATT_CLIENT
#define GATT_CLIENT_DEBUG(x) DEBUG(x)
#else
#define GATT_CLIENT_DEBUG(x) 
#endif


#define GATT_CLIENT_DISCOVERY_ALLOC_MEM(ptr) \
{ \
    ptr = calloc(1, sizeof(gatt_client_discovery_t) * MAX_GATT_CLIENT_SERVICES); \
    GATT_CLIENT_DEBUG(("Mem Alloc (Gatt Client - Discovery): size[%u] addr[%p]\n", \
                       sizeof(gatt_client_discovery_t) * MAX_GATT_CLIENT_SERVICES, \
                       (void *)ptr)); \
}

#define GATT_CLIENT_SERVICES_ALLOC_MEM(ptr) \
{ \
    ptr = calloc(1, sizeof(gatt_client_services_t)); \
    GATT_CLIENT_DEBUG(("Mem Alloc (Gatt Client - Services): size[%u] addr[%p]\n", \
                       sizeof(gatt_client_services_t), \
                       (void *)ptr)); \
}

#define GATT_CLIENT_FREE_MEM(ptr) \
{ \
    GATT_CLIENT_DEBUG(("Mem Free (Gatt Client): addr[%p]\n", \
                       (void *)ptr)); \
    free(ptr); \
    ptr = NULL; \
}

/* Find the maximum number of central mode connections */
#ifdef GATT_HID_REMOTE_CONTROL
#define CENTRAL_ROLE_MAX_CONNECTIONS (uint16)(GATT_HID_RC_MAX_REMOTE_SUPORTED)
#else
#define CENTRAL_ROLE_MAX_CONNECTIONS (uint16)(0)
#endif

/* Check security required for current connection */
#define GATT_CLIENT_IS_SEC_REQUIRED(discover) ((discover) && \
        (discover->service == gatt_client_ancs || discover->service == gatt_client_ias || discover->service == gatt_client_hid))


/***************************************************************************
NAME    
    gattClientHasMaxClients
    
DESCRIPTION
    Utility function called to check if maximum gatt client connection is reached.
    
PARAMETERS
    None
    
NOTE
    TRUE if reached else FALSE.
*/
static bool gattClientHasMaxClients(void)
{
    return GATT_CLIENT.number_connections >= MAX_GATT_CLIENT_CONNECTIONS;
}

/****************************************************************************
NAME    
    gattClientDiscoveryComplete
    
DESCRIPTION
    Called when discovery is complete.
    
PARAMETERS
    connection  The GATT connection
    
NOTE
    This function MUST be called with a non-NULL *connection argument.
*/
static void gattClientDiscoveryComplete(gatt_client_connection_t *connection)
{
    gatt_client_services_t *data = gattClientGetServiceData(connection);

    /* Panic if Connection is NULL */
    PanicNull(connection);
    
    GATT_CLIENT_DEBUG(("GATT Client Discovery Complete\n"));
    
    if (data)
    {
        GATT_CLIENT_FREE_MEM(data->discovery);
    }
    
    if(connection->role == ble_gap_role_central)
    {
        /* Connection is now complete after setting security */
        sinkBleCentralConnCompleteEvent(connection->cid);
    }
}


/****************************************************************************
NAME    
    initialiseGattClientService
    
DESCRIPTION
    Called to initialise the GATT service before all other services.
    This is so Service Changed indications can be captured.
    
PARAMETERS
    connection  The connection pointer

*/
static void initialiseGattClientService(gatt_client_connection_t *connection)
{
    gatt_client_discovery_t *discover = gattClientGetDiscoveredServices(connection);
    gatt_client_services_t *data = gattClientGetServiceData(connection);
    uint16 service_count = 0;
    bool gatt_service_found = FALSE;

    if (discover && data)
    {
        for (service_count = 0 ; service_count < data->number_discovered_services ; service_count++)
        {
#if 0			
            if ((discover) && (discover->service == gatt_client_gatt))
            {
                gatt_service_found = sinkGattClientServiceAdd(connection->cid, discover->start_handle, discover->end_handle);
                GATT_CLIENT_DEBUG(("Add gatt client service; success[%u] cid[0x%x] start[0x%x] end[0x%x]\n", 
                                   gatt_service_found,
                                   connection->cid, 
                                   discover->start_handle,
                                   discover->end_handle));
                break;
            }
#else            
            /*20160201*/
            if ((discover) && (discover->service == gatt_client_ancs))
            {
                                    
                sinkGattAncsClientAddService(connection->cid, discover->start_handle, discover->end_handle);

               /* gatt_service_found = sinkGattClientServiceAdd(connection->cid, discover->start_handle, discover->end_handle);*/
				return;
            }
#endif            
            /* Increment to next discovery data */
            discover +=1;
        }
        if (!gatt_service_found)
        {
            /* An error has occurred trying to process GATT service.
               Attempt to continue operation and check for security requirements. */
            gattClientProcessSecurityRequirements(connection, data);
        }
    }
}


/****************************************************************************
NAME    
    gattClientBdAddrtIsBonded
    
DESCRIPTION
    Determines if the typed Bluetooth address of the client device is in the paired device list.
    
PARAMETERS
    client_taddr  The typed Bluetooth address of the client device.
    public_taddr  If the public address is found it will be returned in the
                  location pointed at by this value.
    
RETURNS    
    TRUE if the cient address was found in the paired device list, FALSE otherwise.

*/
static bool gattClientBdAddrtIsBonded(const typed_bdaddr *client_taddr, typed_bdaddr *public_taddr)
{
    tp_bdaddr tp_addr;
    tp_bdaddr tr_addr;
    bool address_ok = FALSE;
    
    GATT_CLIENT_DEBUG(("gattClientBdAddrtIsBonded\n")); 
    
    if (client_taddr->type == TYPED_BDADDR_RANDOM)
    {
        /* Resolve random address to a public one */
        tr_addr.taddr = *client_taddr; 
        tr_addr.transport = TRANSPORT_BLE_ACL;
        address_ok = VmGetPublicAddress(&tr_addr, &tp_addr);
        GATT_CLIENT_DEBUG((" Random Addr\n"));
    }
    else if (client_taddr->type == TYPED_BDADDR_PUBLIC)
    {
        /* Already a Public address */
        tp_addr.taddr = *client_taddr;
        address_ok = TRUE;
        GATT_CLIENT_DEBUG((" Public Addr\n"));
    }
    
    if (address_ok)
    {
        /* Return public address of the client */
        *public_taddr = tp_addr.taddr;
        /* Find if public address is in PDL */
        address_ok = ConnectionSmGetAttributeNow(0, &public_taddr->addr, 0, NULL);
        
        GATT_CLIENT_DEBUG((" Public Addr[%x:%x:%lx]\n",
                               public_taddr->addr.nap,
                               public_taddr->addr.uap,
                               public_taddr->addr.lap
                               ));
    }
        
    GATT_CLIENT_DEBUG((" Bonded=[%u]\n", address_ok)); 
    
    return address_ok;
}



/****************************************************************************
NAME    
    gattDiscoverPrimaryServices
    
DESCRIPTION
    Discovers GATT primary services supported on the remote device.
    
PARAMETERS
    connection             The GATT connection

*/
void gattDiscoverPrimaryServices(gatt_client_connection_t *connection)
{
    GattDiscoverAllPrimaryServicesRequest(sinkGetBleTask(), connection->cid);
}

/****************************************************************************/
gatt_client_discovery_t *gattClientGetDiscoveredServices(gatt_client_connection_t *connection)
{
    gatt_client_services_t *service = NULL;
    
    if (connection)
    {
        service = gattClientGetServiceData(connection);
        
        if (service)
        {
            return service->discovery;
        }
    }
    
    return NULL;
}

/****************************************************************************/
void gattClientInitialiseDiscoveredServices(gatt_client_connection_t *connection)
{
    gatt_client_discovery_t *discover = gattClientGetDiscoveredServices(connection);
    gatt_client_services_t *data = gattClientGetServiceData(connection);
    bool error_flag = TRUE;

    if (discover && data)
    {
        /* Advance the discover pointer based on the current discovered service */
        discover += data->current_discovered_service;
        if(discover)
        {
            GATT_CLIENT_DEBUG(("GATT Discovered Service: cid[0x%x] index[%u]\n", connection->cid, data->current_discovered_service));
            
            GATT_CLIENT_DEBUG(("    service[%u] start[0x%x] end[0x%x]\n", discover->service, discover->start_handle, discover->end_handle));

            /*MYDEBUG(("    service[%u] start[0x%x] end[0x%x]\n", discover->service, discover->start_handle, discover->end_handle));*/
            
            switch (discover->service)
            {
                case gatt_client_battery:
                {
                    if (gattBatteryClientAddService(connection->cid, discover->start_handle, discover->end_handle))
                    {
                        error_flag = FALSE;
                    }
                }
                break;
                case gatt_client_ancs:
                {
                    if (sinkGattAncsClientAddService(connection->cid, discover->start_handle, discover->end_handle))
                    {
                        error_flag = FALSE;
                    }
                }
                break;
                case gatt_client_ias:
                {
                    if (sinkGattIasClientAddService(connection->cid, discover->start_handle, discover->end_handle))
                    {
                        error_flag = FALSE;
                    }
                }
                break;
                case gatt_client_hid:
                {
                    if (sinkGattHidClientAddService(connection->cid, discover->start_handle, discover->end_handle))
                    {
                        error_flag = FALSE;
                    }
                }
                break;
                case gatt_client_dis:
                {
                    if (sinkGattDisClientAddService(connection->cid, discover->start_handle, discover->end_handle))
                    {
                        error_flag = FALSE;
                    }
                }
                break;

                case gatt_client_spc:
                {
                    if (sinkGattSpClientAddService(connection->cid, discover->start_handle, discover->end_handle))
                    {
                        error_flag = FALSE;
                    }
                }
                break;
                
                case gatt_client_gatt:
                {
                    /* Gatt service will already have been initialised before other services.
                       Send internal message to process next service */
                    MESSAGE_MAKE(message, BLE_INTERNAL_MESSAGE_GATT_CLIENT_SERVICE_INITIALISED_T);
                    message->cid = connection->cid;
                    MessageSend(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_GATT_CLIENT_SERVICE_INITIALISED, message);
                    error_flag = FALSE;
                }
                break;
                default:
                {
                    GATT_CLIENT_DEBUG(("GATT unknown discovered service\n"));
                }
                break;
            }
        }
    }
    
    if (error_flag)
    {
        gattClientDiscoveryError(connection);
    }
}


/****************************************************************************/
bool gattClientAdd(uint16 cid, const typed_bdaddr *client_taddr, ble_gap_role_t client_gap_role)
{
    gatt_client_connection_t *connection = NULL;
    gatt_client_services_t *services = NULL;
    sink_attributes attributes;
    typed_bdaddr public_taddr;
    
    if (gattClientHasMaxClients())
    {
        /* Cannot store more connections, maximum reached */
        return FALSE;
    }
    
    connection = &(GATT_CLIENT.connection[GATT_CLIENT.number_connections]);
    GATT_CLIENT_SERVICES_ALLOC_MEM(services);
                
    if (services)
    {   
        /* Record details of the services */
        connection->data = services;

        /* Store the GATT Connection ID */
        connection->cid = cid;
        
        /* Set client service configuration to default value */
        memset(&connection->client_config, 0, sizeof(gatt_client_attributes_t));

        if (gattClientBdAddrtIsBonded(client_taddr, &public_taddr))
        {
            /* Restore client configuration from PS */
            deviceManagerClearAttributes(&attributes);
            if (deviceManagerGetAttributes(&attributes, &public_taddr.addr))
            {
                connection->client_config = attributes.gatt_client;
                GATT_CLIENT_DEBUG(("GATT Client: Read client config addr[%x:%x:%lx] config_gatt[0x%x] config_batt_l[0x%x] config_batt_r[0x%x] config_batt_p[0x%x]\n",
                                   public_taddr.addr.nap,
                                   public_taddr.addr.uap,
                                   public_taddr.addr.lap,
                                   connection->client_config.gatt,
                                   connection->client_config.battery_local,
                                   connection->client_config.battery_remote,
                                   connection->client_config.battery_peer
                                   ));
            }
        }

		/* check for repeated updates */
		if( LOCAL_UPDATE_REQD(connection) ||
			REMOTE_UPDATE_REQD(connection) ||
			PEER_UPDATE_REQD(connection) )
		{
			MessageCancelFirst( sinkGetBleTask(), BLE_INTERNAL_MESSAGE_BATTERY_READ_TIMER );
            MessageSend( sinkGetBleTask(), BLE_INTERNAL_MESSAGE_BATTERY_READ_TIMER, 0 );
		}

        /* Set the connection role */
        connection->role = client_gap_role;
        
        /* Increase number of active connections */
        GATT_CLIENT.number_connections++;
        
        GATT_CLIENT_DISCOVERY_ALLOC_MEM(services->discovery);
        
        if (services->discovery)
        {
            /* Set MTU and kick off discovery once MTU configured */
            GattExchangeMtuRequest(sinkGetBleTask(), cid, SINK_GATT_MTU_MIN);
            return TRUE;
        }
        else
        {
            GATT_CLIENT_FREE_MEM(services);
        }
    }
    
    return FALSE;
}


/****************************************************************************/
gatt_client_connection_t *gattClientFindByCid(uint16 cid)
{
    uint16 i = 0;
    
    for (i = 0; i < GATT_CLIENT.number_connections; i++)
    {
        if (GATT_CLIENT.connection[i].cid && (GATT_CLIENT.connection[i].cid == cid))
        {
            return &(GATT_CLIENT.connection[i]);
        }
    }
    
    return NULL;
}

/****************************************************************************/
bool gattClientRemove(uint16 cid)
{
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
  
    if (connection && GATT_CLIENT.number_connections)
    {
        connection->cid = 0;
        connection->role = ble_gap_role_unknown;
        if (connection->data)
        {
            gatt_client_discovery_t *discover = gattClientGetDiscoveredServices(connection);
            if (discover)
            {
                GATT_CLIENT_FREE_MEM(discover);
            }
            GATT_CLIENT_FREE_MEM(connection->data);
        }
        GATT_CLIENT.number_connections--;
        
        return TRUE;
    }
    
    return FALSE;
}


/****************************************************************************/
uint16 *gattClientAddService(gatt_client_connection_t *connection, uint16 size_service)
{
    if (connection)
    {
        gatt_client_services_t *services = connection->data;
        uint16 size_client_data = sizeof(gatt_client_services_t) + services->size_client_data + size_service;
        gatt_client_services_t *data = realloc(connection->data, size_client_data);
        
        if (data)
        {
            uint16 *end = data->client_data + services->size_client_data;
            data->size_client_data = size_client_data - sizeof(gatt_client_services_t);
            connection->data = data;
            
            return end;
        }   
    }
    
    return NULL;
}

/****************************************************************************\
 * NOTE: This utility function should be called only in case initialization of client library fails. If called in
 * other situation then the entire client_data shall be corrupted.
*****************************************************************************/
static void gattClientDeleteLastService(gatt_client_connection_t *connection, uint16 size_service)
{
    if (connection)
    {
        gatt_client_services_t *services = connection->data;
        uint16 size_client_data = sizeof(gatt_client_services_t) + services->size_client_data - size_service;
        gatt_client_services_t *data = realloc(connection->data, size_client_data);
        
        if (data)
        {
            data->size_client_data = size_client_data - sizeof(gatt_client_services_t);
            connection->data = data;
        }
    }
}

/****************************************************************************/
void gattClientStoreDiscoveredService(uint16 cid, uint16 uuid_type, uint32 *uuid, uint16 start, uint16 end, bool more)
{
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    gatt_client_discovery_t *discover = NULL;
    gatt_client_services_t *data = gattClientGetServiceData(connection);
    bool error_flag = TRUE;
    GATT_CLIENT_DEBUG(("Gatt Client Store Discovered Service \n"));

    if (connection && data)
    {
        discover = gattClientGetDiscoveredServices(connection);
        if (discover)
        {
            /* The discover poiter has to be advanced by the number of services found to update the new service */
            discover += data->number_discovered_services;
            if (discover)
            {
                if(uuid_type == gatt_uuid16)
                {
                    switch (uuid[0])
                    {
                        case GATT_SERVICE_UUID_BATTERY_SERVICE:
                        {
                            /* As per CS-326396-DD connection spec, in peripheral role, 
                                app should initialize only ANCS and IAS client */
                            if(ble_gap_role_central == connection->role)
                            {
                                GATT_CLIENT_DEBUG(("Gatt Client Storing Battery service handles\n"));

                                /* Store discovered battery service */
                                discover->service = gatt_client_battery;
                                discover->start_handle = start;
                                discover->end_handle = end;
                                
                                data->number_discovered_services++;
                            }
                        }
                        break;

                        case GATT_SERVICE_UUID_HUMAN_INTERFACE_DEVICE:
                        {
                            if(ble_gap_role_central == connection->role)
                            {
                                /* Store discovered HID service */
                                discover->service = gatt_client_hid;
                                discover->start_handle = start;
                                discover->end_handle = end;
                                
                                data->number_discovered_services++;
                            }
                        }
                        break;
                        
                        case GATT_SERVICE_UUID_IMMEDIATE_ALERT:
                        {                   
                            GATT_CLIENT_DEBUG(("Gatt Client Storing Imm Alert service handles\n"));

                            /* Store discovered Imm Alert service */
                            discover->service = gatt_client_ias;
                            discover->start_handle = start;
                            discover->end_handle = end;
                            
                            data->number_discovered_services++;
                        }
                        break;

                        case GATT_SERVICE_UUID_DEVICE_INFORMATION:
                        {                   
                            GATT_CLIENT_DEBUG(("Gatt Client Storing Device Info service handles\n"));

                            /* Store discovered Device Info service */
                            discover->service = gatt_client_dis;
                            discover->start_handle = start;
                            discover->end_handle = end;
                            
                            data->number_discovered_services++;
                        }
                        break;
                        
                        case GATT_SERVICE_UUID_GENERIC_ATTRIBUTE:
                        {                   
                            GATT_CLIENT_DEBUG(("Gatt Client Storing GATT service handles\n"));

                            /* Store discovered GATT service */
                            discover->service = gatt_client_gatt;
                            discover->start_handle = start;
                            discover->end_handle = end;
                            
                            data->number_discovered_services++;
                        }
                        break;

                        case GATT_SERVICE_UUID_SCAN_PARAMETERS:
                        {                   
                            GATT_CLIENT_DEBUG(("Gatt Client Storing Scan params service handles\n"));

                            /* Store discovered Scan params service */
                            discover->service = gatt_client_spc;
                            discover->start_handle = start;
                            discover->end_handle = end;
                            
                            data->number_discovered_services++;
                        }
                        break;
                        
                        /* Handle other services that client is interested in */
                        
                        default:
                        {
                            /* Ignore unknown services */
                        }
                        break;
                    }
                }
                else if(uuid_type == gatt_uuid128)
                {
                    if(CHECK_ANCS_SERVICE_UUID(uuid))
                    {
                        /* Store discovered apple notification service */
                        discover->service = gatt_client_ancs;
                        discover->start_handle = start;
                        discover->end_handle = end;
                        
                        data->number_discovered_services++;
                    }
                }

                if (!more)
                {
                    /* We are done with getting the primary service */
                    sinkBleGapSetDiscoveryInProgress(FALSE);
                    
                    if (data->number_discovered_services)
                    {
                        /* Initialise GATT service first to handle any Service Changed indications */
                        initialiseGattClientService(connection);
                    }
                    else
                    {
                        if(connection->role == ble_gap_role_central)
                        {
                            /* In case the client was connected in central role, then this client connection is not required
                             * because remote device does not have any service of our interest */
                             gattClientDisconnect(cid);
                        }
                        else
                        {
                            /* In case the client was connected in peripheral role, then just remove the client
                             * Because server might be still connected */
                            gattClientRemove(cid);
                        }
                    }
                }
                error_flag = FALSE;
            }
        }
    }
    
    if (error_flag)
    {
        gattClientDiscoveryError(connection);
    }
}


/****************************************************************************/
gatt_client_services_t *gattClientGetServiceData(gatt_client_connection_t *connection)
{
    if (connection)
    {
        return connection->data;
    }
    
    return NULL;
}


/****************************************************************************/
void gattClientDiscoveredServiceInitialised(gatt_client_connection_t *connection)
{
    gatt_client_services_t *data = gattClientGetServiceData(connection);
    
    if (connection && connection->service_changed)
    {
        if (data)
        {
            GATT_CLIENT_FREE_MEM(data->discovery);
        }
        gattClientServiceChanged(connection->cid);
        
        /* Return early as discovery will be restarted */
        return;
    }
    
    if (data)
    {
        data->current_discovered_service++;
        if (data->current_discovered_service >= data->number_discovered_services)
        {
            GATT_CLIENT_DEBUG(("GATT Primary Services Initialised\n"));
            /* Discovery of primary services is now complete */
            gattClientDiscoveryComplete(connection);
        }
        else
        {
            gattClientInitialiseDiscoveredServices(connection);
        }
    }
    else
    {
        gattClientDiscoveryError(connection);
    }
}


/****************************************************************************/
bool gattClientRemoveDiscoveredService(gatt_client_connection_t *connection, 
                                       gatt_client_tasks_t service)
{
    bool retVal = FALSE;
    gatt_client_services_t *data = gattClientGetServiceData(connection);
    if (data)
    {
        /* Move to the last discovered service */
        switch(service)
        {
            case gatt_client_ancs:
            {
                if(data->ancs)
                {
                    sinkGattAncsClientRemoveService(data->ancs);
                    gattClientDeleteLastService(connection, sizeof(GANCS));
                    if(data && data->ancs)
                    {
                        data->ancs = NULL;
                    }
                    retVal = TRUE;
                }
            }
            break;

            case gatt_client_spc:
            {
                if(data->spc)
                {
                    sinkGattSpClientRemoveService(data->spc);
                    gattClientDeleteLastService(connection, sizeof(GSPC_T));
                    if(data && data->spc)
                    {
                        data->spc = NULL;
                    }
                    retVal = TRUE;
                }
            }
            break;
            default:
            break;
        }
    }

    return retVal;
}

/****************************************************************************/
void gattClientDisconnect(uint16 cid)
{
    if (gattClientFindByCid(cid))
    {
        GattManagerDisconnectRequest(cid);
    }
}


/****************************************************************************/
bool gattClientDisconnectAll(void)
{
    uint16 i = 0;
    bool disconnect_request = FALSE;
    
    for (i = 0; i < GATT_CLIENT.number_connections; i++)
    {
        if (GATT_CLIENT.connection[i].cid)
        {
            GattManagerDisconnectRequest(GATT_CLIENT.connection[i].cid);
            disconnect_request = TRUE;
        }
    }
    
    return disconnect_request;
}


/***************************************************************************/
void gattClientDiscoveryError(gatt_client_connection_t *connection)
{
    GATT_CLIENT_DEBUG(("GATT Client Discovery Error!\n"));

    /* Reset the primary service discovery flag */
    sinkBleGapSetDiscoveryInProgress(FALSE);
    
    if (connection)
    {
        gattClientDiscoveryComplete(connection);
    }
}

/***************************************************************************/
static uint16 gattClientGetNumberOfCentralConnected(void)
{
    uint16 central_conn = 0;
    uint16 conn = 0;
    /* Find the number of central device connected */
    while(conn < MAX_GATT_CLIENT_CONNECTIONS)
    {
        if(GATT_CLIENT.connection[conn].role == ble_gap_role_central)
            central_conn++;
        conn++;
    }

    return central_conn;
}

/***************************************************************************/
bool gattClientHasMaxCentralClients(void)
{
    /* If max_central_conn is 0, then we should not allow any central connection */
    if(CENTRAL_ROLE_MAX_CONNECTIONS)
    {
        /* If already all the client connection is used, then don't allow any more central connection.
          * Need to drop one of the client connection to allow any central connection */
        if(!gattClientHasMaxClients())
        {
            /* Get the total number of central devices connected */
            return gattClientGetNumberOfCentralConnected() >= CENTRAL_ROLE_MAX_CONNECTIONS;
        }
    }
    return TRUE;
}

/***************************************************************************/
bool gattClientHasNoClients(void)
{
    return (GATT_CLIENT.number_connections == 0);
}


/***************************************************************************/
void gattClientRemoveServices(gatt_client_connection_t *client_connection)
{
    gatt_client_services_t *services = gattClientGetServiceData(client_connection);
    
    /* Remove services if used */
    if (services)
    {
        if (services->basc)
        {
            gattBatteryClientRemoveService(services->basc, client_connection->cid);
        }
        if(services->ancs)
        {
            sinkGattAncsClientRemoveService(services->ancs);
        }
        if(services->hidc_instance1)
        {
            sinkGattHIDClientRemoveService(services->hidc_instance1, client_connection->cid);
        }
        if(services->hidc_instance2)
        {
            sinkGattHIDClientRemoveService(services->hidc_instance2, client_connection->cid);
        }
        if(services->disc)
        {
            sinkGattDISClientRemoveService(services->disc, client_connection->cid);
        }
        if(services->iasc)
        {
            sinkGattIasClientRemoveService(services->iasc, client_connection->cid);
        }
        if (services->gattc)
        {
            sinkGattClientServiceRemove(services->gattc, client_connection->cid);
        }
        if(services->spc)
        {
            sinkGattSpClientRemoveService(services->spc);
        }
        /* Remove more services here */
    }            
}


/***************************************************************************/
void gattClientServiceChanged(uint16 cid)
{
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    gatt_client_services_t *services = gattClientGetServiceData(connection);
    
    if (connection)
    {
        connection->service_changed = TRUE;
        
        /* If service discovery is in progress then wait for discovery to complete
           before re-discovering services.
           Otherwise re-discover services immediately.
        */
        if (services && (services->discovery == NULL))
        {
            MESSAGE_MAKE(message, BLE_INTERNAL_MESSAGE_REDISCOVER_SERVER_SERVICES_T);
            
            /* Remove all services from client */
            gattClientRemoveServices(connection);
            GATT_CLIENT_FREE_MEM(connection->data);
            
            /* Send message to discover services again */
            message->cid = cid;
            MessageCancelFirst(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_REDISCOVER_SERVER_SERVICES);
            MessageSend(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_REDISCOVER_SERVER_SERVICES, message);
        }
    }
}


/***************************************************************************/
void gattClientRediscoverServices(uint16 cid)
{
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    gatt_client_services_t *services = NULL;
    bool error_flag = TRUE;
    
    if (connection == NULL)
        return;
    
    GATT_CLIENT_SERVICES_ALLOC_MEM(services);
    
    if (services)
    {
        /* Record details of the services */
        connection->data = services;
        
        /* Discover services again for this connection */
        GATT_CLIENT_DISCOVERY_ALLOC_MEM(services->discovery);
            
        if (services->discovery)
        {
            /* Discover primary services on this connection */
            gattDiscoverPrimaryServices(connection);
            error_flag = FALSE;
        }
    }
    
    if (error_flag)
    {
        /* Disconnect on error condition */
        gattClientDisconnect(cid);
    }
    
    connection->service_changed = FALSE;
}


/***************************************************************************/
void gattClientProcessSecurityRequirements(gatt_client_connection_t *connection, gatt_client_services_t *data)
{
   tp_bdaddr current_bd_addr;
   uint16 service_count= 0;
   bool is_security_required = FALSE;
   gatt_client_discovery_t *discover = gattClientGetDiscoveredServices(connection);


    /* Here only pairing or encryption in case of central role is taken care and
    in case of peripheral role wait for bonding timeout to occur there by allowing
    for remote central device to pair and if remote device does not pair within the timeout 
    initiate encryption */

    for (service_count = 0 ; service_count < data->number_discovered_services ; service_count++)
    {
        /* Check any services required security */
        if(GATT_CLIENT_IS_SEC_REQUIRED(discover))
        {
            is_security_required = TRUE;
            break;
        }
        /* Get the next discovered service */
        discover+=1;
    }
     GATT_CLIENT_DEBUG(("gattClientProcessSecurityRequirements: is_security_required = %d\n", is_security_required));
    if(is_security_required)
    {
        if(connection->role == ble_gap_role_central)
        {
            if(VmGetBdAddrtFromCid(connection->cid, &current_bd_addr))
            {
                /* Initiate Encryption request */
                GATT_CLIENT_DEBUG(("Gatt Initiate Encryption request \n"));
                
                ConnectionDmBleSecurityReq(sinkGetBleTask(), 
                                            (const typed_bdaddr *)&current_bd_addr.taddr, 
                                            ble_security_encrypted_bonded,
                                            ble_connection_master_directed
                                            );
            }
        }
        else
        {
            /* In case of already bonded peripheral device we shall not have bond timer to send encryption,
             * therefore send remote_connect_success event which shall trigger encryption.
             * On encryption success/failure we shall initialize client library accordingly */
            sinkBleRemoteConnectionSuccessEvent(connection->cid);
        }
    }
    else
    {
        /* If no security required i.e. discovered service does not require encryption 
            initialize discovered services?
        */
        gattClientInitialiseDiscoveredServices(connection);
    }
}


/***************************************************************************/
void gattClientStoreConfigAttributes(uint16 cid, gatt_attribute_service_t client_service)
{
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    sink_attributes attributes;
    tp_bdaddr tpaddr;
    typed_bdaddr public_taddr;
    bool attr_changed = FALSE;
   
    /* Store client configuration to PS */
    if (connection && VmGetBdAddrtFromCid(cid, &tpaddr))
    {
        if (gattClientBdAddrtIsBonded(&tpaddr.taddr, &public_taddr))
        {
            deviceManagerClearAttributes(&attributes);
            if (!deviceManagerGetAttributes(&attributes, &public_taddr.addr))
            {
                /* No attributes exists for this device so write defaults for BLE - currently no default attributes. */
                attr_changed = TRUE;
            }

            switch(client_service)
            {
                case gatt_attr_service_battery_local:
                    if (attributes.gatt_client.battery_local != connection->client_config.battery_local)
                    {
                        attributes.gatt_client.battery_local = connection->client_config.battery_local;
                        attr_changed = TRUE;
                    }
                    break;
                case gatt_attr_service_battery_remote:
                    if (attributes.gatt_client.battery_remote != connection->client_config.battery_remote)
                    {
                        attributes.gatt_client.battery_remote = connection->client_config.battery_remote;
                        attr_changed = TRUE;
                    }
                    break;
                case gatt_attr_service_battery_peer:
                    if (attributes.gatt_client.battery_peer != connection->client_config.battery_peer)
                    {
                        attributes.gatt_client.battery_peer = connection->client_config.battery_peer;
                        attr_changed = TRUE;
                    }
                    break;
                case gatt_attr_service_gatt:
                    if (attributes.gatt_client.gatt != connection->client_config.gatt)
                    {
                        attributes.gatt_client.gatt = connection->client_config.gatt;
                        attr_changed = TRUE;
                    }
                    break;
                case gatt_attr_service_all:
                    attributes.gatt_client = connection->client_config;
                    attr_changed = TRUE;
                    break;
                default:
                    /* No client config attributes handled for this service */
                    break;
            }
            
            GATT_CLIENT_DEBUG(("GATT Client: Update client config; service[%u] changed[%u]\n", client_service, attr_changed));
                               
            if (attr_changed)
            {
                deviceManagerStoreAttributes(&attributes, &public_taddr.addr);
                GATT_CLIENT_DEBUG(("GATT Client: Store client config addr[%x:%x:%lx] config_gatt[0x%x] config_batt_l[0x%x] config_batt_r[0x%x] config_batt_p[0x%x]\n",
                               public_taddr.addr.nap,
                               public_taddr.addr.uap,
                               public_taddr.addr.lap,
                               connection->client_config.gatt,
                               connection->client_config.battery_local,
                               connection->client_config.battery_remote,
                               connection->client_config.battery_peer
                               ));
            }
        }
    }
}


/***************************************************************************/
bool gattClientGapNameReadRequested(uint16 cid, uint16 offset)
{
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    if (connection)
    {
        GATT_CLIENT_DEBUG(("GATT Client: Read GAP name request cid[0x%x] offset[0x%x]\n", cid, offset));
        /* Store the data to use for the response */
        connection->gap_read_name.requested = TRUE;
        connection->gap_read_name.offset = offset;
        return TRUE;
    }
    
    return FALSE;
}


/***************************************************************************/
void gattClientGapNameReadResponse(GGAPS *gap_server, uint16 size_local_name, uint8 *local_name)
{
    uint16 i = 0;
    gatt_client_connection_t *connection = NULL;
    
    for (i = 0; i < GATT_CLIENT.number_connections; i++)
    {
        connection = &GATT_CLIENT.connection[i];
        if (connection->cid)
        {
            if (connection->gap_read_name.requested)
            {
                if (connection->gap_read_name.offset >= size_local_name)
                {
                    GATT_CLIENT_DEBUG(("GATT Client: Read GAP name response Invalid cid[0x%x]\n", connection->cid));
                    GattGapServerReadDeviceNameResponse(gap_server,
                                                         connection->cid,
                                                         0,
                                                         NULL);
                }
                else
                {
                    GATT_CLIENT_DEBUG(("GATT Client: Read GAP name response cid[0x%x] size[0x%x] offset[0x%x]\n", 
                                       connection->cid,
                                       size_local_name,
                                       connection->gap_read_name.offset));
                    GattGapServerReadDeviceNameResponse(gap_server,
                                                         connection->cid,
                                                         size_local_name - connection->gap_read_name.offset,
                                                         &local_name[connection->gap_read_name.offset]);
                }
                connection->gap_read_name.requested = FALSE;
                connection->gap_read_name.offset = 0;
            }
        }
    }
}

#endif /* GATT_ENABLED */

