# Recorder I/O diagnostic

This independent PlatformIO target checks the new IDF SD, shared I²C touch
and full-duplex codec path before running recognition and compression together.

```sh
pio run -e recorder-io-diag
pio device list
pio run -e recorder-io-diag -t upload --upload-port <port>
pio device monitor --port <port> -b 115200
```

Insert a FAT16/FAT32 card. The app never formats the card. It exclusively creates
`/recorder-io-test.txt` and reads it back; an existing file is preserved and
reported as `not_tested_or_failed`. Look for `sd=ESP_OK`,
`file_roundtrip=passed`, `touch=ready` and `audio=ready` over 115200 baud serial.
The screen shows touch coordinates. Hold the screen to route microphone audio
to the attached PH1.25 speaker; release to disable the amplifier. Keep the
speaker away from the microphone to avoid feedback. No speaker detection is
available. Audio capture runs independently of the touch/rendering loop.

This target uses the existing speech partition boundaries, so uploading it from
an Arduino app replaces the Arduino OTA/FATFS layout as described in the
[recorder guide](../20-recorder/README.md). It does not upload speech models.
Build verified; physical SD, touch and full-duplex playback are **UNKNOWN**.
