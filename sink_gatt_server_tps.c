/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.

FILE NAME
    sink_gatt_server_tps.c

DESCRIPTION
    Routines to handle messages sent from the GATT Transmit Power Server Task.
*/


/* Firmware headers */
#include <csrtypes.h>
#include <message.h>

/* Application headers */
#include "sink_gatt_db.h"
#include "sink_gatt_server_tps.h"

#include "sink_debug.h"
#include "sink_gatt_server.h"
#include "sink_private.h"


#ifdef GATT_TPS_SERVER


#ifdef DEBUG_GATT
#define GATT_DEBUG(x) DEBUG(x)
#else
#define GATT_DEBUG(x) 
#endif

/*******************************************************************************/
bool sinkGattTxPowerServerInitialiseTask(uint16 **ptr)
{
    gatt_tps_status tps_status;
    tps_status = GattTransmitPowerServerInitTask(sinkGetBleTask(), (GTPSS *)*ptr,
                                            HANDLE_TRANSMIT_POWER_SERVER_SERVICE,
                                            HANDLE_TRANSMIT_POWER_SERVER_SERVICE_END);

    if (tps_status == gatt_tps_status_success)
    {
        GATT_DEBUG(("GATT Tx Power Server initialised\n"));
        /* The size of TPS is also calculated and memory is alocated.
         * So advance the ptr so that the next Server while initializing.
         * shall not over write the same memory */
        *ptr += sizeof(GTPSS);
        return TRUE;
    }
    else
    {
        GATT_DEBUG(("GATT Tx Power Server init failed [%x]\n", tps_status));
    }
    return FALSE;
}

#endif /* GATT_TPS_SERVER */

