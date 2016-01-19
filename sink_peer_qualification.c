/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2004 - 2015.

FILE NAME
    sink_peer_qualification.c

DESCRIPTION
     Interface to handle Peer State machine to ensure PTS qualification for A2DP Sorce functionality
*/

#include "sink_peer_qualification.h"
#include "sink_a2dp.h"
#include "sink_debug.h"

#include <source.h>
#include <string.h>
#include <vm.h>

#ifdef ENABLE_PEER

#ifdef DEBUG_A2DP_QUALIFICATION
#define A2DP_QUALIFICATION_DEBUG(x) DEBUG(x)
#else
#define A2DP_QUALIFICATION_DEBUG(x) 
#endif

#define SERVICE_CAPS(caps) ((caps)[0])
#define CAPS_SIZE(caps)    ((caps)[1] + 2)

uint8* gdest_service_caps = NULL;

/*************************************************************************
NAME    
    findDelayReportServiceCaps
    
DESCRIPTION
    Utility function to find if the configured AG's Cap has delay reporting support.

**************************************************************************/
static uint16 findDelayReportServiceCaps (const uint8 *codec_caps, uint16 size_codec_caps)
{
    while (codec_caps && (size_codec_caps > 1))
    {
        uint16 size;

        if ((SERVICE_CAPS(codec_caps) < AVDTP_SERVICE_MEDIA_TRANSPORT) || (SERVICE_CAPS(codec_caps) > AVDTP_SERVICE_DELAY_REPORTING))
        {
            return 0;
        }

        if ((size = CAPS_SIZE(codec_caps)) > size_codec_caps)
        {
            return 0;
        }

        if (SERVICE_CAPS(codec_caps) == AVDTP_SERVICE_DELAY_REPORTING)
        {
            return CAPS_SIZE(codec_caps);
        }

        size_codec_caps -= size;
        codec_caps += size;
    }

    return 0;
}

/*************************************************************************
NAME    
    peerQualificationReplaceDelayReportServiceCaps
    
DESCRIPTION
    Utility function to replace PTS configured caps with AG's configured caps by removing delay 
    reporting, provided AG supports one.

**************************************************************************/
bool peerQualificationReplaceDelayReportServiceCaps (uint8 *dest_service_caps, uint16 *size_dest_service_caps, const uint8 *src_service_caps, uint16 size_src_service_caps)
{
    uint16 size;
    if (!src_service_caps || !size_src_service_caps || !theSink.features.TwsQualificationEnable)
    {
        return FALSE;
    }
    else
    {
        if((size = findDelayReportServiceCaps(src_service_caps, size_src_service_caps)) != 0) 
        {
            if((dest_service_caps != NULL) && (size_dest_service_caps != NULL))
            {
                /* size of dest_service_caps is always allocated to size_src_service_caps */
                memset(dest_service_caps, 0, size_src_service_caps);
            	*size_dest_service_caps = size_src_service_caps - size;
                memcpy(dest_service_caps, src_service_caps, (size_src_service_caps -size));
            	return TRUE;
        	}
    	}
    }
    
    return FALSE;
}

/*************************************************************************
NAME    
    findMediaTransportServiceCaps
    
DESCRIPTION
    Utility function to find if the configured AG's Cap has media transport support.

**************************************************************************/
static uint16 findMediaTransportServiceCaps (const uint8 *codec_caps, uint16 size_codec_caps)
{
    while (codec_caps && (size_codec_caps > 1))
    {
        uint16 size;

        if ((SERVICE_CAPS(codec_caps) < AVDTP_SERVICE_MEDIA_TRANSPORT) || (SERVICE_CAPS(codec_caps) > AVDTP_SERVICE_DELAY_REPORTING))
        {
            return 0;
        }

        if ((size = CAPS_SIZE(codec_caps)) > size_codec_caps)
        {
            return 0;
        }

        if (SERVICE_CAPS(codec_caps) == AVDTP_SERVICE_MEDIA_TRANSPORT)
        {
            return CAPS_SIZE(codec_caps);
        }

        size_codec_caps -= size;
        codec_caps += size;
    }

    return 0;
}

