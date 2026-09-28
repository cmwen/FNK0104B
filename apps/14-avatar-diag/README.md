# Avatar animation diagnostic

This standalone display diagnostic composes four deterministic 32×32 floating
robot avatars. Their pale helmets, dark visors, glowing eyes, code crests, and
side modules follow the supplied reference images. It enlarges them 3× in a
portrait 2×2 layout and cycles each agent through idle, thinking,
needs-input, and error states every eight seconds. It uses compiled-in art and
a reusable RGB565 buffer; it does not access SD, Wi-Fi, or the monitor app.

[Preview of the four sample agents and states](preview.png)

The state cues are visible at a glance: no frame for idle, cyan with a moving
ellipsis for thinking, amber with a question mark for needs input, and red
with an exclamation mark and alert expression for error. Each status also has
a large matching badge below its avatar.

Build with `pio run -e avatar-diag`. After the existing display diagnostic has
passed on the board, upload with `pio run -e avatar-diag -t upload` and inspect
`pio device monitor -b 115200`. Check that the four identities look different,
each state is visible, animation has no trails or obvious flicker, and colors
look correct. Serial output reports compose and display-transfer times, maximum
frame time, free heap, free PSRAM, and buffer sizes every five seconds.
The small red, green, and blue pairs at the top right compare direct TFT
drawing (left chip) with the RGB565 pixel buffer (right chip). Both chips in
each pair should match.

The configured app partition is 3 MiB. This diagnostic keeps its assets inside
the firmware image and does not change partitions.

## First board run (2026-09-28)

`pio run -e avatar-diag` succeeded and produced a 305,104-byte firmware image.
The same image uploaded to the connected ESP32-S3 and passed esptool hash
verification. The 115200-baud serial log reported 50 frames per five seconds
(10 FPS), average composition time 3.4 ms and average display transfer time
31.6 ms for all four avatars, with a maximum complete frame time of 35.1 ms.
It reported 349,068 bytes free heap and 8,386,295 bytes free PSRAM. These are
one board/run measurements, not guaranteed performance limits. Screen colors,
readability, and visual smoothness still need a human check on the panel.

## Reference-inspired redesign (2026-09-28)

The redesigned 306,208-byte firmware image uploaded and passed hash
verification. At 115200 baud, the board reported about 50 frames per five
seconds, average composition time 5.3 ms, average display transfer time
31.7 ms, and maximum complete frame time 37.1 ms for four avatars. It still
reported 349,068 bytes free heap and 8,386,295 bytes free PSRAM. The preview
was generated from the same renderer, but the physical display still needs a
visual check for color and readability.

## RGB565 and status update (2026-09-28)

The RGB565 buffer now uses TFT_eSPI's byte swap when sent to the display.
Previously the preview interpreted RGB565 values correctly while the display
received the two bytes in reverse order. The image now includes paired color
chips to check the correction on the panel. The new 307,152-byte image uploaded
and passed hash verification. Serial reported 50 frames per five seconds,
average composition time 5.6 ms, average transfer time 31.6 ms, and maximum
frame time 37.3 ms for all four avatars. The subsequent board photo showed
matching chip pairs but complementary colors across the entire display.

## Board photo review and portrait layout (2026-09-28)

The supplied photo showed that both the direct-drawn background and buffered
avatars had complementary colors. Sending the ILI9341 INVOFF command did not
correct them in the next photo, so shared FNK0104B display initialization now
sends INVON. The diagnostic also uses
the panel's portrait orientation and a large status badge under each avatar.
The subsequent board photo confirmed this panel-specific setting visually.

The portrait image is 307,760 bytes. It uploaded with esptool hash verification,
and the 115200-baud log reported 50 frames per five seconds, 5.6 ms average
composition, 31.6 ms average transfer, and 37.3 ms maximum complete frame
time. The avatar, display, calculator, Wi-Fi, connectivity, LocalLink, and
screen-timeout environments built after the shared display change; all 10
native tests passed.

After the second photo confirmed that INVOFF still showed complementary
colors, the same-size image was rebuilt with INVON. It uploaded with hash
verification and reported 49 frames in five seconds, 5.6 ms average
composition, 31.6 ms average transfer, and 37.3 ms maximum frame time.
The final board photo confirmed a dark background and faceplate, pale helmet,
expected red/green/blue test chips with matching pairs, upright avatars, and
readable idle, thinking, input, and error badges.
