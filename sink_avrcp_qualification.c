
/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2004 - 2015.

FILE NAME
    sink_avrcp_qualification.c

DESCRIPTION
   Interface to the PTS qualification for AVRCP qualification, which includes:
  - AVRCP TG CAT1 mandatory functionality and AVRCP CT CAT2 passthrough command.
*/

#include "sink_avrcp_qualification.h"
#include "sink_avrcp.h"
#include "sink_debug.h"

#include <source.h>
#include <string.h>
#include <vm.h>


#ifdef ENABLE_AVRCP

#ifdef DEBUG_AVRCP_QUALIFICATION
#define AVRCP_QUALIFICATION_DEBUG(x) DEBUG(x)
#else
#define AVRCP_QUALIFICATION_DEBUG(x) 
#endif

#define AVRCP_AVC_QUALIFICATION_DATA_SIZE     30

SinkAvrcpCleanUpTask        dataCleanUpTask;

/*************************************************************************/
void sinkAvrcpQualificationInit(void)
{
     if(theSink.features.TwsQualificationEnable)
     {
        dataCleanUpTask.task.handler = sinkAvrcpDataCleanUp;
        dataCleanUpTask.data = NULL;
     }
 }

/*********************** AVRCP Library Message Handling Functions ***************************/
/*************************************************************************/
static void handleAvrcpQualificationSetAddressedPlayer(AVRCP_SET_ADDRESSED_PLAYER_IND_T *req)
{
    avrcp_response_type res_type = avrcp_response_rejected_invalid_player_id;
    AVRCP_QUALIFICATION_DEBUG(("Request for AVRCP_SET_ADDRESSED_PLAYER_IND\n"));
    if(req->player_id != 0xffff)
    {
        AVRCP_QUALIFICATION_DEBUG(("Request for AVRCP Addressed Played id %d\n", req->player_id));
        res_type = avctp_response_accepted;
    }
    AvrcpSetAddressedPlayerResponse(req->avrcp, res_type);
}

/*************************************************************************/
static void handleAvrcpQualificationGetFolderItems(AVRCP_BROWSE_GET_FOLDER_ITEMS_IND_T *req)
{
    Source src_pdu=0;
    avrcp_response_type res_type = avrcp_response_rejected_invalid_scope;
    uint16 num_items=0;
    uint16 item_list_size=0;
    uint8 *item=NULL;

    if(req->scope == avrcp_media_player_scope)
    {
        res_type = avrcp_response_browsing_success;

        item = PanicUnlessMalloc(35);
        memset(item, 0, 35);
        item[0] = 0x01; /* Media Player Item */
        item[2] = 0x20; /* Item length */
        item[3] = 0x00; 
        item[4] = 0x01;/* Player ID */
        item[5] = 0x01; /* Player Type */
        item[9] = 0x01;/* Sub-type */
        item[11] = 0x01;/* Play Status */
        item[15] = 0x60; /* Feature Bit mask */
        item[16] = 0xb7;
        item[17] = 0x01;
        item[18] = 0x02;
        item[30] = 0x04;
        strncpy((char*)&item[31], "CSR", 4);
        
        item_list_size = 35;
        num_items = 1;
        src_pdu = sinkAvrcpSourceFromData(&dataCleanUpTask, item, item_list_size);
    }

    AvrcpBrowseGetFolderItemsResponse(req->avrcp, res_type, 0, num_items, item_list_size, src_pdu);
}

/*************************************************************************/
static void handleAvrcpQualificationGetTotalNumberOfItems(AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND_T *req)
{
    avrcp_response_type res_type = avrcp_response_rejected_invalid_scope;
    uint16 num_items=0;

    if(req->scope == avrcp_media_player_scope)
    {
        res_type = avrcp_response_browsing_success;
        num_items = 1;
    }

    AvrcpBrowseGetNumberOfItemsResponse(req->avrcp, res_type, 0, num_items);
}

/*************************************************************************/
static void handleAvrcpQualificationGetPlayStatus(AVRCP_GET_PLAY_STATUS_IND_T  *req)
{
    AVRCP_QUALIFICATION_DEBUG(("Recieved metadata ind\n"));
    AvrcpGetPlayStatusResponse(req->avrcp, avctp_response_stable, 0, 0, 0x01);
}