/*************************************************************************
NAME    
    peerQualificationReplaceMediaTransportServiceCaps
    
DESCRIPTION
    Utility function to replace PTS configured caps with AG's configured caps by removing media 
    transport, provided AG supports one.

**************************************************************************/
static bool peerQualificationReplaceMediaTransportServiceCaps (uint8 *dest_service_caps, uint16 *size_dest_service_caps, const uint8 *src_service_caps, uint16 size_src_service_caps)
{
    uint16 size;
    if (!src_service_caps || !size_src_service_caps || !theSink.features.TwsQualificationEnable)
    {
        return FALSE;
    }
    else
    {
        if((size = findMediaTransportServiceCaps(src_service_caps, size_src_service_caps)) != 0) 
        {
            if((dest_service_caps != NULL) && (size_dest_service_caps != NULL))
            {
                /* size of dest_service_caps is always allocated to size_src_service_caps */
                memset(dest_service_caps, 0, size_src_service_caps);
            	*size_dest_service_caps = size_src_service_caps - size;
                memcpy(dest_service_caps, (src_service_caps+size), (size_src_service_caps -size));
            	return TRUE;
        	}
    	}
    }
    
    return FALSE;
}

/*************************************************************************
NAME    
    a2dpIssuePeerReconfigureRequest
    
DESCRIPTION
    Issues a request to reconfigure the relay channel to the currently connected Peer

RETURNS
    TRUE if request issued, FALSE otherwise

**************************************************************************/
static bool a2dpIssuePeerReconfigureRequest (void)
{
    uint16 peer_id;
    a2dp_link_priority priority;
    a2dp_codec_settings* codec_settings = NULL;
    uint16 size_dest_service_caps = 0;
    uint8 *find_media_transport = NULL;
    uint8 *config_caps = NULL;
    uint16 size_of_conf_caps = 0;
    uint16 size_of_transport = 0;
    
    if (a2dpGetPeerIndex(&peer_id))
    {
        A2DP_QUALIFICATION_DEBUG(("issuePeerReconfigureRequest peer=%u\n",peer_id));
        if (findCurrentA2dpSource( &priority ))
        {
            codec_settings = A2dpCodecGetSettings(theSink.a2dp_link_data->device_id[priority], theSink.a2dp_link_data->stream_id[priority]);
        }

        if(codec_settings)
        {
            /* Allocate the memory for the dest_service_caps, atleast equal to the configured caps */
            gdest_service_caps = PanicUnlessMalloc(codec_settings->size_configured_codec_caps);
            find_media_transport = PanicUnlessMalloc(codec_settings->size_configured_codec_caps);
            config_caps = codec_settings->configured_codec_caps;
            size_of_conf_caps = codec_settings->size_configured_codec_caps;

             /* If at all there is media_transport in the configured caps, then remove it and copy it to dest_caps */
             if(peerQualificationReplaceMediaTransportServiceCaps(find_media_transport, &size_of_transport, config_caps, size_of_conf_caps))
             {
                memcpy(gdest_service_caps, find_media_transport, size_of_transport);
                size_dest_service_caps = size_of_transport;
                config_caps = find_media_transport;
                size_of_conf_caps = size_of_transport;
             }
             /* Remove any delay_reporting, and update the dest_caps */
             peerQualificationReplaceDelayReportServiceCaps(gdest_service_caps, &size_dest_service_caps, config_caps, size_of_conf_caps);
            
            free(find_media_transport);
            find_media_transport = NULL;

            /* If the configured caps has no media_transport or delay_reporting use it as is for reconfiguration */
            if(size_dest_service_caps == 0)
            {
                free(gdest_service_caps);
                gdest_service_caps = NULL;
            	return A2dpMediaReconfigureRequest(theSink.a2dp_link_data->device_id[peer_id], theSink.a2dp_link_data->stream_id[peer_id], codec_settings->size_configured_codec_caps, codec_settings->configured_codec_caps);
        	}
            /* gdest_service_caps pointer shall be de-allocated in response of AVDTP_RECONFIGURE */
        	return A2dpMediaReconfigureRequest(theSink.a2dp_link_data->device_id[peer_id], theSink.a2dp_link_data->stream_id[peer_id], size_dest_service_caps, gdest_service_caps);
    	}
    }
    
    return FALSE;
}

