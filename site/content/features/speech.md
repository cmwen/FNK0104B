---
title: "Wake words, commands & on-device ML"
summary: "Say a wake phrase and control the board without a cloud service."
status: Implemented
---

## Four pieces with different jobs
| Piece | Question it answers | Repository use |
| --- | --- | --- |
| WakeNet10 | Did I hear the wake phrase? | “Hi ESP” |
| VADNet | Is this speech or silence? | Recording and speech/silence feedback |
| MultiNet7 | Which configured command was spoken? | A small English command vocabulary |
| Transcription service | What words were said in an open-ended message? | Optional service behind the local bridge |

WakeNet, VADNet and MultiNet run on the board. They are not a general conversational model. They do not require a transcription endpoint for local commands. Free-form messages use a separate service; its privacy and connectivity depend on the backend you configure.

## Using the monitor
Say **Hi ESP**, then within the twelve-second command window say one of:

- **start listening** — begin recording a message;
- **go back** — return to the agent overview;
- **show status** — hold the quota view even while agents are active;
- **turn on the screen** — enable the display backlight;
- **turn off the screen** — disable the display backlight.

The screen lists the actual registered phrases and shows listening/volume feedback. The message recording panel is a separate interaction with separate timeouts. Speech never authorizes Codex approvals; handle approvals in the Codex client.

## What we learned
Prototype the microphone, wake word and vocabulary independently before combining them with Wi-Fi/BLE and graphics. Models occupy their own flash partition and consume working memory. A firmware image without the model image is incomplete. In the speech diagnostic, command inference reached 31.52 ms for a 32 ms frame: sustained throughput and scheduling need measurement.

The monitor now has device evidence for model initialization, continuous audio processing and live SSE together. Systematic spoken accuracy, long captures and long-term heap stability remain **UNKNOWN**. Do not advertise an unmeasured recognition rate.

## Other ML ideas
Sound-event classification or a custom wake phrase could be useful experiments. No custom model training, gesture model or general sound classifier is implemented here. Start with a supported model and a standalone diagnostic; check memory, inference time and false activations before integration.

## Brief your coding agent
> Build an offline speech diagnostic with the existing wake phrase and a short explicit vocabulary. Show listening and timeout states. Measure inference against the audio frame interval. Keep model upload in the named PlatformIO environment.

## Sources
[ESP-SR](https://github.com/espressif/esp-sr), [ESP32-S3 benchmarks](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html), [VADNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/vadnet/README.html).
