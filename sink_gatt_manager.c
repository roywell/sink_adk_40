/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.

FILE NAME
    sink_gatt_manager.c        

DESCRIPTION
    Contains GATT Manager functionality.
*/

#include "sink_gatt_manager.h"

#include "sink_ble.h"
#include "sink_ble_gap.h"
#include "sink_debug.h"
#include "sink_development.h"
#include "sink_gaia.h"
#include "sink_gatt_client.h"
#include "sink_gatt_client_battery.h"
#include "sink_gatt_client_ancs.h"
#include "sink_gatt_client_ias.h"
#include "sink_gatt_client_hid.h"
#include "sink_gatt_client_dis.h"
#include "sink_gatt_client_gatt.h"
#include "sink_gatt_server_lls.h"
#include "sink_gatt_server.h"
#include "sink_gatt_server_battery.h"
#include "sink_gatt_server_gatt.h"
#include "sink_private.h"
#include "sink_statemanager.h"

#include <gatt_manager.h>

#include <csrtypes.h>
#include <stdlib.h>


#ifdef GATT_ENABLED


#ifdef DEBUG_GATT_MANAGER
#define GATT_MANAGER_INFO(x) DEBUG(x)
#define GATT_MANAGER_ERROR(x) DEBUG(x) TOLERATED_ERROR(x)
#else
#define GATT_MANAGER_INFO(x)
#define GATT_MANAGER_ERROR(x)
#endif


/*******************************************************************************
NAME
    handleGattManagerRegistrationCfm
    
DESCRIPTION
    Handle when GATT_MANAGER_REGISTER_WITH_GATT_CFM message was received
    
PARAMETERS
    cfm Pointer to a GATT_MANAGER_REGISTER_WITH_GATT_CFM message
    
RETURNS
    TRUE if the message was handled, FALSE otherwise
*/
static bool handleGattManagerRegistrationCfm(GATT_MANAGER_REGISTER_WITH_GATT_CFM_T * cfm)
{
    GATT_MANAGER_INFO(("GATT_MANAGER_REGISTER_WITH_GATT_CFM status=[0x%x]\n", cfm->status));
    if (cfm)
    {
        if (cfm->status == gatt_manager_status_success)
        {
            MessageSend(sinkGetBleTask(), BLE_INTERNAL_MESSAGE_INIT_COMPLETE, 0);
            return TRUE;
        }
    }
    return FALSE;
}


/*******************************************************************************
NAME
    handleGattManagerRemoteClientConnectCfm
    
DESCRIPTION
    Handle when GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM message was received
    
PARAMETERS
    cfm Pointer to a GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM message
    
RETURNS
    TRUE if the message was handled, FALSE otherwise
*/
static bool handleGattManagerRemoteClientConnectCfm(GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T * cfm)
{
    GATT_MANAGER_INFO(("GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM status=[0x%x]\n", cfm->status));
    if (cfm)
    {
        if (cfm->status == gatt_status_success)
        {
            /* Add the server connection */
            if (gattServerConnectionAdd(cfm->cid))
            {
                /* There has been a link up, so check if PXP if alert has to be stopped */
                sinkGattHandleLinkUpInd(cfm->cid);
                /* Associate GAIA with the connection ID */
                gaiaGattConnect(cfm->cid);
                /* Send connection success event */
                sinkBleRemoteConnectionSuccessEvent(cfm->cid);

                /* Record this connection which stores device specific data */
                gattClientAdd(cfm->cid, &cfm->taddr, ble_gap_role_peripheral);






                return TRUE;
            }
        }
    }
    
    /* Send connection failed event */
    sinkBleRemoteConnectionFailEvent();
            
    return FALSE;
}


