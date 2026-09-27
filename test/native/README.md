# Native host tests

PlatformIO requires runnable suite directory names to start with `test_`. The native Unity suite is in `test/test_native/`; this directory records the host-test category and can hold test design notes.

The suite covers LocalLink's DNS-SD endpoint selection, fallback validation,
mono PCM WAV header, and multipart request framing in addition to the runner
smoke check. Hardware capture and network behavior still require the firmware
diagnostics and physical device.
