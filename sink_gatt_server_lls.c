/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.

FILE NAME
    sink_gatt_server_lls.c

DESCRIPTION
    Routines to handle messages sent from the GATT Link Loss Server Task.
*/


/* Firmware headers */
#include <csrtypes.h>
#include <message.h>

/* Application headers */
#include "sink_gatt_db.h"
#include "sink_gatt_server_lls.h"

#include "sink_debug.h"
#include "sink_gatt_server.h"
#include "sink_private.h"


#ifdef GATT_LLS_SERVER


#ifdef DEBUG_GATT
#define GATT_DEBUG(x) DEBUG(x)
#else
#define GATT_DEBUG(x) 
#endif


/*******************************************************************************/
bool sinkGattLinkLossServerInitialiseTask(uint16 **ptr)
{
    if (ptr)
    {
        gatt_link_loss_server_status lls_status;
        lls_status = GattLinkLossServerInit(sinkGetBleTask(), (GLLSS_T*)*ptr,
                                HANDLE_LINK_LOSS_SERVICE,
                                HANDLE_LINK_LOSS_SERVICE_END);
        
        if (lls_status == gatt_link_loss_server_status_success)
        {
            GATT_DEBUG(("GATT Link Loss Server initialised\n"));
            GATT_SERVER.lls_server = (GLLSS_T*)*ptr;
           *ptr += sizeof(GLLSS_T);
            return TRUE;
        }
        else
        {
            GATT_DEBUG(("GATT Link Loss Server init failed [%x]\n", lls_status));
        }
    }
    return FALSE;
}

/******************************************************************************/
void sinkGattHandleLinkUpInd(uint16 cid)
{
    /* Check if the cid is same of the connected device */
    if(GATT_SERVER.cid == cid)
    {
        /* Stop timer and stop alert */
        sinkGattLinkLossAlertStop();
    }
}

/******************************************************************************/
void sinkGattHandleLinkLossInd(uint16 cid)
{
    bool alertStopTimer = FALSE;
    gatt_link_loss_alert_level alert_level = gatt_link_loss_alert_level_reserved;
    /* Check if the cid is same of the connected device */
    if(cid != GATT_SERVER.cid)
    {
        return;
    }

    /* Read the alert level from the library */
    alert_level = GattLinkLossServerGetAlertLevel(GATT_SERVER.lls_server);

    /* Only if the alert level is set, handle it properly */
    switch(alert_level)
    {
        case gatt_link_loss_alert_level_no:
        /* Need not alert on link loss */
        break;

        case gatt_link_loss_alert_level_mild:
        /* Generate Mild Alert level event */
        {
            alertStopTimer = TRUE;
            sinkGattLinkLossAlertStop();
            sinkGattLinkLossAlertMild(0);
        }
        break;
        case gatt_link_loss_alert_level_high:
        /* Generate High Alert level event */
        {
            alertStopTimer = TRUE;
            sinkGattLinkLossAlertStop();
            sinkGattLinkLossAlertHigh(0);
        }
        break;
        
        default:
        break;
    }

    if(alertStopTimer)
    {
        /* Start the stop alert timeout if either High/Mild alert is set */
        MessageSendLater(&theSink.task, EventSysLlsAlertTimeout , 0, D_SEC(theSink.conf1->timeouts.LinkLossAlertStopTimeout_s));
    }
}

/******************************************************************************
 * Helper funtion to stop the alterting
******************************************************************************/ 
void sinkGattLinkLossAlertStop(void)
{
    MessageCancelAll(&theSink.task, EventSysLlsAlertTimeout);
    MessageCancelAll(&theSink.task, EventSysLlsAlertMild);
    MessageCancelAll(&theSink.task, EventSysLlsAlertHigh);
}

/******************************************************************************/
void sinkGattLinkLossServerMsgHandler(Task task, MessageId id, Message message)
{
    switch(id)
    {
        case GATT_LLS_ALERT_LEVEL_CHANGE_IND:
        {
             GATT_DEBUG(("GATT Link Loss Alert level changed\n"));
        }
        break;

        default:
             GATT_DEBUG(("GATT Unknown message from LLS lib\n"));
         break;
    }
}

#endif /* GATT_LLS_SERVER */

