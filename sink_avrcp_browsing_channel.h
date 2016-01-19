#ifdef ENABLE_AVRCP
/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.

FILE NAME
    sink_avrcp_browsing_channel.h
    
DESCRIPTION
    Handles AVRCP Browsing Channel Connection/Disconnection.
    
*/

#ifndef _SINK_AVRCP_BROWSING_CHANNEL_H_
#define _SINK_AVRCP_BROWSING_CHANNEL_H_


#include <avrcp.h>

void sinkAvrcpBrowsingChannelConnectInd(AVRCP_BROWSE_CONNECT_IND_T *msg);

void sinkAvrcpBrowsingChannelDisconnectInd(AVRCP_BROWSE_DISCONNECT_IND_T *msg);

void sinkAvrcpBrowsingChannelDisconnectRequest(AVRCP *avrcp);

bool sinkAvrcpBrowsingChannelGetIndexFromInstance(AVRCP *avrcp, uint16 *Index);

/* initialisation */
void sinkAvrcpBrowsingChannelInit(bool all_links, uint16 link_index);

/* connection/disconnection */
void sinkAvrcpBrowsingChannelConnectRequest(AVRCP *avrcp);

void sinkAvrcpBrowsingChannelConnectCfm(AVRCP_BROWSE_CONNECT_CFM_T *msg);

/* Utility function to check if Browsing channel is connected */
bool sinkAvrcpBrowsingChannelIsConnected(uint16 index);

/* Utility function to check if Browsing channel is disconnected */
bool sinkAvrcpBrowsingChannelIsDisconnected(uint16 index);

/* Utility function to send the message on connection */
void sinkAvrcpBrowsingChannelSendMessageWhenConnected(Task task, MessageId id, void* message, uint16 index);

#endif /* _SINK_AVRCP_BROWSING_CHANNEL_H_ */

#endif /* ENABLE_AVRCP */