/*************************************************************************/
static void handleAvrcpQualificationGetElementAttribute(AVRCP_GET_ELEMENT_ATTRIBUTES_IND_T  *req)
{
    Source src_pdu;
    uint16 attr_len_size;
    uint8* attr_data=NULL;
    uint16 num_items=0;

    /* Allocate 64 bytes of memory */
    attr_data = (uint8*)PanicUnlessMalloc(64);
    memset(attr_data, 0, 64);
   
    attr_data[3] =0x1; /* Attribute ID */
    attr_data[5] = 0x6A; /* Character Set - UTF-8 */
    attr_data[7] = 0x04;/* Attribute length */
    strncpy((char*)&attr_data[8], "CSR", 4);

    attr_data[15] =0x1; /* Attribute ID */
    attr_data[17] = 0x6A; /* Character Set - UTF-8 */
    attr_data[19] = 0x2c;/* Attribute length 44  bytes*/
    strncpy((char*)&attr_data[20], "PTS-QUALIFICATION-FOR-TG-FRAGMENTATION-TEST", 44);

    
    /* size of attribute values */
    attr_len_size = 64;
    num_items =2;
    src_pdu = sinkAvrcpSourceFromData(&dataCleanUpTask, attr_data, attr_len_size);
    
    AvrcpGetElementAttributesResponse(req->avrcp, avctp_response_stable, num_items, attr_len_size, src_pdu);
}


/*************************************************************************
NAME    
    handleAvrcpQualificationTestCaseInd
    
DESCRIPTION
    Handles all the mandotary TG CAT-1 indications.

**************************************************************************/
bool handleAvrcpQualificationTestCaseInd (Task task, MessageId id, Message message)
{
    /* This entire handling shall come PS Key PTS enable Bit */
    if(!theSink.features.TwsQualificationEnable)
    {
        return FALSE;
    }
    
    switch(id)
    {
        case AVRCP_SET_ADDRESSED_PLAYER_IND:
        {
            handleAvrcpQualificationSetAddressedPlayer((AVRCP_SET_ADDRESSED_PLAYER_IND_T *)message);
        }
        break;
        case AVRCP_BROWSE_GET_FOLDER_ITEMS_IND:
        {
            handleAvrcpQualificationGetFolderItems((AVRCP_BROWSE_GET_FOLDER_ITEMS_IND_T *)message);
        }
        break;
        case AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND:
        {
            handleAvrcpQualificationGetTotalNumberOfItems((AVRCP_BROWSE_GET_NUMBER_OF_ITEMS_IND_T *)message);
        }
        break;
        case AVRCP_GET_PLAY_STATUS_IND:
        {
            handleAvrcpQualificationGetPlayStatus((AVRCP_GET_PLAY_STATUS_IND_T *)message);
        }
        break;
        case AVRCP_GET_ELEMENT_ATTRIBUTES_IND:
        {
            handleAvrcpQualificationGetElementAttribute((AVRCP_GET_ELEMENT_ATTRIBUTES_IND_T *)message);
        }
        break;
        default:
        break;
    }

    return TRUE;
}


