/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2005 - 2015.
*/

/*!
@file    sink_avrcp_qualification.h
@brief   Interface to the PTS qualification for AVRCP qualification, which includes:
            - AVRCP TG CAT1 mandatory functionality and AVRCP CT CAT2 passthrough command.
*/

#ifndef _SINK_AVRCP_QUALIFICATION_H_
#define _SINK_AVRCP_QUALIFICATION_H_

#include "sink_private.h"

#include <message.h>
#include <app/message/system_message.h>
#include <avrcp.h>

#ifdef ENABLE_AVRCP
/* Initialization of AVRCP qualification */
void sinkAvrcpQualificationInit(void);
#else
#define sinkAvrcpQualificationInit() ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to handle all the TG CAT-1 mandatory request indications */
bool handleAvrcpQualificationTestCaseInd (Task task, MessageId id, Message message);
#else
#define handleAvrcpQualificationTestCaseInd (task, id, message) (FALSE)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to handle all the TG CAT-1 mandatory register of notifications */
bool handleAvrcpQualificationRegisterNotificationInd(
                                                            AVRCP_REGISTER_NOTIFICATION_IND_T  *msg);
#else
#define handleAvrcpQualificationRegisterNotificationInd(msg) (FALSE)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to check the "volume up" passthrough command capabilities of CAT-2 CT*/
void handleAvrcpQualificationVolumeUp (void);
#else
#define handleAvrcpQualificationVolumeUp () ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to check the "volume down" passthrough command capabilities of CAT-2 CT*/
void handleAvrcpQualificationVolumeDown (void);
#else
#define handleAvrcpQualificationVolumeDown () ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to set the status that a track is selected and playing. This is used as an intermediate step while testing 
 CAT-1 TG for EVENT_TRACK_CHANGED notification with CT */
void handleAvrcpQualificationPlayTrack (void);
#else
#define handleAvrcpQualificationPlayTrack () ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to send SetAbsoluteVolume for CAT-2 CT*/
void handleAvrcpQualificationSetAbsoluteVolume (void);
#else
#define handleAvrcpQualificationSetAbsoluteVolume () ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to handle VolumeChange Indication from TG CAT-2*/
void handleAvrcpVolumeChangedInd(AVRCP_EVENT_VOLUME_CHANGED_IND_T *msg);
#else
#define handleAvrcpVolumeChangedInd(msg) ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to handle SetAbsoluteVolumeCfm from TG CAT-2*/
void handleAvrcpSetAbsoluteVolumeCfm(AVRCP_SET_ABSOLUTE_VOLUME_CFM_T *msg);
#else
#define handleAvrcpSetAbsoluteVolumeCfm(msg) ((void)0)
#endif

#ifdef ENABLE_AVRCP
/* Utility function to configure AVRCP frame data size*/
void handleAvrcpQualificationConfigureDataSize(void);
#else
#define handleAvrcpQualificationConfigureDataSize() ((void)0)
#endif

#endif /* _SINK_AVRCP_QUALIFICATION_H_ */

