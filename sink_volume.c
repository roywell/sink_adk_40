/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2004 - 2015.

FILE NAME
    sink_volume.c
    
DESCRIPTION
    module responsible for Vol control 
    
*/
#include "sink_statemanager.h"
#include "sink_volume.h"
#include "sink_tones.h"
#include "sink_pio.h"
#include "sink_slc.h"
#include "sink_audio.h"
#include "sink_wired.h"
#include "sink_display.h"

#include "sink_avrcp.h"

#ifdef ENABLE_PEER
#include "sink_peer.h"    
#endif 


#ifdef ENABLE_FM
#include "sink_fm.h"
#endif

#ifdef ENABLE_SUBWOOFER
#include "sink_swat.h"
#endif

#include <audio.h>
#include <stddef.h>
#include <string.h>

#ifdef DEBUG_VOLUME
#define VOL_DEBUG(x) DEBUG(x)
#else
#define VOL_DEBUG(x) 
#endif

/****************************************************************************
NAME
 volumeInit

DESCRIPTION
 malloc memory for storing of current volume levels for a2dp, usb, wired and fm
 interfaces

RETURNS
 none

*/
void volumeInit(void)
{
    /* malloc slot for current volume levels */
    theSink.volume_levels = PanicUnlessMalloc(sizeof(volume_levels_t));
    /* initialise to mute for all interfaces */
    memset(theSink.volume_levels, 0 , sizeof(volume_levels_t));
}

/* Helper to decide if sync request from AG affects our gain settings */
static bool volumeSyncAffectsGain(hfp_link_priority priority)
{
    Sink sink;
    /* Match active sink against HFP sink */
    if(HfpLinkGetAudioSink(priority, &sink))
        if( theSink.routed_audio && theSink.routed_audio == sink )
            return TRUE;

    return FALSE;
}

/******************************************************************************
NAME
    volumeGetMuteState

DESCRIPTION
    Get the stored mute state of a mute group

RETURNS
    AUDIO_MUTE_STATE_T of the mute group

*/
static AUDIO_MUTE_STATE_T volumeGetMuteState(AUDIO_MUTE_GROUP_T mute_group)
{
    if (theSink.mute_states & AUDIO_MUTE_MASK(mute_group))
        return AUDIO_MUTE_ENABLE;
    else
        return AUDIO_MUTE_DISABLE;
}

/******************************************************************************
NAME
    volumeSetMuteState

DESCRIPTION
    Set the stored mute state of a mute group

RETURNS
    void

*/
static void volumeSetMuteState(AUDIO_MUTE_GROUP_T mute_group, AUDIO_MUTE_STATE_T state)
{
    /* First un-set the bit corresponding to mute_group in the mute_states bit-field) */
    theSink.mute_states &= ~AUDIO_MUTE_MASK(mute_group);
    /* New state can now be OR'd over the top of the masked bit */
    theSink.mute_states |= (state << mute_group);
}

/******************************************************************************
NAME
    volumeGetMuteAllStatus

DESCRIPTION
    Get the status of the 'mute all outputs' override flag.

RETURNS
    TRUE if mute enabled, FALSE otherwise.

*/
static bool volumeGetMuteAllStatus(void)
{
    return theSink.mute_all_outputs;
}

/******************************************************************************
NAME
    volumeSetMuteAllStatus

DESCRIPTION
    Set the 'mute all outputs' override flag.

PARAMETERS
    bool enable_mute    Status to set the flag to.

RETURNS
    void

*/
static void volumeSetMuteAllStatus(bool enable_mute)
{
    theSink.mute_all_outputs = enable_mute;
}

/******************************************************************************
NAME
    VolumeApplySoftMuteStates

DESCRIPTION
    Send the stored soft mute state to the audio library, overriding the output
    mute states if necessary.

RETURNS
    void

*/
void VolumeApplySoftMuteStates(void)
{
    AUDIO_PLUGIN_SET_SOFT_MUTE_MSG_T message;

    message.mute_states = theSink.mute_states;

    if (volumeGetMuteAllStatus())
    {
        message.mute_states |= (AUDIO_MUTE_ENABLE << audio_mute_group_main);
        message.mute_states |= (AUDIO_MUTE_ENABLE << audio_mute_group_aux);
    }

    AudioSetSoftMute(&message);
}

/******************************************************************************
NAME
    VolumeAllOutputsMuted

DESCRIPTION
    Checks if both main and aux outputs are in a muted state.

RETURNS
    bool

*/
bool VolumeAllOutputsMuted(void)
{
    if (volumeGetMuteAllStatus())
        return TRUE;

    return volumeGetMuteState(audio_mute_group_main) &&
           volumeGetMuteState(audio_mute_group_aux);
}

/******************************************************************************
NAME
    VolumeUpdateMuteState

DESCRIPTION
    Update the stored mute state for a specific mute group

RETURNS
    void

*/
void VolumeUpdateMuteState(AUDIO_MUTE_GROUP_T mute_group, AUDIO_MUTE_STATE_T state)
{
    if (volumeGetMuteState(mute_group) == state)
        return;

    if (theSink.routed_audio)
    {
        if (mute_group == audio_mute_group_mic)
            VolumeSetMicrophoneMute(state);
        else
        {
            volumeSetMuteState(mute_group, state);
            VolumeApplySoftMuteStates();
        }
    }
}

/******************************************************************************
NAME
    VolumeUpdateMuteStatusAllOutputs

DESCRIPTION
    Manual override to temporarily force muting of all outputs or restore them
    to their previous mute states. This is so that other modules do not need to
    be aware of the distinction between multi-channel output groups ('main' and
    'aux'). They can simply mute everything as they always have done, but using
    the new soft mute interface rather than through mode setting. The stored
    multi-channel mute states can then be automatically restored later.

PARAMETERS
    bool enable_mute    If TRUE, mute all outputs, else restore previous states.

RETURNS
    void

*/
void VolumeUpdateMuteStatusAllOutputs(bool enable_mute)
{
    if (volumeGetMuteAllStatus() == enable_mute)
        return;

    volumeSetMuteAllStatus(enable_mute);

    VolumeApplySoftMuteStates();
}