/*************************************************************************
NAME    
    handleAvrcpQualificationRegisterNotificationInd
    
DESCRIPTION
    Functionality as a result of receiving AVRCP_REGISTER_NOTIFICATION_IND from the AVRCP library
    for mandatory TG CAT-1

**************************************************************************/
bool handleAvrcpQualificationRegisterNotificationInd(
                                                                    AVRCP_REGISTER_NOTIFICATION_IND_T *msg)
{
    uint16 Index;

    /* This entire handling shall come PS Key PTS enable Bit */
    if(!theSink.features.TwsQualificationEnable)
    {
        return FALSE;
    }
    AVRCP_QUALIFICATION_DEBUG(("   event_id [%d]\n", msg->event_id));
    
    switch (msg->event_id)
    {
        case avrcp_event_track_changed:
            if (sinkAvrcpGetIndexFromInstance(msg->avrcp, &Index))
            {
                uint32              track_index_high = 0xffffffff;
                uint32              track_index_low = 0xffffffff;
                theSink.avrcp_link_data->registered_events[Index] |= (1 << msg->event_id);
                AVRCP_QUALIFICATION_DEBUG(("   (avrcp_event_track_changed)  registered events [%d] index [%d]\n", theSink.avrcp_link_data->registered_events[Index], Index));
                /* If a track is already playing or selected, then CT expects 0x0 in the interim response 
                  * NOTE: CT shall ask to put the TG in track playing or select track. To set this, 
                  * generate the user event EventUsrTwsQualificationPlayTrack */
                if(theSink.avrcp_link_data->play_status[Index] == avrcp_play_status_playing)
                {
                    track_index_high = 0;
                    track_index_low = 0;
                    /* Reset */
                    theSink.avrcp_link_data->play_status[Index] = avrcp_play_status_stopped;
                }
                /* Register for EVENT_TRACK_CHANGED notification */
                AvrcpEventTrackChangedResponse(theSink.avrcp_link_data->avrcp[Index], 
                                                        avctp_response_interim,
                                                        track_index_high, track_index_low);
                /* Send track changed notification after registration for EVENT_TRACK_CHANGED notification */
                AvrcpEventTrackChangedResponse(theSink.avrcp_link_data->avrcp[Index], 
                                                        avctp_response_changed,
                                                        0, 1);
            }
            break;
        /* AVRCP TG for Addressed Player */
        case avrcp_event_addressed_player_changed:
            if (sinkAvrcpGetIndexFromInstance(msg->avrcp, &Index))
            {
                theSink.avrcp_link_data->registered_events[Index] |= (1 << msg->event_id);
                AVRCP_QUALIFICATION_DEBUG(("   (avrcp_event_addressed_player_changed)  registered events [%d] index [%d]\n", theSink.avrcp_link_data->registered_events[Index], Index));
                AvrcpEventAddressedPlayerChangedResponse(theSink.avrcp_link_data->avrcp[Index], 
                                                        avctp_response_interim,
                                                        1, 0);
            }
            break;
        default:
            /* other registrations should not be received */
            break;
    }
    return TRUE;
}

/*************************************************************************
NAME    
    handleAvrcpQualificationVolumeUp
    
DESCRIPTION
    Send an AVRCP_VOLUME_UP to the device with the currently active AVRCP connection.

**************************************************************************/
void handleAvrcpQualificationVolumeUp (void)
{
    uint16 Index = sinkAvrcpGetActiveConnection();
    
    AVRCP_QUALIFICATION_DEBUG(("AVRCP: Avrcp Volume Up\n"));
    if(theSink.features.TwsQualificationEnable)
    {
        if (theSink.avrcp_link_data->connected[Index] && (getAvrcpQueueSpace(Index) >= 2))
        {
            avrcpSendControlMessage(AVRCP_CTRL_VOLUME_UP_PRESS, Index);
            avrcpSendControlMessage(AVRCP_CTRL_VOLUME_UP_RELEASE, Index);
        }
    }
}

/*************************************************************************
NAME    
    handleAvrcpQualificationVolumeDown
    
DESCRIPTION
    Send an AVRCP_VOLUME_DOWN to the device with the currently active AVRCP connection.

**************************************************************************/
void handleAvrcpQualificationVolumeDown (void)
{
    uint16 Index = sinkAvrcpGetActiveConnection();
    
    AVRCP_QUALIFICATION_DEBUG(("AVRCP: Avrcp Volume Up\n"));

    if(theSink.features.TwsQualificationEnable)
    {
        if (theSink.avrcp_link_data->connected[Index] && (getAvrcpQueueSpace(Index) >= 2))
        {
            avrcpSendControlMessage(AVRCP_CTRL_VOLUME_DOWN_PRESS, Index);
            avrcpSendControlMessage(AVRCP_CTRL_VOLUME_DOWN_RELEASE, Index);
        }
    }
}

/*************************************************************************
NAME    
    handleAvrcpQualificationPlayTrack
    
DESCRIPTION
    When CT asks for putting the TG for playing a track or select a track, then this utility API is 
    called to set the playing status.

**************************************************************************/
void handleAvrcpQualificationPlayTrack (void)
{
    uint16 Index = sinkAvrcpGetActiveConnection();
    
    AVRCP_QUALIFICATION_DEBUG(("AVRCP: Avrcp Play\n"));

    if(theSink.features.TwsQualificationEnable)
    {
        if (theSink.avrcp_link_data->connected[Index])
        {
            /* Based on the status below, EVENT_TRACK_CHANGED response
                shall be appropriately set while sending interim response to CT*/
            theSink.avrcp_link_data->play_status[Index] = avrcp_play_status_playing;
        }
    }
}

