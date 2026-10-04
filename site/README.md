# FNK0104B field guide

An Astro static site for board owners, focused on capabilities, useful interaction
patterns and lessons from the Codex monitor. Each capability is a Markdown file
in `content/features/`; `src/data/features.json` supplies catalog labels.
`src/data/firmware.json` describes every PlatformIO environment and links to the
canonical app README. Keep those labels aligned when adding firmware.

## Work locally

From the repository root, use Node 24:

```sh
npm ci --prefix web-flasher
npm run build --prefix web-flasher
npm ci --prefix site
ASTRO_TELEMETRY_DISABLED=1 npm run dev --prefix site
```

Open `http://localhost:4321/FNK0104B/`. Firmware binaries are generated during
packaging, so the installer reports firmware unavailable in a plain site preview.
Its BLE controls remain available in a compatible browser.

```sh
ASTRO_TELEMETRY_DISABLED=1 npm run build --prefix site
npm test --prefix site
python3 scripts/package_web_firmware.py --output /tmp/fnk0104b-pages
```

Packaging requires the published firmware environments to have already been
built with `python3 scripts/build_firmware.py`. CI builds the guide and packages
it with those images into one GitHub Pages artifact. The Astro base is `/FNK0104B`.
There is no second hosting service.

After building, `npm run test:browser --prefix site` starts a temporary local
static server, checks desktop/mobile navigation, search, legacy setup links and
firmware selection with a mocked catalog, then writes page screenshots to
`docs/site-previews/`. Install Chromium once with
`cd site && npx playwright install chromium` if it is not already available.
The browser check never connects to Bluetooth or installs firmware.

## Preserve installation and setup

The canonical installer stays in `../web-flasher/`. `prepare-assets.mjs` copies its
shell and bundle into generated `public/`, with its HTML named `setup.html`.
Do not edit generated assets. The guide home handles old `index.html#setup` and
`index.html#flash` links; firmware pages deep-link to the selected environment.
Firmware is excluded from service-worker caching. The erase choice, Security 1
provisioning and on-screen proof-of-possession entry remain in the existing flow.

## Screenshots and evidence

`docs/images/codex-monitor-screen.png` is actual LCD readback. The recorder and
alternate monitor screens are host previews rendered from firmware drawing code
with sample data. Regenerate them with the tools in `scripts/README.md`; label
provenance rather than implying a preview is a physical test. Firmware without a
screen capture says so explicitly. Never flash another app just to get a picture
without accounting for the partition changes.

This site does not implement HID, custom ML or deep sleep. Those are documented
experiments. Update `knowledge/`, the app README and dated hardware records when
new device facts are verified.

## Dependency note

Astro is pinned to 7.3.5. At implementation time, npm audit still reports an
unpatched `http-cache-semantics` advisory through Astro. This project produces
static files for GitHub Pages and has no server-side response cache; reassess
the dependency before introducing server rendering.