/******************************************************************************
NAME
    VolumeUpdateMuteStatusMicrophone

DESCRIPTION
    Mute or unmute the microphone. Convenience function which would have the
    same effect as calling VolumeUpdateMuteState(audio_mute_group_mic, state);
    the only difference being that it takes a bool parameter. Complementary
    function to VolumeUpdateMuteStatusAllOutputs(), for older code that used to
    use AUDIO_PLUGIN_SET_MODE_MSG to control the mute state of the speaker/mic.

PARAMETERS
    bool enable_mute    If TRUE, mute microphone, else unmute it.

RETURNS
    void

*/
void VolumeUpdateMuteStatusMicrophone(bool enable_mute)
{
    AUDIO_MUTE_STATE_T state = enable_mute? AUDIO_MUTE_ENABLE : AUDIO_MUTE_DISABLE;

    if (volumeGetMuteState(audio_mute_group_mic) == state)
        return;

    VolumeSetMicrophoneMute(state);
}

/****************************************************************************
NAME
    VolumeToggleMute

DESCRIPTION
    Toggles the mute state of a specific mute group.

RETURNS
    void

*/
void VolumeToggleMute(AUDIO_MUTE_GROUP_T mute_group)
{
    sinkEvents_t event;

    /* If MUTE enabled for a group, generate the corresponding 'MuteOff' event.
       Otherwise, generate 'MuteOn' event, to toggle its mute status bit. */
    switch (mute_group)
    {
        case audio_mute_group_main:
        {
            if (volumeGetMuteState(mute_group))
                event = EventUsrMainOutVolMuteOff;
            else
                event = EventUsrMainOutVolMuteOn;
        }
        break;

        case audio_mute_group_aux:
        {
            if (volumeGetMuteState(mute_group))
                event = EventUsrAuxOutVolMuteOff;
            else
                event = EventUsrAuxOutVolMuteOn;
        }
        break;

        case audio_mute_group_mic:
        {
            if (volumeGetMuteState(mute_group))
                event = EventUsrMuteOff;
            else
                event = EventUsrMuteOn;
        }
        break;

        default:
        return;
    }

    MessageSend(&theSink.task, event, 0);
}

/******************************************************************************
NAME
    VolumeSetMicrophoneMute

DESCRIPTION
    Update the stored mute state for the HFP microphone and send it to the DSP.
    Will also mute all outputs if this feature is enabled.

RETURNS
    void

*/
void VolumeSetMicrophoneMute(AUDIO_MUTE_STATE_T state)
{
    /* Determine if currently routed audio is HFP */
    audio_source_status* lAudioStatus = audioGetStatus(theSink.routed_audio);

    if ((theSink.routed_audio == lAudioStatus->sinkAG1) || (theSink.routed_audio == lAudioStatus->sinkAG2))
    {
        /* Determine which AG is to be muted or unmuted, start with the AG that is currently routing audio */
        hfp_link_priority priority = HfpLinkPriorityFromAudioSink(theSink.routed_audio);

        /* Send unmute message if suitable AG found */
        if (priority)
        {
            VOL_DEBUG(("VOL: Mute T [%c]\n", (theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted ? 'F' : 'T') ));

            /* HFP audio currently being routed, update mute state of appropriate AG */
            VolumeSetHfpMicrophoneGain(priority, (state? VOLUME_MUTE_ON : VOLUME_MUTE_OFF));
        }
    }
#if defined(APTX_LL_BACK_CHANNEL) || defined(INCLUDE_FASTSTREAM)
    else if ((theSink.routed_audio == lAudioStatus->a2dpSinkPri) || (theSink.routed_audio == lAudioStatus->a2dpSinkSec))
    {
        /* A2DP currently routed, update mute state of the low latency back channel */
        VolumeSetA2dpMicrophoneGain(state);
    }
#endif /* defined(APTX_LL_BACK_CHANNEL) || defined(INCLUDE_FASTSTREAM)*/

    /* Free malloc'd status memory slot used for audio status checking */
    freePanic(lAudioStatus);
}


#if defined (APTX_LL_BACK_CHANNEL) || defined(INCLUDE_FASTSTREAM)
/****************************************************************************
DESCRIPTION
    Set mute or unmute of A2DP low latency back channel mic.
*/
void VolumeSetA2dpMicrophoneGain(AUDIO_MUTE_STATE_T state)
{
    VOL_DEBUG(("VOL: Set A2DP Mic Mute [%s]\n", state? "ON" : "OFF"));


    /* Update the mute state for the music streaming with back channel support dsp apps */
    theSink.a2dp_link_data->a2dp_audio_mode_params.mic_mute = state;
    
    volumeSetMuteState(audio_mute_group_mic, state);
    
    /* Check whether feature to mute microphone and speaker is set */
    if (theSink.features.MuteSpeakerAndMic)
    {
        if (state == AUDIO_MUTE_ENABLE)
            volumeSetMuteAllStatus(TRUE);
        else
            volumeSetMuteAllStatus(FALSE);
    }
    
    /* Apply mute/unmute the via the audio plugin */
    VolumeApplySoftMuteStates();
}
#endif /* defined (APTX_LL_BACK_CHANNEL) || defined(INCLUDE_FASTSTREAM)*/


