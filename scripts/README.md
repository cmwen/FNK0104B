# Scripts

`use_standalone_gdb.py` selects PlatformIO's pinned standalone Xtensa GDB for
the `hello-debug` environment. The release firmware environments do not use it.

`99-fnk0104b-jtag.rules` grants this host's `plugdev` group access to the
board's observed USB JTAG ID. See `docs/development.md` for installation and
verification; it is not applied by a normal build.

## Monitor UI preview

`monitor_ui_preview.cpp` renders the shared firmware theme with the pinned
TFT_eSPI font tables. Build the monitor once to fetch its dependencies, then:

```sh
c++ -std=c++17 -I lib/ui/src -I .pio/libdeps/codex-monitor/TFT_eSPI scripts/monitor_ui_preview.cpp -o /tmp/monitor-ui-preview
/tmp/monitor-ui-preview /tmp/monitor-ui
magick /tmp/monitor-ui-idle.ppm -filter point -resize 300% apps/codex-monitor/preview.png
```

It writes idle, active, attention, offline, recording, command listening (loud
and quiet), status with an active agent, empty-quota and full-quota PPM previews. These check drawing and text at 320×240; touch and physical display
appearance still require the board.

`generate_monitor_icons.py` regenerates the tintable icon masks from the owner's
two reference images using ImageMagick. Run it with `--status-reference` and
`--screen-reference` pointing to those originals, and `--output` pointing to
`lib/ui/src/ui/monitor_icons.hpp`. The crop coordinates are specific to those
references, which are not bundled in the repository.

## Recorder UI preview

`recorder_ui_preview.py` uses the recorder's actual C++ drawing routine and
the shared IDF display font. It substitutes a host framebuffer for display
access and renders sample status and filenames; it does not capture the device
through USB or photograph the screen. Regenerate the documentation image with:

```sh
python3 scripts/recorder_ui_preview.py /tmp/recorder-preview.ppm
ffmpeg -v error -y -i /tmp/recorder-preview.ppm -frames:v 1 docs/images/recorder-ui.png
```

The Pages packager copies `docs/images/` into the deployed site's same path.
The web app displays this image in its Voice recorder preview section.

## Actual monitor display capture

`capture_monitor_screen.py` captures the physical LCD's display memory through
the monitor firmware's `screenshot` serial command. It uses PlatformIO Core
inside a pseudo-terminal, with no direct pyserial device access. The firmware
reads one RGB row at a time through the shared board display support. The script
rejects missing/duplicate/malformed rows and writes a PNG using Python's standard
library. Close other serial terminals before running:

```sh
python3 scripts/capture_monitor_screen.py docs/images/codex-monitor-screen.png --port /dev/ttyACM0
```

Unlike `monitor_ui_preview.cpp`, this captures the actual board and current
values. `docs/images/codex-monitor-screen.png` is copied into the Pages artifact
and shown in the Codex monitor screenshot section.

Additional monitor documentation previews use the same host renderer: convert its `commands`, `recording`, `idle` and `offline` PPM outputs to `docs/images/monitor-command.png`, `monitor-recording.png`, `monitor-idle.png` and `monitor-offline.png`. These contain sample data and are labeled host previews in the Astro guide.
