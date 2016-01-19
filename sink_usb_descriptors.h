/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2004 - 2015.

FILE NAME
    sink_usb_descriptors.h
    
DESCRIPTION
    
*/
#ifndef _SINK_USB_DESCRIPTORS_H_
#define _SINK_USB_DESCRIPTORS_H_

#if defined(ENABLE_USB) && defined(ENABLE_USB_AUDIO)

#define SAMPLE_RATE_CVC    ((uint32) 8000)
#define SAMPLE_RATE_CVC_WB ((uint32) 16000)
#define SAMPLE_RATE_STEREO ((uint32) 48000)

/* Unit/Terminal IDs */
#define SPEAKER_IT  0x01
#define SPEAKER_FU  0x02
#define SPEAKER_OT  0x03
#define MIC_IT      0x04
#define MIC_FU      0x05
#define MIC_OT      0x06

/* audio volume configuration */
#define MIC_MAX 0x0000 /* +00.00 */
#define MIC_MIN 0xD200 /* -45.00 */
#define MIC_RES 0x0300 /* +03.00 */
#define MIC_DEFAULT 0x0000

#define VOL_MIN 0x00C4
#define VOL_MAX 0x0000


#endif

#endif /* _SINK_USB_DESCRIPTORS_H_ */
