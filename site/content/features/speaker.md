---
title: "Speaker output & alerts"
summary: "Play a note, acknowledge an action or replay a recording."
status: Implemented
---

## What is available
The ES8311 audio path supports speaker output through the external connector. A speaker is optional attached hardware. The board has no verified speaker-present signal, so firmware cannot tell whether one is plugged in.

## Useful patterns
Offer adjustable volume and a quiet option. A visual acknowledgement should still work without sound. Avoid making an attention tone compete with command listening or recording. The monitor defers tones during command listening and suspends recognition around playback.

The speaker diagnostic offers a touch piano and volume control. Recorder playback reads a selected Opus file through the decoder and I²S path. Its player accepts this recorder's bounded format; it is not a general music player.

## Evidence limits
Short recorder runs confirmed decoder/I²S completion and return to listening. Audible output and sustained playback are **UNKNOWN** in saved evidence. A serial “playback done” message proves the code path completed, not that a connected speaker was heard.

## Brief your coding agent
> Add a short optional alert with adjustable volume and a visual equivalent. Coordinate playback with microphone recognition. Validate with the speaker diagnostic and record an audible device check.
