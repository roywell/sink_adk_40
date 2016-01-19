/*******************************************************************************
 Copyright Cambridge Silicon Radio Limited 2015
 Part of ADK 4.0
*******************************************************************************/



#ifndef _SINK_GATT_SERVER_H_
#define _SINK_GATT_SERVER_H_

#include <gatt_server.h>
#include <gatt_gap_server.h>
#include <gatt_battery_server.h>
#include <gatt_transmit_power_server.h>
#include <gatt_link_loss_server.h>
#include <gatt_imm_alert_server.h>

#include <csrtypes.h>
#ifdef GATT_ENABLED


/* Define for Server variables */
#define GATT_SERVER theSink.rundata->ble.gatt.server


typedef struct __gatt_server_t
{
    uint16      cid;                    /* For now, only support one connection */
    
    void        *servers_ptr;       /* Pointer to the memory block used to store the RUNDATA required by the GATT server tasks */
    GGATTS      *gatt_server;       /* Pointer to the "GATT Server" (library) RUNDATA */
    GGAPS       *gap_server;        /* Pointer to the "GAP Server" (library) RUNDATA */
    GBASS       *bas_server_local;  /* Pointer to the "Battery Server" (library) RUNDATA - internal local battery level */    
    GBASS       *bas_server_remote; /* Pointer to the "Battery Server" (library) RUNDATA - remote battery level */
    GBASS       *bas_server_peer;   /* Pointer to the "Battery Server" (library) RUNDATA - peer battery level */
    GLLSS_T     *lls_server;        /* Pointer to the "Link Loss Server" (library) RUNDATA - link loss service */    
} gatt_server_t;


/****************************************************************************
NAME    
    gattServerConnectionAdd
    
DESCRIPTION
    Allocates resource for a GATT server connection.
    
PARAMETERS
    cid             The connection ID
    
RETURNS    
    TRUE if the server connection was successfully added, FALSE otherwise.
*/
bool gattServerConnectionAdd(uint16 cid);


/****************************************************************************
NAME    
    gattServerConnectionFindByCid
    
DESCRIPTION
    Finds the GATT server connection by connection ID.
    
PARAMETERS
    cid             The connection ID
    
RETURNS    
    TRUE if the server connection is found, NULL otherwise.
*/
bool gattServerConnectionFindByCid(uint16 cid);


/****************************************************************************
NAME    
    gattServerConnectionRemove
    
DESCRIPTION
    Frees resource for a GATT server connection.
    
PARAMETERS
    cid             The connection ID
    
RETURNS    
    TRUE if the connection was found and removed. FALSE otherwise.    
*/
bool gattServerConnectionRemove(uint16 cid);


#endif /* GATT_ENABLED */


/****************************************************************************
NAME    
    gattServerDisconnectAll
    
DESCRIPTION
    Disconnects all server role connections.
    
PARAMETERS
    None
    
RETURNS    
    TRUE if a disconnect request was sent. FALSE otherwise.
*/
#ifdef GATT_ENABLED
bool gattServerDisconnectAll(void);
#else
#define gattServerDisconnectAll() (FALSE)
#endif


/****************************************************************************
NAME    
    gattServerIsFullyDisconnected
    
DESCRIPTION
    Gets if there are no connections active.
    
PARAMETERS
    connection      The GATT connection
    
RETURNS
    TRUE if no connections. FALSE otherwise.

*/
#ifdef GATT_ENABLED
bool gattServerIsFullyDisconnected(void);
#else
#define gattServerIsFullyDisconnected() (TRUE)
#endif


#endif /* _SINK_GATT_SERVER_H_ */