/****************************************************************************
DESCRIPTION
    Set mute or unmute (mic gain of VOLUME_MUTE_ON - 0 is mute, all other
    gain settings unmute) of HFP mic.
*/
void VolumeSetHfpMicrophoneGain(hfp_link_priority priority, uint8 mic_gain)
{
    VOL_DEBUG(("VOL: Set Mic Gain [%d] %d\n", priority, mic_gain));


    /* determine the priority to control if not passed in */
    if(!priority)
    {
        /* try to get the currently routed audio */
        priority = HfpLinkPriorityFromAudioSink(theSink.routed_audio);

        /* is there a routed audio that can be muted or unmuted ? */
        if(!priority)
        {
            /* no routed audio on device, check for active call on either AG */
            hfp_call_state CallState = hfp_call_state_idle;
            HfpLinkGetCallState(PROFILE_INDEX(hfp_primary_link), &CallState);
            /* any calls on AG1? */
            if(CallState != hfp_call_state_idle)
                priority = hfp_primary_link;
            /* no calls on AG1, check AG2 */
            else
            {
                /* call state of AG2 */
                HfpLinkGetCallState(PROFILE_INDEX(hfp_secondary_link), &CallState);
                /* any calls on AG2? */
                if(CallState != hfp_call_state_idle)
                    priority = hfp_secondary_link;
            }
        }
    }

    VOL_DEBUG(("VOL: Set Mic Gain [%d] priority is: %d\n",  mic_gain, priority));

    /* ensure there is a valid AG to mute */
    if(priority)
    {
        /* If vol has been set to 0 then mute */
        if(mic_gain == VOLUME_MUTE_ON)
        {
            VOL_DEBUG(("VOL: Mute On\n"));
            /* Update settings */
            theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted = TRUE;
            volumeSetMuteState(audio_mute_group_mic, AUDIO_MUTE_ENABLE);
            /* Check whether feature to mute microphone and speaker is set */
            if (theSink.features.MuteSpeakerAndMic)
                volumeSetMuteAllStatus(TRUE);
            /* Cancel any existing reminder tones to prevent numerous scheduled reminder tones */
            MessageCancelAll( &theSink.task , EventSysMuteReminder ) ;
            /* Start reminder tones */
            if(theSink.conf1->timeouts.MuteRemindTime_s !=0)
               MessageSendLater( &theSink.task , EventSysMuteReminder , 0 ,D_SEC(theSink.conf1->timeouts.MuteRemindTime_s ) ) ;
        }
        /* mute off */
        else
        {
            VOL_DEBUG(("VOL: Mute Off on priority %d\n",priority));
            /* Update settings */
            theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted = FALSE;
            volumeSetMuteState(audio_mute_group_mic, AUDIO_MUTE_DISABLE);
            /* Check whether feature to mute microphone and speaker was set */
            if (theSink.features.MuteSpeakerAndMic)
                volumeSetMuteAllStatus(FALSE);
            /* Cancel reminder tones if no other muted AG */
            if((!theSink.profile_data[PROFILE_INDEX(hfp_primary_link)].audio.gMuted)&&
               (!theSink.profile_data[PROFILE_INDEX(hfp_secondary_link)].audio.gMuted))
            {
                MessageCancelAll( &theSink.task , EventSysMuteReminder ) ;
                VOL_DEBUG(("VOL: Mute Off - no mute - stop reminder tones\n"));
            }
        }

        /* mute/unmute the audio via the audio plguin */
        VolumeApplySoftMuteStates();
    }
}

/****************************************************************************
DESCRIPTION
    Set mute or unmute remotely from AG if SyncMic feature bit is enabled
    (mic gain of VOLUME_MUTE_ON - 0 is mute, all other gain settings unmute).
*/
void VolumeSetHfpMicrophoneGainCheckMute(hfp_link_priority priority, uint8 mic_gain)
{

    VOL_DEBUG(("VOL: Set Remote Mic Gain [%d] priority is: %d\n",  mic_gain, priority));

    /* ensure profile passed in is valid */
    if(priority)
    {
        /* if currently muted and volume is non zero then send unmute event */
        if((mic_gain != VOLUME_MUTE_ON)&&(theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted))
        {
            /* does this mute state change affect the currently routed audio? */
            if(volumeSyncAffectsGain(priority))
            {
                /* it does so indicate unmute has taken place */
                MessageSend(&theSink.task, EventUsrMuteOff, 0);
            }
            /* unmute state change occurred on AG that isn't currently having its audio
               routed, if other AG is not muted then cancel the mute reminder timer */
            else if(!theSink.profile_data[PROFILE_INDEX(OTHER_PROFILE(priority))].audio.gMuted)
            {
                /* prevent mute reminder timer from continuing as no valid mute cases any more */
                MessageCancelAll( &theSink.task , EventSysMuteReminder ) ;
                VOL_DEBUG(("VOL: Check Mute Off - no mute - stop reminder tones\n"));
            }
            /* update the profile's mute bit */
            theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted = FALSE;
        }
        /* if not muted and request to go into mute state */
        else if((mic_gain == VOLUME_MUTE_ON)&&(!theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted))
        {
            /* is this AG currently having its audio routed?*/
            if(volumeSyncAffectsGain(priority))
            {
                /* audio from this AG routed so process full mute indication */
                MessageSend(&theSink.task, EventUsrMuteOn, 0);
            }
            /* update the profile's mute bit */
            theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted = TRUE;
        }
    }
}

/****************************************************************************
DESCRIPTION
    Sends the current HFP microphone volume to the AG on connection.
*/
void VolumeSendHfpMicrophoneGain(hfp_link_priority priority, uint8 mic_gain)
{
    /* ensure link is valid before applying mute */
    if(priority != hfp_invalid_link)
    {
        VOL_DEBUG(("VOL: Sending Mic Gain [%d] %d\n", priority, mic_gain));
        /* Set profile's mute bit */
        theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted = (mic_gain ? FALSE : TRUE);
        /* Send mic volume to AG */
        HfpVolumeSyncMicrophoneGainRequest(priority , &mic_gain);
    }
}


