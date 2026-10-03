# Native host tests

PlatformIO requires runnable suite directory names to start with `test_`. The native Unity suite is in `test/test_native/`; this directory records the host-test category and can hold test design notes.

The suite covers LocalLink's DNS-SD endpoint selection, fallback validation,
mono PCM WAV header, and multipart request framing in addition to the runner
smoke check. Hardware capture and network behavior still require the firmware
diagnostics and physical device.

Recorder tests cover Ogg page CRC/lacing bounds and silence/duration stop rules.
The optional `python3 scripts/check_recorder_opus.py` uses host libopus and
FFmpeg to verify container compatibility, pre-skip and exact final trimming.
It does not test the ESP-specific codec binary or physical audio path.

`python3 scripts/recorder_ui_preview.py` renders the recorder's actual drawing
code with the shared IDF font, bounds-checking all six screen states. The output
is PPM; FFmpeg can convert it to PNG without additional Python dependencies.
