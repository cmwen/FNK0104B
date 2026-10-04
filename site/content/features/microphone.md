---
title: "Microphone & audio capture"
summary: "Turn sound into a level meter, a recording or a message."
status: Implemented
---

## The audio path
An onboard MEMS microphone connects through the ES8311 audio codec and I²S. The shared board library owns this hardware setup. A useful first diagnostic reads a short capture and reports its signal level without saving or printing the audio.

The recorder and speech paths use 16 kHz mono signed 16-bit audio. Recording a message and recognizing a command are different jobs: the first collects samples; the second runs a model on them.

## Practical interaction
Use a level meter so someone can tell whether the microphone hears them. Give clear waiting, speech, silence, stopped and submitting states. A no-speech timeout prevents a recording from hanging forever. A maximum duration bounds memory. VAD can decide when to stop after a pause, but a manual stop should remain available.

## Monitor behavior
After starting a message, the monitor allows ten seconds to begin speaking, ends after approximately five seconds of silence, and caps capture at thirty seconds. Silence-only captures are discarded. Recognition pauses during recording/submission. The recording is sent over Wi-Fi to the local bridge for transcription; this requires a configured service.

Physical meter sensitivity, actual thirty-second capture and running-agent message interaction still need device exercises. A displayed meter or successful build alone does not establish transcription quality.

## Start here
Use `audio-diag`, then `speech-diag` or `recorder-io-diag`. `locallink` demonstrates LAN speech transcription; `recorder` keeps recordings on SD.

## Brief your coding agent
> First show a microphone-level diagnostic. Then add bounded recording with a manual stop, silence termination and visible feedback. Keep captured audio out of logs. Explain where audio goes and how failures appear.