/****************************************************************************
DESCRIPTION
    Determine whether the mute reminder tone should be played in the device, e.g. #
    if AG1 is in mute state but AG2 is not muted and is the active AG then the mute reminder
    tone will not be played, when AG1 becomes the active AG it will be heard.
*/
bool VolumePlayMuteToneQuery(void)
{
    hfp_link_priority priority;

    /* determine which AG is currently the active AG */
    priority = audioGetLinkPriority(TRUE);


    /* check whether AG that is currently routing its audio to the device speaker is in mute state,
       if it is not then prevent the mute reminder tone from being heard. */
    if((priority) && (theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted == FALSE))
    {
        VOL_DEBUG(("VOL: PlayMuteTone FALSE on %d mute is %d \n",priority,theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted));
        return FALSE;
    }
    /* all other use cases the mute reminder tone will be heard */
    else
    {
        VOL_DEBUG(("VOL: PlayMuteTone TRUE on %d mute is %d\n",priority,theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted));
        return TRUE;
    }
}

/****************************************************************************
    VolumeSetxxVolumeIndB functions, populate the AUDIO_PLUGIN_SET_GROUP_VOLUME_MSG_T
    message and call the function to send group volume message to the plugins
*****************************************************************************/
static void volumeSetMessageMainVolumeInDb(AUDIO_PLUGIN_SET_GROUP_VOLUME_MSG_T* msg, int16 vol)
{
    msg->main.master = vol;
    msg->main.tone = TonesGetToneVolumeInDb(multi_channel_group_main);
    
    /* Always supply device trims when setting main volume */
    msg->main.device_trim_master = sinkVolumeGetTwsMasterTrim();
    msg->main.device_trim_slave = sinkVolumeGetTwsSlaveTrim();
}

static void volumeSetMessageAuxVolumeInDb(AUDIO_PLUGIN_SET_GROUP_VOLUME_MSG_T* msg, int16 vol)
{
    msg->aux.master = vol;
    msg->aux.tone = TonesGetToneVolumeInDb(multi_channel_group_aux);
}

static void VolumeSetMainAndAuxVolumeIndB(const int16 volume_main, const int16 volume_aux)
{
    /* Populate all fields in group volume message */
    AUDIO_PLUGIN_SET_GROUP_VOLUME_MSG_T group_vol;
    group_vol.group = multi_channel_group_all;
    volumeSetMessageMainVolumeInDb(&group_vol, volume_main);
    volumeSetMessageAuxVolumeInDb(&group_vol, volume_aux);

    AudioSetGroupVolume(&group_vol);
}

static void VolumeSetMainVolumeIndB(const int16 volume_main)
{
    /* Populate fields for main volume */
    AUDIO_PLUGIN_SET_GROUP_VOLUME_MSG_T group_vol;
    group_vol.group = multi_channel_group_main;
    volumeSetMessageMainVolumeInDb(&group_vol, volume_main);

    AudioSetGroupVolume(&group_vol);
}

static void VolumeSetAuxVolumeIndB(const int16 volume_aux)
{
    /* Populate fields for aux volume */
    AUDIO_PLUGIN_SET_GROUP_VOLUME_MSG_T group_vol;
    group_vol.group = multi_channel_group_aux;
    volumeSetMessageAuxVolumeInDb(&group_vol, volume_aux);

    AudioSetGroupVolume(&group_vol);
}


/****************************************************************************
    Volume related helper functions
*****************************************************************************/
static int16 VolumeGetVolumeIndB(const int16 volume, const audio_plugin_mch_group_t group)
{
    int16 volume_in_dB = VolumeConvertStepsToDB(volume, &sinkVolumeGetGroupConfig(group), DSP_DB_SCALE);

    if(volume == VOLUME_A2DP_MUTE_GAIN)
    {
        volume_in_dB = VOLUME_MUTE_DB;
    }

    return volume_in_dB;
}

static void VolumeSetVolumeIndB(const int16 volume, const audio_plugin_mch_group_t group)
{
    if(group == multi_channel_group_all)
    {
        VolumeSetMainAndAuxVolumeIndB(volume, volume);
    }
    if(group == multi_channel_group_main)
    {
        VolumeSetMainVolumeIndB(volume);
    }
    else
    {
        VolumeSetAuxVolumeIndB(volume);
    }
}

static int16 VolumeGetGroupVolumeFromInputVolumes(const volume_info * const volume, const audio_plugin_mch_group_t group)
{
    if(group == multi_channel_group_aux)
    {
        return volume->aux_volume;
    }
    return volume->main_volume;
}

static void VolumeSetDSPOperatingMode(const volume_info * const volumes, const audio_plugin_mch_group_t group, const int16 previousVolume)
{
    if(VolumeGetGroupVolumeFromInputVolumes(volumes, group) != previousVolume)
    {
        /* Check whether volume has gone from 0 (mute) to a valid level, or vice versa, as if so the DSP needs to enter/exit standby mode. */
        if (previousVolume == VOLUME_A2DP_MIN_LEVEL &&
                (volumes->aux_volume == VOLUME_A2DP_MIN_LEVEL || volumes->main_volume == VOLUME_A2DP_MIN_LEVEL))
        {
            /* Set the operating mode to FULL_PROCESSING */
            AudioSetMode(AUDIO_MODE_CONNECTED, &theSink.a2dp_link_data->a2dp_audio_mode_params);
        }
        else if ((previousVolume != VOLUME_A2DP_MIN_LEVEL) &&
                (volumes->main_volume == VOLUME_A2DP_MIN_LEVEL && volumes->aux_volume == VOLUME_A2DP_MIN_LEVEL))
        {
            /* Set the operating mode to STANDBY */
            AudioSetMode(AUDIO_MODE_STANDBY, &theSink.a2dp_link_data->a2dp_audio_mode_params);
        }
    }
}

static void VolumeSetNewVolume(const volume_info * const volumes, const int16 previousVolume, const audio_plugin_mch_group_t group)
{
    int16 group_volume = VolumeGetGroupVolumeFromInputVolumes(volumes, group);

    VolumeSetVolumeIndB((VolumeGetVolumeIndB(group_volume, group)), group);

    if(group == multi_channel_group_main)
    {
        displayUpdateVolume((VOLUME_NUM_VOICE_STEPS * group_volume)/sinkVolumeGetGroupConfig(group).no_of_steps);
#ifdef ENABLE_SUBWOOFER
        updateSwatVolume(group_volume);
#endif
    }

    VolumeSetDSPOperatingMode(volumes, group, previousVolume);
}

