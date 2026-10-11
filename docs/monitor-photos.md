# Optional monitor idle photos

Monitor firmware 0.8.0 adds three saved idle choices to the BLE configuration
page: keep the monitor UI, blank the area below the usage bar, or show SD photos.
The default is to keep the monitor UI. The live usage/status bar occupies the
top 44 pixels in every idle view; photos fill the remaining 320 × 196 pixels.

Connect through **Monitor settings**, choose an idle mode, set the idle delay
(5–7200 seconds) and photo interval (5–3600 seconds), then save. Choose an idle
delay shorter than the screen-off timeout. For example, photos after 60 seconds
and screen off after 10 minutes display photos for the intervening nine minutes.
Photo changes do not restart the screen-off countdown. Work and voice activity
prevent the idle view; a touch restores the controls. Existing screen-off and
wake rules still apply.

Choose a JPEG, PNG, WebP or BMP in **Photos on the SD card**. The browser applies
image orientation, center crops and resizes it, then previews the result before
upload. Transparent areas have a black background. Upload sends a prepared
RGB565 frame to `/monitor-photos` on the SD card. The monitor can keep eight
photos, sorted by filename, with upload, refresh and delete controls. Uploading
an existing name replaces that photo. Names are sanitized to 20 characters
plus `.fpf`; names with the same sanitized prefix will replace each other.

The standalone `photo-frame` firmware still supports its original image formats
and `/photos` directory. Monitor photos use a separate directory and do not
need JPEG, PNG or WebP decoders on the board. Each prepared file is 125,456
bytes, including its 16-byte header. Only the current frame is cached in
PSRAM (125,440 bytes), loaded in 4 KiB steps and released on leaving the idle
view. Missing cards, invalid photos and allocation failures leave the area
below the bar blank. Voice and firmware updates take priority over SD work.
The monitor enables FAT long filenames with a 64-character limit and a temporary
heap workspace; the previous 8.3-only build could mount this card but rejected
the photo directory with `EINVAL`.

## Transfer and interruption handling

The existing settings BLE service exposes additional read/write characteristics:

| UUID suffix | Purpose |
| --- | --- |
| `0007` | Six-byte settings: version 1, mode, delay LE16, interval LE16 |
| `0008` | Bounded file requests and JSON acknowledgement/readback |

The full UUID pattern is `4e4b0104-0007-4d20-8f4b-0104b0000001`, replacing
`0007` with `0008` for transfers. The page sends 180-byte file chunks with
sequence numbers and offsets, waits for each acknowledgement, and checks the
accepted offset. Firmware checks the total size, file CRC32 and frame header
before committing, then checks the pixel CRC32 before displaying. A cancel,
BLE disconnect or 30-second transfer timeout removes the incomplete file.
Replacement keeps the old file as a named backup until the new file commits;
the next mount restores that backup if the target disappeared during a reset.
FAT filesystem writes are not a guarantee against SD corruption on power loss.

Older monitor firmware leaves these controls disabled. Native tests cover the
file format, settings, paths and idle timers; browser tests cover conversion,
acknowledgements, cancellation and delay validation. Physical BLE throughput
and coexistence must be established separately from these tests.

## Hardware diagnostics

Use PlatformIO Core serial monitoring at 115200 baud. `health` reports monitor
memory and worker state. `photo-list` checks the SD directory, `photo-preview`
shows photos temporarily for 30 seconds, and `photo-off` ends that preview.
These previews do not save the idle mode. `screenshot` reads the actual LCD.
`photo-packet <lowercase hex>` feeds a maximum 200-byte request through the
same mailbox and transfer processor as BLE, returning `monitor_photo_reply`
JSON. It is a serial diagnostic for SD, offsets and checksums; it does not
establish that a physical BLE upload works.
Serial test senders should use at most 100 file bytes per request and wait for
each acknowledgement: the hex command must fit Arduino CDC's 256-byte receive
queue. BLE uses binary packets and the page's 180-byte chunks.

`scripts/check_monitor_photos.py` repeats these checks using the bundled images
and requires Pillow. Run with `--output <capture-directory> --transfer-only`
to upload and validate the two files, then with `--output <capture-directory>
--skip-upload` to capture the LCD. Supply `--port` when the serial port differs
from `/dev/ttyACM0`. The transfer check replaces the two sample filenames on SD;
the display check temporarily previews them without saving idle settings.

Keep this optional feature disabled until the combined monitor works on the
target board. See [monitor readiness](monitor-readiness.md) for device evidence
and outstanding physical checks.