/*************************************************************************
NAME    
    peerQualificationAdvanceRelayState
    
DESCRIPTION
    This function is to bypass Peer state machine so that we can handle PTS request/reponse.

**************************************************************************/
bool peerQualificationAdvanceRelayState (RelayEvent relay_event)
{
    bool retVal = FALSE;
    if(!theSink.features.TwsQualificationEnable)
    {
        return retVal;
    }
    
    switch (relay_event)
    {
        case RELAY_EVENT_CONNECTED:
            A2DP_QUALIFICATION_DEBUG(("peerAdvanceRelayState : RELAY_EVENT_CONNECTED\n"));
            updateCurrentState( RELAY_STATE_IDLE );
            /* for some TC's PTS expects SRC to open the channel.
                With this flag, ADK shall trigger opening of the media channel,
                provided AG is already connected and is streaming */
            if(theSink.tws_qual_enable_peer_open)
            {
                if (a2dpIssuePeerOpenRequest())
                {
                    A2DP_QUALIFICATION_DEBUG(("PEER: Issue PEER OPEN REQ\n"));
                    updateCurrentState( RELAY_STATE_OPENING );
                }
            }
            retVal = TRUE;
            break;
            
        case RELAY_EVENT_OPEN:
            A2DP_QUALIFICATION_DEBUG(("peerAdvanceRelayState : RELAY_EVENT_OPEN\n"));
            updateCurrentState( RELAY_STATE_IDLE );
            retVal = TRUE;
            break;
            
        
        case RELAY_EVENT_SUSPENDED:
            A2DP_QUALIFICATION_DEBUG(("peerAdvanceRelayState : RELAY_EVENT_SUSPENDED\n"));
            updateCurrentState( RELAY_STATE_OPEN );
            if (theSink.peer.target_state < theSink.peer.current_state)
            {   /* reconfigure relay stream */
                if(a2dpIssuePeerReconfigureRequest())
                {
                    A2DP_QUALIFICATION_DEBUG(("PEER: Issue PEER RECONFIGURE REQ\n"));
                    updateCurrentState(RELAY_STATE_IDLE);
                }
            }
            retVal = TRUE;
            break;

        case RELAY_EVENT_NOT_OPENED:
        {
            retVal = TRUE;
        }
        break;
        default:
            retVal = FALSE;
            break;
    }
    return retVal;
}

/*************************************************************************
NAME    
    handlePeerQualificationReconfigureCfm
    
DESCRIPTION
    handle a successful confirm of a media channel reconfiguration

RETURNS
    
**************************************************************************/
void handlePeerQualificationReconfigureCfm(uint16 DeviceId, uint16 StreamId, a2dp_status_code status)
{

    if(!theSink.features.TwsQualificationEnable)
        return;
        
    if(gdest_service_caps)
    {
        free(gdest_service_caps);
        gdest_service_caps = NULL;
    }
    
    /* ensure successful confirm status */
    if (status == a2dp_success)
    {
        uint16 Id;
        bdaddr bd_addr;

        /*Get the A2DP index from the BD Address corresponding to the DeviceId */
        if(A2dpDeviceGetBdaddr(DeviceId, &bd_addr) && getA2dpIndexFromBdaddr(&bd_addr , &Id))
        {
            if (theSink.a2dp_link_data->peer_device[Id] == remote_device_peer)
            {
                peerAdvanceRelayState(RELAY_EVENT_START);
            }
        }
    }
   else
   {
        uint16 Id;
        if (getA2dpIndex(DeviceId, &Id))
        {
            if (theSink.a2dp_link_data->peer_device[Id] == remote_device_peer)
            {
                peerAdvanceRelayState(RELAY_EVENT_NOT_OPENED);
            }
        }
    }
}

/*************************************************************************
NAME    
    handlePeerQualificationEnablePeerOpen
    
DESCRIPTION
    Utility function to toggle the "Enable Peer Open" flag. This flag is used to send stream open to
    PTS. For some TC's PTS expects the SRC to open the channel.
    
**************************************************************************/
void handlePeerQualificationEnablePeerOpen(void)
{
    if(theSink.features.TwsQualificationEnable)
    {
        if(theSink.tws_qual_enable_peer_open)
        {
            theSink.tws_qual_enable_peer_open = FALSE;
        }
        else
        {
            theSink.tws_qual_enable_peer_open = TRUE;
        }
    }
}

#endif /* ENABLE_PEER */