static void VolumeIncrementGroupVolumeAndCheckLimit(volume_info * const volume, const audio_plugin_mch_group_t group)
{
    int16 *groupVolume = &volume->main_volume;

    if(group == multi_channel_group_aux)
    {
        groupVolume = &volume->aux_volume;
    }

    (*groupVolume)++;

    if(*groupVolume >= (sinkVolumeGetGroupConfig(group).no_of_steps - 1))
    {
        *groupVolume = (sinkVolumeGetGroupConfig(group).no_of_steps - 1);
        MessageSend(&theSink.task, EventSysVolumeMax, 0);
    }
}

static void VolumeDecrementGroupVolumeAndCheckLimit(volume_info * const volume, const audio_plugin_mch_group_t group)
{
    int16 *groupVolume = &volume->main_volume;

    if(group == multi_channel_group_aux)
    {
        groupVolume = &volume->aux_volume;
    }

    (*groupVolume)--;

    if(*groupVolume <= VOLUME_A2DP_MIN_LEVEL)
    {
        *groupVolume = VOLUME_A2DP_MIN_LEVEL;
        MessageSend(&theSink.task, EventSysVolumeMin, 0);
    }
}

static void VolumeUnmuteOnVolumeChange(const audio_plugin_mch_group_t group)
{
    if (theSink.features.VolumeChangeCausesUnMute)
    {
        if((group == multi_channel_group_all || group == multi_channel_group_main) &&
                volumeGetMuteState(audio_mute_group_main) == AUDIO_MUTE_ENABLE)
        {
            MessageSend(&theSink.task, EventUsrMainOutVolMuteOff, 0);
        }
        if((group == multi_channel_group_all || group == multi_channel_group_aux) &&
                volumeGetMuteState(audio_mute_group_aux) == AUDIO_MUTE_ENABLE)
        {
            MessageSend(&theSink.task, EventUsrAuxOutVolMuteOff, 0);
        }
    }
}

/****************************************************************************
NAME 
 VolumeCheckA2dpMute

DESCRIPTION
 check whether any a2dp connections are at minimum volume and mutes them properly if they are

RETURNS
 bool   Returns true if stream muted
    
*/
bool VolumeCheckA2dpMute()
{
    uint8 index;
 
    /* check both possible instances of a2dp connection */
    for(index = a2dp_primary; index < (a2dp_secondary+1); index++)
    {
        /* is a2dp connected? */
        if(theSink.a2dp_link_data->connected[index])
        {
            /* check whether the a2dp connection is present and streaming data and that the audio is routed */
            if(theSink.routed_audio && 
               (theSink.routed_audio == A2dpMediaGetSink(theSink.a2dp_link_data->device_id[index], theSink.a2dp_link_data->stream_id[index])))
            {
                /* get current volume for this profile */
                uint16 currentVol = theSink.volume_levels->a2dp_volume[index].main_volume;
                                       
                if( currentVol == VOLUME_A2DP_MUTE_GAIN)
                {
                    /* mute adjusted for a A2DP media stream */
                    VOL_DEBUG(("VOL: A2dp re-mute\n"));
                    return TRUE;
                }
            }
        }
    }
    /* no routed a2dp media streams found */
    return FALSE;
}

/****************************************************************************
DESCRIPTION
 sets the internal speaker gain to the level corresponding to the phone volume level
 
*/
void VolumeSetHeadsetVolume( uint16 pNewVolume , bool pPlayTone, hfp_link_priority priority) 
{      
    bool lPlayTone = FALSE ;
    bool lVolumeChangeCausesUnMute = theSink.features.VolumeChangeCausesUnMute ;
    bool lAdjustVolumeWhilstMuted = theSink.features.AdjustVolumeWhilstMuted ;
    bool set_gain = volumeSyncAffectsGain(priority);

    VOL_DEBUG(("SetVol [%x] [%d][%d][%d]\n " ,pNewVolume, theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted , lVolumeChangeCausesUnMute , lAdjustVolumeWhilstMuted)) ;
    
	/* only adjust the volume if this AG is not muted or the device has the feature to allow volume change
       whilst muted or the feature to unmute on volume changes */
    if ( (!theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted ) || ( lVolumeChangeCausesUnMute ) || 
         (lAdjustVolumeWhilstMuted))
    {
        /* set the local volume only, check whether unMute on volume change feature is enabled*/
        if ((theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted)&&(lVolumeChangeCausesUnMute))
		{
            VolumeSetHfpMicrophoneGainCheckMute(priority, VOLUME_MUTE_OFF);
        }
        
        /* the tone needs to be played so set flag */
        lPlayTone = TRUE ;     
        
        /* set new volume */
        theSink.profile_data[PROFILE_INDEX(priority)].audio.gSMVolumeLevel = pNewVolume ; 
        
        /* update the display */
        displayUpdateVolume((VOLUME_NUM_VOICE_STEPS * pNewVolume)/sinkVolumeGetGroupConfig(multi_channel_group_main).no_of_steps);
        
        /* determine whether this volume change affects the audio currently being routed to the speaker #
           and update volume level via audio plugin if this is the case */
        if(set_gain)
            AudioSetVolume ( sinkVolumeGetCvcVol(pNewVolume) , TonesGetToneVolume(FALSE), theSink.codec_task ) ;
    }
    
    /* ensure there is a valid tone (non zero) to be played */
    if( pPlayTone && lPlayTone && theSink.conf6->volume_config.gVolMaps[pNewVolume].Tone )
    {   /*only attempt to play the tone if it has not yet been played*/
        VOL_DEBUG(("VOL: VolTone[%x]\n" , (int)theSink.conf6->volume_config.gVolMaps[pNewVolume].Tone)) ;
        TonesPlayTone(theSink.conf6->volume_config.gVolMaps[pNewVolume].Tone ,theSink.features.QueueVolumeTones, FALSE);
    }  
    
    /* Send notification for max/min volume */
    if(pNewVolume >= VOLUME_HFP_MAX_LEVEL)
    {
        MessageSend ( &theSink.task , EventSysVolumeMax , 0 );
    }
    else if (pNewVolume == VOLUME_HFP_MIN_LEVEL)
    {
        MessageSend ( &theSink.task , EventSysVolumeMin , 0 );
    }
}

