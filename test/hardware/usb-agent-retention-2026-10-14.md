# USB agent status retention — 2026-10-14

At the owner's request, PlatformIO Core CLI built and uploaded monitor **0.8.1**
to FNK0104B B81F3FC39F94 through `/dev/ttyACM0`. All five bootloader, partition,
OTA-initializer, model and application hashes verified; watchdog reset completed.
Existing partition boundaries and NVS were preserved. No full-chip erase,
eFuse or security-setting change occurred.

Local configured application SHA-256:
`d144ade2548d53732a2ce8ddf2e34acbfe1cf4f762c92e09950d733a5ffea863`.
This local build contains the owner's ignored bridge configuration and is not
the public release image. Public assets are built from the clean GitHub checkout.

The board returned to Windows as 303a:8360 with HID, TinyUSB UAC1 and COM9.
PlatformIO monitored at 115200 baud through a temporary loopback COM9 relay,
preserving Windows HID ownership. The relay and monitor were closed after checks.

## Observed runtime

- Live `v.oai.thstatus` revisions 3–6 were decoded, acknowledged and displayed.
  Slot 0 was blue `304ffe`; slots 1–5 were white `ffffff`.
- At revision 4, `micro-status`, `micro-repaint` and `micro-status` reported
  identical retained values. The full repaint at uptime 46,738 ms preserved
  those values before the next host update at 46,915 ms. No agent tap or
  synthetic HID key/status was sent for this check.
- Health: version 0.8.1, Wi-Fi connected (`wifi=3`), status and voice workers
  present, 37,443 free internal heap bytes and 8,318,396 free PSRAM bytes.
  Saved layout was three slots with avatars enabled.
- Audio: ready, USB microphone muted, no USB stream and zero dropped USB reports.
  Speech reported `starting` while USB mode was active. No panic/reset appeared
  during this short capture.

## Validation and limits

All 38 native tests and 44 host tests passed. PlatformIO monitor/HID diagnostic
builds passed. Native replay verifies that off/black lighting and quiet reads
preserve the LCD cache, explicit white idle changes it, partial/empty updates
preserve other slots, and real connection invalidation clears it.

This runtime check verifies normal Windows receipt and a local repaint. It does
not reproduce the Mac failure or physically verify blackout retention. Mac
acceptance remains UNKNOWN until the owner tests. If Desktop reports completion
only by turning a slot black, the requested policy retains the last working
state until a nonzero update or disconnect.