/*************************************************************************
NAME 
    handleAvrcpQualificationSetAbsoluteVolume
    
DESCRIPTION
    Send an AVRCP_SET_ABSOLUTE_VOLUME_PDU_ID to the device with the currently active AVRCP connection.

**************************************************************************/
void handleAvrcpQualificationSetAbsoluteVolume (void)
{
    uint16 Index = sinkAvrcpGetActiveConnection();
    uint8 volume;
    
    AVRCP_QUALIFICATION_DEBUG(("AVRCP: Avrcp Set Absolute Volume\n"));

    if(theSink.features.TwsQualificationEnable)
    {
        if (theSink.avrcp_link_data->connected[Index] && (getAvrcpQueueSpace(Index) >= 2))
        {
            volume = 0x40; /* 50% */
            AvrcpSetAbsoluteVolumeRequest(theSink.avrcp_link_data->avrcp[Index], volume);             
        }
    }
}

/*************************************************************************
NAME    
    handleAvrcpVolumeChangedInd
    
DESCRIPTION
    Functionality as a result of receiving AVRCP_EVENT_VOLUME_CHANGED_IND from the AVRCP library.

**************************************************************************/
void handleAvrcpVolumeChangedInd(AVRCP_EVENT_VOLUME_CHANGED_IND_T *msg)
{
    uint16 Index;

    /* This entire handling shall come PS Key PTS enable Bit */
    if(!theSink.features.TwsQualificationEnable)
    {
        return;
    }
    
    AVRCP_QUALIFICATION_DEBUG(("   volume changed ind [%x] volume[0x%x]\n", msg->response, msg->volume));
    
    if (sinkAvrcpGetIndexFromInstance(msg->avrcp, &Index))
    {
        if ((msg->response == avctp_response_changed) || (msg->response == avctp_response_interim))
        {                    
            if (msg->response == avctp_response_changed)
            {
                /* re-register to receive notifications */
                AvrcpRegisterNotificationRequest(msg->avrcp, avrcp_event_volume_changed, 0);                 
            }
        }     
        else
        {
            /* assume not supported by remote device */
            theSink.avrcp_link_data->event_capabilities[Index] &= ~(1 << avrcp_event_volume_changed);
        }
    }
}

/*************************************************************************
NAME    
    handleAvrcpSetAbsoluteVolumeCfm
    
DESCRIPTION
    Functionality as a result of receiving AVRCP_SET_ABSOLUTE_VOLUME_CFM from the AVRCP library.

**************************************************************************/
void handleAvrcpSetAbsoluteVolumeCfm(AVRCP_SET_ABSOLUTE_VOLUME_CFM_T *msg)
{
    uint16 Index;

    /* This entire handling shall come PS Key PTS enable Bit */
    if(!theSink.features.TwsQualificationEnable)
    {
        return;
    }
                
    AVRCP_QUALIFICATION_DEBUG(("   absolute value cfm [%x] [%x]\n", msg->status, msg->volume));
    
    if ((msg->status == avrcp_success) && sinkAvrcpGetIndexFromInstance(msg->avrcp, &Index))
    {
        theSink.avrcp_link_data->absolute_volume[Index] = msg->volume;
    }
}

/*************************************************************************
NAME    
    handleAvrcpQualificationConfigureDataSize
    
DESCRIPTION
    Functionality to configure AVRCP frame data size from default AVRCP_AVC_MAX_DATA_SIZE
    to user defined size.

**************************************************************************/
void handleAvrcpQualificationConfigureDataSize(void)
{
    uint16 Index = sinkAvrcpGetActiveConnection();
    
    AVRCP_QUALIFICATION_DEBUG(("AVRCP: Configure AVRCP data size\n"));

    if(theSink.features.TwsQualificationEnable)
    {
        if (theSink.avrcp_link_data->connected[Index] && (getAvrcpQueueSpace(Index) >= 2))
        {
            AvrcpSetMetadataResponsePDUDataSize(theSink.avrcp_link_data->avrcp[Index], AVRCP_AVC_QUALIFICATION_DATA_SIZE);
        }
    }   
}
    
#endif /* ENABLE_AVRCP*/

