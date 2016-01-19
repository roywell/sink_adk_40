/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2004 - 2015.

FILE NAME
    sink_audio.h
    
DESCRIPTION
    
*/

#ifndef _SINK_AUDIO_ROUTING_H_
#define _SINK_AUDIO_ROUTING_H_


#include "sink_private.h"

/* structure of audio sources statuses, used once by audio routing to reduce
   stack usage */
typedef struct
{
    Sink audio_routed;              /* currently routed audio */
    Sink sinkAG1;                   /* SCO sink of AG1 */
    Sink sinkAG2;                   /* SCO sink of AG2 */
    hfp_call_state stateAG1;        /* call state of AG1 */
    hfp_call_state stateAG2;        /* call state of AG2 */
    Sink a2dpSinkPri;               /* A2dp primary audio sink */
    Sink a2dpSinkSec;               /* A2dp secondary audio sink */
    a2dp_stream_state a2dpStatePri; /* A2dp primary audio state */
    a2dp_stream_state a2dpStateSec; /* A2dp secondary audio state */  
    a2dp_role_type a2dpRolePri;     /* Role (source/sink) of A2DP primary sink */
    a2dp_role_type a2dpRoleSec;     /* Role (source/sink) of A2DP secondary sink */
}audio_source_status;    

typedef enum
{
    audio_source_none = 0,
    audio_source_FM,
    audio_source_ANALOG,
    audio_source_SPDIF,
    audio_source_USB,
    audio_source_AG1,
    audio_source_AG2,
    audio_source_end_of_list
}audio_sources;

typedef enum
{
    audio_gate_none       = 0x00,
    audio_gate_call       = 0x01,
    audio_gate_party      = 0x02,
    audio_gate_a2dp       = 0x04,
    audio_gate_usb        = 0x08,
    audio_gate_wired      = 0x10,
    audio_gate_fm         = 0x20,
    audio_gate_multimedia = 0x3E,   /* All but audio_gate_call, audio_gate_sco and audio_gate_relay */
    audio_gate_sco        = 0x40,
    audio_gate_relay      = 0x80,
    audio_gate_noncall    = 0x8E,   /* All but audio_gate_call */
    audio_gate_all        = 0xFF
}audio_gating;

/* intended as a clean mechanism to determine what audio is routed */
typedef enum
{
    audio_route_none            	= 0x00,
    audio_route_a2dp_primary		= 0x01,
    audio_route_a2dp_secondary		= 0x02,
    audio_route_hfp_primary			= 0x04,
    audio_route_hfp_secondary		= 0x08,
    audio_route_usb					= 0x10,
    audio_route_analog				= 0x20,
    audio_route_spdif				= 0x40,
    audio_route_fm					= 0x80
} audio_route_available;

audio_gating audioGateAudio (uint16 audio_gated_mask);
audio_gating audioUngateAudio (uint16 audio_ungated_mask);

#ifdef ENABLE_SOUNDBAR
/****************************************************************************
NAME    
    audioSwitchToAudioSource
    
DESCRIPTION
	Switch audio routing to the source passed in, it may not be possible
    to actually route the audio at that point until the audio sink becomes
    available. 
    
    If the audio is not routed, it will be queued for routing unless cancelled,
    un-comment line to cancel the queuing feature

RETURNS
    TRUE if audio routed, FALSE id not possible to route audio
*/
bool audioSwitchToAudioSource(audio_sources source);

/****************************************************************************
NAME    
    audioSwitchToNextAudioSource
    
DESCRIPTION
	attempt to cycle to the next available audio source, if a source
    isn't available then cycle round until one is found, if no audio sources
    are available then stop

RETURNS
    none
*/
void audioSwitchToNextAudioSource(void);
#endif

/****************************************************************************
NAME    
    audioHandleRouting
    
DESCRIPTION
	Handle the routing of the audio connections or connection based on
    sco priority level, can specify a required source for soundbar type apps

RETURNS
    TRUE if audio routed correctly, FALSE if no audio available to route 
*/
bool audioHandleRouting (audio_sources required_source);

/****************************************************************************
NAME    
    audioRouteSource
    
DESCRIPTION
	attempt to route the audio for the passed in source

RETURNS
    TRUE if audio routed correctly, FALSE if no audio available yet to route 
*/
bool audioRouteSource(audio_sources source, audio_source_status * lAudioStatus);

#ifdef ENABLE_SUBWOOFER        
/****************************************************************************
NAME    
    audioCheckSubwooferConnection
    
DESCRIPTION
	check the sub woofer connection and streaming status and reconnect it if necessary

RETURNS
    TRUE if sub woofer reconnected, FALSE if no action has been taken
*/
bool audioCheckSubwooferConnection(bool okToConnectSub);
#endif
        
/****************************************************************************
NAME    
    audioRouteUsbIfAvailable
    
DESCRIPTION
    checks for a usb stream being present and routes it if present
    
RETURNS
    true if usb routed, false if no usb routable
*/
bool audioRouteUsbIfAvailable(audio_source_status * lAudioStatus);

/****************************************************************************
NAME    
    audioRouteA2dpStreamIfAvailable
    
DESCRIPTION
    checks for any a2dp streams being present and routes them if present, if no
    streaming connections check for suspended streams and resume them
    
RETURNS
    true if a2dp routed, false if no a2dp routable
*/
bool audioRouteA2dpStreamIfAvailable(audio_source_status * lAudioStatus, a2dp_link_priority priority);

