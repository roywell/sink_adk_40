/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.

FILE NAME
    sink_gatt_server_gap.c

DESCRIPTION
    Routines to handle messages sent from the GATT GAP Server task.
*/
#include "sink_gatt_db.h"
#include "sink_gatt_server_gap.h"

#include "sink_debug.h"
#include "sink_development.h"
#include "sink_gatt_device.h"
#include "sink_private.h"

#include <gatt_gap_server.h>

#include <csrtypes.h>
#include <message.h>


#ifdef GATT_ENABLED


#ifdef DEBUG_GATT
#define GATT_INFO(x) DEBUG(x)
#define GATT_ERROR(x) DEBUG(x) TOLERATED_ERROR(x)
#else 
#define GATT_INFO(x) 
#define GATT_ERROR(x) 
#endif


/*******************************************************************************
NAME
    sinkGattGapServerInitialiseTask
    
DESCRIPTION
    Initialise the GAP server task.
    NOTE: This function will modify *ptr.
    
PARAMETERS
    ptr - pointer to allocated memory to store server tasks rundata.
    
RETURNS
    TRUE if the GAP server task was initialised, FALSE otherwise.
*/
bool sinkGattGapServerInitialiseTask(uint16 **ptr)
{
    if (ptr)
    {
        if (GattGapServerInit((GGAPS*)*ptr, sinkGetBleTask(),
                                         HANDLE_GAP_SERVICE,
                                         HANDLE_GAP_SERVICE_END) == gatt_gap_server_status_success)
        {
            GATT_INFO(("GATT GAP Server initialised\n"));
            GATT_SERVER.gap_server = (GGAPS*)*ptr;
            *ptr += sizeof(GGAPS);
            return TRUE;
        }
        else
        {
            GATT_INFO(("GATT GAP Server init failed\n"));
        }
    }
    return FALSE;
}


/*******************************************************************************
NAME
    sinkGattGapServerHandleReadAppearanceInd
    
DESCRIPTION
    Function to handle when a remote device wants to read the appearance 
    characteristic of the GAP service.
    
PARAMETERS
    void
    
RETURNS
    TRUE if the initialisation request was successful, FALSE otherwise.
*/
static bool sinkGattGapServerHandleReadAppearanceInd(void)
{
    /* Appearance=unknown; refer to Bluetooth Sig website for appearance values */
    uint8 appearance[2] = {0x00, 0x00};
    gatt_gap_server_status_t status;
    
    status = GattGapServerReadAppearanceResponse(GATT_SERVER.gap_server, GATT_SERVER.cid, 2, appearance);
    
    if (status == gatt_gap_server_status_success)
    {
        return TRUE;
    }
    return FALSE;
}


/******************************************************************************/
void sinkGattGapServerSendLocalNameResponse(CL_DM_LOCAL_NAME_COMPLETE_T * msg)
{ 
    gattClientGapNameReadResponse(GATT_SERVER.gap_server, msg->size_local_name, msg->local_name);
}


/******************************************************************************/
gap_msg_status_t sinkGattGapServerMsgHandler(Task task, MessageId id, Message message)
{
    gap_msg_status_t status = gap_msg_failed;
    
    switch(id)
    {
        case GATT_GAP_SERVER_READ_DEVICE_NAME_IND:
        {
            GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T* ind
             = (GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T*)message;
            GATT_INFO(("GATT_GAP_SERVER_READ_DEVICE_NAME_IND\n"));
            if (gattClientGapNameReadRequested(ind->cid, ind->name_offset))
            {
                status = gap_msg_read_name_required;
            }
        }
        break;
        case GATT_GAP_SERVER_READ_APPEARANCE_IND:
        {
            GATT_INFO(("GATT_GAP_SERVER_READ_APPEARANCE_IND\n"));
            sinkGattGapServerHandleReadAppearanceInd();
            status = gap_msg_success;
        }
        break;
        default:
        {
            GATT_ERROR(("GATT GAP Server task unhandled msg[%x]\n", id));
        }
    }
    
    return status;
}


#endif /* GATT_ENABLED */