/*******************************************************************************
NAME
    handleGattManagerRemoteServerConnectCfm
    
DESCRIPTION
    Handle when GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM message was received
    
PARAMETERS
    cfm Pointer to a GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM message
    
RETURNS
    TRUE if the message was handled, FALSE otherwise
*/
static bool handleGattManagerRemoteServerConnectCfm(GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM_T * cfm)
{
    GATT_MANAGER_INFO(("GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM status=[0x%x]\n", cfm->status));
    if (cfm)
    {
        if (cfm->status == gatt_status_success)
        {
            /* Record the new connection */
            if (!gattClientAdd(cfm->cid, &cfm->taddr, ble_gap_role_central))
            {
                GATT_MANAGER_INFO(("Couldn't store client->server connection!\n"));
                
                /* Remove GATT connection if it couldn't be stored */
                GattManagerDisconnectRequest(cfm->cid);
                        
                return FALSE;
            }
            
            /* Send connection success event */
            sinkBleLocalConnectionSuccessEvent();
            
            return TRUE;
        }
    }
    return FALSE;
}


/*******************************************************************************
NAME
    handleGattManagerDisconnectInd
    
DESCRIPTION
    Handle when GATT_MANAGER_DISCONNECT_IND message was received
    
PARAMETERS
    ind Pointer to a GATT_MANAGER_DISCONNECT_IND message
    
RETURNS
    TRUE if the message was handled, FALSE otherwise
*/
static bool handleGattManagerDisconnectInd(GATT_MANAGER_DISCONNECT_IND_T * ind)
{
    gatt_client_connection_t *client_connection = gattClientFindByCid(ind->cid);
    bool server_connection = gattServerConnectionFindByCid(ind->cid);
    
    GATT_MANAGER_INFO(("GATT_MANAGER_DISCONNECT_IND\n"));
    
    if (ind)
    {  
        if (client_connection)
        {
            GATT_MANAGER_INFO(("    Client\n"));
            
            /* Remove client services */
            gattClientRemoveServices(client_connection);
            
            /* Finally remove client connection */
            if (!gattClientRemove(ind->cid))
            {
                GATT_MANAGER_ERROR(("Couldn't find client->server connection to remove!\n"));
            }
            
            /* This would have been a connection made in Central role, send event to inform of disconnection */
            sinkBleCentralDisconnectionEvent();
        }
        if (server_connection)
        {
            GATT_MANAGER_INFO(("    Server\n"));

            /* There has been a link loss, so check if PXP has to alerted */
            if(ind->status == gatt_status_link_loss)
            {
                sinkGattHandleLinkLossInd(ind->cid);
            }
            
            /* Server disconnect happened ,inform the disconnect to gaia */
            gaiaGattDisconnect(ind->cid);

            if (!gattServerConnectionRemove(ind->cid))
            {
                GATT_MANAGER_ERROR(("Couldn't find server->client connection to remove!\n"));
            }   
        }
        
        /* Send an event if no connections now exist */
        sinkBleCheckNoConnectionsEvent();
        
        return TRUE;
    }
    return FALSE;
}

/*******************************************************************************
NAME
    handleGattManagerCancelRemoteClientConnectCfm
    
DESCRIPTION
    Handle when GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM message was received
    
PARAMETERS
    cfm Pointer to a GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM message
    
RETURNS
    Nothing
*/
static void handleGattManagerCancelRemoteClientConnectCfm(GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM_T* cfm)
{
    GATT_MANAGER_INFO(("GattMgr: Cancel advertising cfm status=[%x]\n", cfm->status));
    
    /* Generate a cancel advertising which can restart advertising in a different mode */
    sinkBleCancelAdvertisingEvent();
}


/******************************************************************************/
void sinkGattManagerStartAdvertising(void)
{
    GATT_MANAGER_INFO(("GattMgr: Start advertising\n"));


	
    GattManagerWaitForRemoteClient(sinkGetBleTask(), NULL, gatt_connection_ble_slave_undirected);

	/*MessageSendLater( &theSink.task, EventUsrBleStartBonding, 0, D_SEC(1) );*//*roywell 20160126*/
}


/******************************************************************************/
void sinkGattManagerStopAdvertising(void)
{
    GATT_MANAGER_INFO(("GattMgr: Send stop advertising request\n"));
    GattManagerCancelWaitForRemoteClient();
    /* Wait for GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM to indicate outcome of
     * the request.
     */
}