/*************************************************************************
NAME    
    storeCurrentSinkVolume
    
DESCRIPTION
    Stores the current volume level of the sink which is streaming audio (HFP or A2DP)

RETURNS

*/
void storeCurrentSinkVolume( void )
{
    tp_bdaddr SrcAddr;
    audio_route_available route = sinkAudioRouteAvailable();

    if( route ) 
    {
        SinkGetBdAddr(theSink.routed_audio, &SrcAddr);
        switch( route )
        {
            case audio_route_a2dp_primary:
                VOL_DEBUG(("VOL : update a2dp_primary attributes\n")) ;
                deviceManagerUpdateAttributes(&SrcAddr.taddr.addr, sink_a2dp, hfp_invalid_link, a2dp_primary);  
                break;
            case audio_route_a2dp_secondary:
                VOL_DEBUG(("VOL : update a2dp_secondary attributes\n")) ;
                deviceManagerUpdateAttributes(&SrcAddr.taddr.addr, sink_a2dp, hfp_invalid_link, a2dp_secondary);
                break;
            case audio_route_hfp_primary:
                /* check primary hfp    */
                VOL_DEBUG(("VOL : update hfp_primary attributes\n")) ;
                deviceManagerUpdateAttributes(&SrcAddr.taddr.addr, sink_hfp, hfp_primary_link, 0);  
                break;
            case audio_route_hfp_secondary:
                VOL_DEBUG(("VOL : update hfp_secondary attributes\n")) ;
                deviceManagerUpdateAttributes(&SrcAddr.taddr.addr, sink_hfp, hfp_secondary_link, 0);  
                break;
            case audio_route_usb:
            case audio_route_analog:
            case audio_route_spdif:
            case audio_route_fm:
            case audio_route_none:
            default:
                VOL_DEBUG(("VOL: No Attributes to store for route :[%d]\n",route));
                break;
        }
    }
}


/****************************************************************************
DESCRIPTION
    sets the vol to the level corresponding to the phone volume level
    In addition - send a response to the AG indicating new volume level
    
*/
void VolumeSendAndSetHeadsetVolume( uint16 pNewVolume , bool pPlayTone, hfp_link_priority priority ) 
{
    /* ensure profile is connected before changing volume */
    if(priority)
    {
        /*if hfp is attached - then send the vol change, but only if the device is not
          muted or the feature to allow volume changes whilst muted is enabled or the feature to unmute on
          volume changes is enabled */
        if ( (stateManagerIsConnected() && (!theSink.profile_data[PROFILE_INDEX(priority)].audio.gMuted))||
             (theSink.features.VolumeChangeCausesUnMute)||(theSink.features.AdjustVolumeWhilstMuted))
        {     
            HfpVolumeSyncSpeakerGainRequest ( priority , (uint8*)&pNewVolume ) ;
        }
        VOL_DEBUG(("VOL: SEND and %x",(unsigned int) priority)) ;
        
        /* make the corresponding local volume changes */
        VolumeSetHeadsetVolume( pNewVolume , pPlayTone, priority );
    }
}


/****************************************************************************
    HFP Audio Volume increment/decrement functions
*****************************************************************************/
static bool VolumeIfHFPConnectedModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(deviceManagerNumConnectedDevs())
    {
        if(group == multi_channel_group_main)
        {
            /* Get the link to change volume on */
            hfp_link_priority priority = audioGetLinkPriority(TRUE);
            /* Get current volume for this profile */
            uint16 lOldVol = theSink.profile_data[PROFILE_INDEX(priority)].audio.gSMVolumeLevel;
            /* Obtain new volume level */
            VolMapping_t* mapping = &theSink.conf6->volume_config.gVolMaps[lOldVol];
            uint16 lNewVol = (direction == increase_volume ? mapping->IncVol : mapping->DecVol);

			/* limit to maximum */
			if(lNewVol >= VOLUME_HFP_MAX_LEVEL)
			{
				lNewVol = VOLUME_HFP_MAX_LEVEL;
			}
            
			/* send, set and store new volume level */
			VolumeSendAndSetHeadsetVolume ( lNewVol ,TRUE , priority);

            VOL_DEBUG(("VOL: VolUp[%d][%d] to AG%d\n",lOldVol, lNewVol, priority));
        }
        return TRUE;
    }
    return FALSE;
}

static bool VolumeIfHFPAudioRoutedModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(HfpLinkPriorityFromAudioSink(theSink.routed_audio))
    {
        return VolumeIfHFPConnectedModifyAndUpdateVolume(direction, group);
    }
    return FALSE;
}


/****************************************************************************
    Generic VolumeModifyAndUpdateVolume function called from plugin specific
    VolumeXXModifyAndUpdateVolume functions
*****************************************************************************/
static void VolumeModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group, volume_info * const volumeToModify)
{
    int16 previousVolume = VolumeGetGroupVolumeFromInputVolumes(volumeToModify, group);

    if(direction == increase_volume)
    {
        VolumeIncrementGroupVolumeAndCheckLimit(volumeToModify, group);
    }
    else if(direction == decrease_volume)
    {
        VolumeDecrementGroupVolumeAndCheckLimit(volumeToModify, group);
    }

    VOL_DEBUG(("VOL: Music main [%d] aux [%d] \n", volumeToModify->main_volume, volumeToModify->aux_volume));    
    
    VolumeSetNewVolume(volumeToModify, previousVolume, group);

    VolumeUnmuteOnVolumeChange(group);
}

