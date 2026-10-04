// Copyright 2015-2026 Espressif Systems (Shanghai) PTE LTD
// Adapted from Arduino-ESP32 USBAudioCardDescriptors.h under Apache-2.0.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <USBAudioCardDescriptors.h>
// Windows groups audio interfaces by IAD whenever CDC's IAD is present.
// Keep the pinned UAC1 microphone template and add its missing two-interface IAD.
#undef TUD_AUDIO10_MICROPHONE_DESC_LEN
#undef TUD_AUDIO10_MICROPHONE_DESCRIPTOR
#define TUD_AUDIO10_MICROPHONE_DESC_LEN(_nfreqs) (8\
    +TUD_AUDIO10_DESC_STD_AC_LEN\
    + TUD_AUDIO10_DESC_CS_AC_LEN(1)\
    + TUD_AUDIO10_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO10_DESC_OUTPUT_TERM_LEN\
    /* Interface 2, Alternate 0 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    /* Interface 2, Alternate 1 (microphone) */\
    + TUD_AUDIO10_DESC_STD_AS_LEN\
    + TUD_AUDIO10_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO10_DESC_TYPE_I_FORMAT_LEN(_nfreqs)\
    + TUD_AUDIO10_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO10_DESC_CS_AS_ISO_EP_LEN)

#define TUD_AUDIO10_MICROPHONE_DESCRIPTOR(_itfnum, _stridx, _epin, _nMaxSampleRate, _nChannels_MIC, _nBytesPerSample, _nBitsUsedPerSample, ...) \
    8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_AUDIO, 0x01, 0x00, _stridx,\
    /* Standard AC Interface Descriptor(4.3.1) */\
    TUD_AUDIO10_DESC_STD_AC(/*_itfnum*/ _itfnum, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.3.2) */\
    TUD_AUDIO10_DESC_CS_AC(/*_bcdADC*/ 0x0100, /*_totallen*/ (TUD_AUDIO10_DESC_INPUT_TERM_LEN+TUD_AUDIO10_DESC_OUTPUT_TERM_LEN), /*_itf*/ (uint8_t)((_itfnum)+1)),\
    /* Microphone Input Terminal Descriptor(4.3.2.1) */\
    TUD_AUDIO10_DESC_INPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_nchannels*/ _nChannels_MIC, /*_channelcfg*/ AUDIO10_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_stridx*/ 0x00),\
    /* Microphone Output Terminal Descriptor(4.3.2.2) */\
    TUD_AUDIO10_DESC_OUTPUT_TERM(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_srcid*/ UAC1_ENTITY_MIC_INPUT_TERMINAL, /*_stridx*/ 0x00),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 1, Alternate 0 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx),\
    /* Standard AS Interface Descriptor(4.5.1) - Microphone Interface 1, Alternate 1 */\
    TUD_AUDIO10_DESC_STD_AS_INT(/*_itfnum*/ (uint8_t)((_itfnum)+1), /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx),\
    /* Class-Specific AS Interface Descriptor(4.5.2) */\
    TUD_AUDIO10_DESC_CS_AS_INT(/*_termid*/ UAC1_ENTITY_MIC_OUTPUT_TERMINAL, /*_delay*/ 0x01, /*_formattype*/ AUDIO10_DATA_FORMAT_TYPE_I_PCM),\
    /* Type I Format Type Descriptor(2.2.5) */\
    TUD_AUDIO10_DESC_TYPE_I_FORMAT(/*_nrchannels*/ _nChannels_MIC, /*_subframesize*/ _nBytesPerSample, /*_bitresolution*/ _nBitsUsedPerSample, /*_freqs*/ __VA_ARGS__),\
    /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.6.1.1) */\
    TUD_AUDIO10_DESC_STD_AS_ISO_EP(/*_ep*/ _epin, /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS), /*_maxEPsize*/ TUD_AUDIO_EP_SIZE(false, _nMaxSampleRate, _nBytesPerSample, _nChannels_MIC), /*_interval*/ 0x01, /*_syncep*/ 0x00),\
    /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.6.1.2) */\
    TUD_AUDIO10_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO10_CS_AS_ISO_DATA_EP_ATT_SAMPLING_FRQ, /*_lockdelayunits*/ AUDIO10_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001)