/****************************************************************************
NAME    
    audioRouteActiveCallScoIfAvailable
    
DESCRIPTION
    checks for any sco being present, check whether there is a corresponding
    active call and route it based on its priority. check whether sco is already
    routed or whether a different audio source needs to be suspended/disconnected
    
RETURNS
    true if sco routed, false if no sco routable
*/
bool audioRouteActiveCallScoIfAvailable(audio_source_status * lAudioStatus, hfp_link_priority priority);

/****************************************************************************
NAME    
    audioRelaySourceStream
    
DESCRIPTION
    Checks for a Peer relay (source) stream and asks DSP to relay audio from any active AV Source
    
RETURNS
    true if audio relayed, false otherwise
*/
bool audioRelaySourceStream (void);

/****************************************************************************
NAME    
    audioRouteFMIfAvailable
    
DESCRIPTION
    checks for an fm audio stream being present and routes it if present
  
    
RETURNS
    true if fm routed, false if no fm routed
*/
bool audioRouteFMIfAvailable(audio_source_status * lAudioStatus);

/****************************************************************************
NAME    
    audioRouteScoIfAvailable
    
DESCRIPTION
    checks for any sco being present without any call indications
    
RETURNS
    true if sco routed, false if no sco routable
*/
bool audioRouteScoIfAvailable(audio_source_status * lAudioStatus);

/****************************************************************************
NAME    
    audioSuspendDisconnectSource
    
DESCRIPTION
    determines source of sink passed in and decides whether to issue a suspend or
    not based on source type, an audio disconnect is performed thereafter regardless
    of wether or not the source requires a suspend
    
RETURNS
    none
*/
bool audioSuspendDisconnectSource(audio_source_status * lAudioStatus);


/****************************************************************************
NAME    
    audioConnectScoSink
    
DESCRIPTION
    Route audio from a given SCO sink

RETURNS
    
*/
void audioConnectScoSink(hfp_link_priority priority, Sink sink);

/****************************************************************************
NAME    
    audioDisconnectActiveSink
    
DESCRIPTION
    Disconnect the active audio sink

RETURNS
    
*/
void audioDisconnectActiveSink(void);

/****************************************************************************
NAME    
    audioUpdateDisplayAmp
    
DESCRIPTION
    update the display if applicable and control the audio amp pio
    
RETURNS
    none
*/
void audioUpdateDisplayAmp(Sink audio_routed, audio_source_status * lAudioStatus);


/****************************************************************************
NAME    
    audioGetStatus
    
DESCRIPTION
    malloc slot and get status of sco/calls and a2do links, significantly
    reduces stack usage at the expense of a temporary slot use

RETURNS
    ptr to structure containing audio statuses
*/
audio_source_status * audioGetStatus(Sink routed_audio);

/****************************************************************************
NAME    
    audioRouteAnalogIfAvailable
    
DESCRIPTION
    checks for an analog audio stream being present and routes it if present
    
RETURNS
    true if analog audio routed, false if no analog audio routable
*/
bool audioRouteAnalogIfAvailable(audio_source_status * lAudioStatus);

/****************************************************************************
NAME    
    audioRouteSpdifIfAvailable
    
DESCRIPTION
    checks for an spdif audio stream being present and routes it if present
    
RETURNS
    true if spdif audio routed, false if no spdif audio routable
*/
bool audioRouteSpdifIfAvailable(audio_source_status * lAudioStatus);
        
/****************************************************************************
NAME    
    sinkAudioRouteAvailable
    
DESCRIPTION
    returns which audio source is routed. Only the route of highest priority is 
    returned. The priority starting at the top of the enum audio_route_available
    
RETURNS
    audio_route_available
*/
audio_route_available sinkAudioRouteAvailable(void);



#ifdef ENABLE_SOUNDBAR
#ifdef ENABLE_SUBWOOFER

/****************************************************************************
NAME    
    audioSuspendDisconnectAllA2dpMedia
    
DESCRIPTION
    called when the SUB link wants to use an ESCO link, there is not enough
    link bandwidth to support a2dp media and esco links so suspend or disconnect
    all a2dp media streams
    
RETURNS
    true if audio disconnected, false if no action taken
*/
bool audioSuspendDisconnectAllA2dpMedia(audio_source_status * lAudioStatus);

#endif
#endif

#ifdef ENABLE_SOUNDBAR
#ifdef ENABLE_AVRCP
/****************************************************************************
NAME    
    SetSoundbarAVRCPActiveSource
    
DESCRIPTION
    called when manually selecting AG1 or AG2 input, this function determines
    if it is possible to ascertain which AVRCP connections are tied to which
    AG and sets the active AVRCP instance if it is able to do so based on the 
    selected audio source
    
RETURNS
    none
*/
void SetSoundbarAVRCPActiveSource(audio_sources source);

#endif
#endif


/****************************************************************************
NAME    
    audioRouteAncIfAvailable
    
DESCRIPTION
    Checks for an ANC stream being present and routes it if present
    
RETURNS
    TRUE if ANC routed, FALSE otherwise
*/
bool audioRouteAncIfAvailable(audio_source_status * lAudioStatus);

/****************************************************************************
NAME    
    audioA2dpStartStream
    
DESCRIPTION
    Start the A2DP Audio stream request from the application.
    
RETURNS
    None
*/
void audioA2dpStartStream(void);

/****************************************************************************
NAME    
    audioRouteSelectedSource
    
DESCRIPTION
    attempts to process a button press request to change source, to prevent multiple 
    source change requests from being generated and consuming resources ultimately 
    leading to failure, if user presses buttons excessively quickly extra source change 
    requests will be discarded until the current one has completed
    
RETURNS
    success or failure status, false if discarded
*/
#ifdef ENABLE_SOUNDBAR
bool audioRouteSelectedSource(audio_sources source);
#endif

#endif /* _SINK_AUDIO_ROUTING_H_ */