/****************************************************************************
    A2DP Audio related increment/decrement volume functions
*****************************************************************************/
static void volumeModifyAndUpdateAvrcpAbsoluteVolume(const volume_direction direction,
                                                                    const int16 newVolume,
                                                                    const int16 previousVolume,
                                                                    const audio_plugin_mch_group_t group)
{
    if(avrcpAvrcpIsEnabled() && group == multi_channel_group_main)
    {
        int16 vol_step_change = 0;
#ifdef ENABLE_AVRCP
        uint16 no_of_steps = sinkVolumeGetGroupConfig(multi_channel_group_main).no_of_steps;
#endif
        if (previousVolume > (newVolume - 1))
        {
            vol_step_change = previousVolume - newVolume;
        }
        else
        {
            vol_step_change = newVolume - previousVolume;
        }

        VOL_DEBUG(("VOL: avrcp abs vol old[%d] new[%d] step_change[%d]\n", previousVolume, newVolume, vol_step_change));

        /* update absolute volume control change */
        sinkAvrcpVolumeStepChange(direction, vol_step_change, no_of_steps);
    }
}

static bool volumeIfPeerAudioRoutedModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group, const uint8 a2dpIndex)
{
#if defined ENABLE_PEER
     if((theSink.a2dp_link_data->peer_device[a2dpIndex] == remote_device_peer) &&
            (theSink.a2dp_link_data->peer_features[a2dpIndex] & remote_features_tws_a2dp_source) &&
            group == multi_channel_group_main)
     {
         /* Audio routed from a Peer device which supports TWS as a Source.
          * This takes priority over ShareMe and thus means we are in a TWS session */
         if(direction == increase_volume)
         {
             sinkAvrcpVolumeUp();
         }
         else if(direction == decrease_volume)
         {
             sinkAvrcpVolumeDown();
         }
         return TRUE;
     }
#endif
     return FALSE;
}

static bool VolumeIfA2dpAudioRoutedModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(a2dpA2dpAudioIsRouted())
    {
        a2dp_link_priority index = a2dpGetRoutedInstanceIndex();

        if(volumeIfPeerAudioRoutedModifyAndUpdateVolume(direction, group, index))
        {
            VolumeUnmuteOnVolumeChange(group);
        }
        else
        {
            int16 previousVolume = VolumeGetGroupVolumeFromInputVolumes(&theSink.volume_levels->a2dp_volume[index], group);

            VolumeModifyAndUpdateVolume(direction, group, &theSink.volume_levels->a2dp_volume[index]);

            volumeModifyAndUpdateAvrcpAbsoluteVolume(direction,
                                            VolumeGetGroupVolumeFromInputVolumes(&theSink.volume_levels->a2dp_volume[index], group),
                                            previousVolume, group);
        }

        return TRUE;
    }
    return FALSE;
}

/****************************************************************************
    Wired Audio related increment/decrement volume functions
*****************************************************************************/
static bool VolumeIfWiredAudioRoutedModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(wiredAnalogAudioIsRouted())
    {
        VolumeModifyAndUpdateVolume(direction, group, &theSink.volume_levels->analog_volume);
        return TRUE;
    }
    if(wiredSpdifAudioIsRouted())
    {
        VolumeModifyAndUpdateVolume(direction, group, &theSink.volume_levels->spdif_volume);
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************
    FM related increment/decrement volume functions
*****************************************************************************/
static bool VolumeIfFmIsOnModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(IsSinkFmRxOn())
    {
        VolumeModifyAndUpdateVolume(direction, group, &theSink.volume_levels->fm_volume);
        return TRUE;
    }
    return FALSE;
}

/****************************************************************************
    USB Audio related increment/decrement volume functions
*****************************************************************************/
static void VolumeModifyAndUpdateUsbVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(group == multi_channel_group_aux)
    {
        VolumeModifyAndUpdateVolume(direction, group, &theSink.volume_levels->usb_volume);
    }
    else
    {
        if(direction == increase_volume)
        {
            usbVolUp();
        }
        else
        {
            usbVolDn();
        }
    }
}

static bool VolumeIfCurrentSinkIsUSBModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(usbIfCurrentSinkIsUsb())
    {
        VolumeModifyAndUpdateUsbVolume(direction, group);
        return TRUE;
    }
    return FALSE;
}

static bool VolumeIfUSBSinkExistsModifyAndUpdateVolume(const volume_direction direction, const audio_plugin_mch_group_t group)
{
    if(usbIfUsbSinkExists())
    {
        VolumeModifyAndUpdateUsbVolume(direction, group);
        return TRUE;
    }
    return FALSE;
}


static void VolumeModifyAndUpdateRoutedAudioVolume(volume_direction direction, audio_plugin_mch_group_t group)
{
    /* Try A2DP first */
    if(VolumeIfA2dpAudioRoutedModifyAndUpdateVolume(direction, group))
        return;

    /* Try HFP if has audio */
    if(VolumeIfHFPAudioRoutedModifyAndUpdateVolume(direction, group))
        return;

    /* Try USB if has audio */
    if(VolumeIfCurrentSinkIsUSBModifyAndUpdateVolume(direction, group))
        return;

    /* Try wired if has audio */
    if(VolumeIfWiredAudioRoutedModifyAndUpdateVolume(direction, group))
        return;

    /*Try FM RX*/
    if(VolumeIfFmIsOnModifyAndUpdateVolume(direction, group))
        return;

    /* Try USB */
    if(VolumeIfUSBSinkExistsModifyAndUpdateVolume(direction, group))
        return;

    /* Try HFP without any audio connections */
    VolumeIfHFPConnectedModifyAndUpdateVolume(direction, group);
}

/****************************************************************************
    TWS device trim related volume functions
*****************************************************************************/
#if defined ENABLE_PEER && defined PEER_TWS
bool VolumeIfAudioRoutedModifyAndUpdateTWSDeviceTrim (const volume_direction dir, const tws_device_type tws_device)
{
    if(a2dpCheckDeviceTrimVol(dir, tws_device) || usbCheckDeviceTrimVol(dir, tws_device) ||wiredAudioCheckDeviceTrimVol(dir, tws_device))
    {
        return TRUE;
    }

    return FALSE;
}

