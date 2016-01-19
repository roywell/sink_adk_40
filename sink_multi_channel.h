/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2004 - 2015.

FILE NAME
    sink_multi_channel.h

DESCRIPTION
    Module responsible for reading multi-channel configuration and other 
    functionality related to multi-channel audio output.

*/
#ifndef SINK_MULTI_CHANNEL_H
#define SINK_MULTI_CHANNEL_H

/****************************************************************************
NAME
    SinkMultiChannelReadConfig

DESCRIPTION
    Reads the PSKEY containing multi-channel output configuration and 
    initialises the multi-channel helper plug-in.

RETURNS
    void
*/
void SinkMultiChannelReadConfig(void);

#endif /*SINK_MULTI_CHANNEL_H*/
