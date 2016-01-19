/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.
*/

/*!
@file    sink_peer_qualification.h
@brief   Interface to handle Peer State machine to ensure PTS qualification for 
            A2DP Sorce functionality. 
*/

#ifndef _SINK_PEER_QUALIFICATION_H_
#define _SINK_PEER_QUALIFICATION_H_

#include "sink_private.h"
#include "sink_peer.h"

#include <message.h>
#include <app/message/system_message.h>
#include <a2dp.h>

#ifdef ENABLE_PEER
/******************************************************/
bool peerQualificationAdvanceRelayState (RelayEvent relay_event);
#else
#define peerQualificationAdvanceRelayState (relay_event) (FALSE)
#endif

#ifdef ENABLE_PEER
void handlePeerQualificationReconfigureCfm(uint16 DeviceId, uint16 StreamId, a2dp_status_code status);
#else
#define handlePeerQualificationReconfigureCfm(DeviceId, StreamId, status) ((void)0)
#endif

#ifdef ENABLE_PEER
void handlePeerQualificationEnablePeerOpen(void);
#else
#define handlePeerQualificationEnablePeerOpen() ((void)0)
#endif

#ifdef ENABLE_PEER
bool peerQualificationReplaceDelayReportServiceCaps (uint8 *dest_service_caps, uint16 *size_dest_service_caps, const uint8 *src_service_caps, uint16 size_src_service_caps);
#else
#define peerQualificationReplaceDelayReportServiceCaps (dest_service_caps, size_dest_service_caps, src_service_caps, size_src_service_caps) (FALSE)
#endif

#endif /* _SINK_PEER_QUALIFICATION_H_ */