/******************************************************************************/
void sinkGattManagerMsgHandler( Task task, MessageId id, Message message )
{
	/*MYDEBUG(("sinkGattManagerMsgHandler id=%x\r\n", id));*/
	
    switch(id)
    {
        case GATT_MANAGER_REGISTER_WITH_GATT_CFM:
        {
            if (!handleGattManagerRegistrationCfm((GATT_MANAGER_REGISTER_WITH_GATT_CFM_T*)message))
            {
                FATAL_ERROR(("handleGattManagerRegistrationCfm failed\n"));
            }
        }
        break;
        case GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM:
        {
            handleGattManagerRemoteClientConnectCfm((GATT_MANAGER_REMOTE_CLIENT_CONNECT_CFM_T*)message);
        }
        break;
        case GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM:
        {
            if (!handleGattManagerRemoteServerConnectCfm((GATT_MANAGER_REMOTE_SERVER_CONNECT_CFM_T*)message))
            {
                sinkBleCentralConnCompleteEvent(0);
            }
        }
        break;
        case GATT_MANAGER_DISCONNECT_IND:
        {
            handleGattManagerDisconnectInd((GATT_MANAGER_DISCONNECT_IND_T*)message);
        }
        break;
        case GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM:
        {
            handleGattManagerCancelRemoteClientConnectCfm((GATT_MANAGER_CANCEL_REMOTE_CLIENT_CONNECT_CFM_T*)message);
        }
        break;
        case GATT_MANAGER_REMOTE_SERVER_NOTIFICATION_IND:
        {
            /* Ignore any notifications received before the client libraries have been initialised */
            GATT_MANAGER_INFO(("GATT_MANAGER_REMOTE_SERVER_NOTIFICATION_IND ignored cid=[%u] handle=[%u]\n",
                               ((GATT_MANAGER_REMOTE_SERVER_NOTIFICATION_IND_T*)message)->cid,
                               ((GATT_MANAGER_REMOTE_SERVER_NOTIFICATION_IND_T*)message)->handle));
        }
        break;
        case GATT_MANAGER_REMOTE_SERVER_INDICATION_IND:
        {
            /* Respond to any indications received before the client libraries have been initialised */
            GATT_MANAGER_INFO(("GATT_MANAGER_REMOTE_SERVER_INDICATION_IND ignored cid=[%u] handle=[%u]\n",
                               ((GATT_MANAGER_REMOTE_SERVER_INDICATION_IND_T*)message)->cid,
                               ((GATT_MANAGER_REMOTE_SERVER_INDICATION_IND_T*)message)->handle));
            GattManagerIndicationResponse(((GATT_MANAGER_REMOTE_SERVER_NOTIFICATION_IND_T*)message)->cid);
        }
        break;
        case GATT_MANAGER_SERVER_ACCESS_IND:
        {
            /* No server handle exists for this access request, so just reject. */
            GattAccessResponse(((GATT_MANAGER_REMOTE_SERVER_INDICATION_IND_T*)message)->cid,
                               ((GATT_MANAGER_REMOTE_SERVER_INDICATION_IND_T*)message)->handle, 
                               gatt_status_request_not_supported, 
                               0, 
                               NULL);
            
            /* Inform the remote device that it has invalid handles, and should start a new discovery */
            sinkGattServerSendServiceChanged(((GATT_MANAGER_REMOTE_SERVER_INDICATION_IND_T*)message)->cid);
        }
        break;
        case GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM:
        case GATT_MANAGER_REMOTE_CLIENT_INDICATION_CFM:
        default:
        {
            GATT_MANAGER_ERROR(("GATT Manager unhandled msg[%x]\n", id));
        }
        break;
    }
}


/******************************************************************************/
void sinkGattManagerStartConnection(const typed_bdaddr *addr)
{
    GATT_MANAGER_INFO(("GattMgr: Start Connection\n"));
    
    GattManagerConnectToRemoteServer(sinkGetBleTask(),
                        addr,
                        gatt_connection_ble_master_directed,
                        TRUE);
}


#endif /* GATT_ENABLED */