void VolumeModifyAndUpdateTWSDeviceTrim(const volume_direction direction, const tws_device_type tws_device, const volume_info * const currentVolumes)
{
    int16 *device_trim;

    VOL_DEBUG(("VOL: VolumeModifyAndUpdateTWSDeviceTrim [%i][%i][%i][%i][%i]\n", theSink.conf6->volume_config.volume_control_config.device_trim_master,
                                                            theSink.conf6->volume_config.volume_control_config.device_trim_slave,
                                                            theSink.conf6->volume_config.volume_control_config.device_trim_change,
                                                            theSink.conf6->volume_config.volume_control_config.device_trim_min,
                                                            theSink.conf6->volume_config.volume_control_config.device_trim_max));

    if (tws_device == tws_master)
    {
        device_trim = &theSink.conf6->volume_config.volume_control_config.device_trim_master;
    }
    else
    {
        device_trim = &theSink.conf6->volume_config.volume_control_config.device_trim_slave;
    }

    if (direction == increase_volume)
    {   /* Increase and clamp to upper limit */
        *device_trim += theSink.conf6->volume_config.volume_control_config.device_trim_change;

        if (*device_trim >= theSink.conf6->volume_config.volume_control_config.device_trim_max)
        {
            *device_trim = theSink.conf6->volume_config.volume_control_config.device_trim_max;
            MessageSend ( &theSink.task , EventSysTrimVolumeMax , 0 );
        }

        VOL_DEBUG(("VOL: inc device trim [%i]\n", *device_trim));
    }
    else
    {   /* Decrease and clamp to lower limit */
        *device_trim -= theSink.conf6->volume_config.volume_control_config.device_trim_change;

        if (*device_trim <= theSink.conf6->volume_config.volume_control_config.device_trim_min)
        {
            *device_trim = theSink.conf6->volume_config.volume_control_config.device_trim_min;
            MessageSend ( &theSink.task , EventSysTrimVolumeMin , 0 );
        }

        VOL_DEBUG(("VOL: dec device trim [%i]\n", *device_trim));
    }

    VolumeModifyAndUpdateRoutedAudioVolume(same_volume, multi_channel_group_main);
}

#endif


/****************************************************************************
NAME 
 VolumeHandleSpeakerGainInd

DESCRIPTION
 Handle speaker gain change indication from the AG

RETURNS
 void
    
*/
void VolumeHandleSpeakerGainInd(HFP_VOLUME_SYNC_SPEAKER_GAIN_IND_T* ind)
{
    VolumeSetHeadsetVolume ( ind->volume_gain , theSink.features.PlayLocalVolumeTone , ind->priority ) ;
}

/****************************************************************************
    Functions to increment/decrement or resend stored group volumes to
    the current audio plugin
*/
void VolumeModifyAndUpdateRoutedAudioMainVolume(const volume_direction dir)
{
    VolumeModifyAndUpdateRoutedAudioVolume(dir, multi_channel_group_main);
}

void VolumeModifyAndUpdateRoutedAudioAuxVolume(const volume_direction dir)
{
    VolumeModifyAndUpdateRoutedAudioVolume(dir, multi_channel_group_aux);
}

void VolumeModifyAndUpdateRoutedA2DPAudioMainVolume(const volume_direction direction)
{
    VolumeIfA2dpAudioRoutedModifyAndUpdateVolume(direction, multi_channel_group_main);
}

void VolumeUpdateRoutedAudioMainAndAuxVolume(void)
{
    VolumeModifyAndUpdateRoutedAudioVolume(same_volume, multi_channel_group_main);
    VolumeModifyAndUpdateRoutedAudioVolume(same_volume, multi_channel_group_aux);
}

/****************************************************************************
    Send new main group volume level to currently loaded plugin
    - updates display, sub and uses previous volume to update dsp
    operating mode
*/
void VolumeSetNewMainVolume(const volume_info * const volumes, const int16 previousVolume)
{
    VolumeSetNewVolume(volumes, previousVolume, multi_channel_group_main);
}

/****************************************************************************
    Send new main and aux group volumes directly to currently loaded plugin
*/
void VolumeSetAndUpdateMainAndAuxVolume(const volume_info * const volumes)
{
    int16 main_volume_in_dB = VolumeConvertStepsToDB(volumes->main_volume, &sinkVolumeGetGroupConfig(multi_channel_group_main), DSP_DB_SCALE);
    int16 aux_volume_in_dB = VolumeConvertStepsToDB(volumes->aux_volume, &sinkVolumeGetGroupConfig(multi_channel_group_aux), DSP_DB_SCALE);

    VolumeSetMainAndAuxVolumeIndB(main_volume_in_dB, aux_volume_in_dB);
}

#define PIO_AUX_OUT_DETECT (theSink.conf6->PIOIO.pio_inputs.aux_out_detect)

/* JACK_DET LOW = Disconnected, HIGH = Connected */
#define pio_aux_out_connected() (PioGetPio(PIO_AUX_OUT_DETECT))

/****************************************************************************
    Set the initial mute states for audio inputs and outputs
*/
void VolumeSetInitialMuteState(void)
{
    /* Default is to un-mute all outputs so main and aux play concurrently */
    theSink.mute_states = AUDIO_MUTE_DISABLE_ALL;
    
    /* If the aux_out_detect is configured then we apply privacy rules */
    if(PIO_AUX_OUT_DETECT != NO_PIO)
    {
        /* Mute main if aux is connected, otherwise mute aux */
        if(pio_aux_out_connected())
            theSink.mute_states |= AUDIO_MUTE_MASK(audio_mute_group_main);
        else
            theSink.mute_states |= AUDIO_MUTE_MASK(audio_mute_group_aux);
    }
    
    theSink.mute_all_outputs = FALSE;
}
