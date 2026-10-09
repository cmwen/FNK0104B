# Independent firmware builds and guide publishing

The repository has three GitHub Actions workflows. Documentation and browser
configuration changes no longer start a firmware matrix.

| Workflow | Automatic triggers | Result |
| --- | --- | --- |
| Publish board guide (`pages.yml`) | Guide, documentation, browser setup and relevant packaging changes on main; matching pull requests; successful main firmware builds | Build and check the guide/configuration UI, reuse verified firmware, publish Pages on main |
| Build firmware (`firmware.yml`) | Firmware source, hardware libraries, toolchain, components and build scripts; matching pull requests | Build changed or uncached named PlatformIO environments, verify and retain a complete portable catalog |
| Host checks (`checks.yml`) | Called by firmware builds; independently on bridge, test and publishing-helper changes | Native logic, bridge and packaging tests without flashing hardware |

Firmware catalog publication waits for its reusable host-check job as well as
all required firmware builds.

Every workflow can also be run independently from Actions with **Run workflow**.
A docs-only or browser-settings change runs the guide workflow, without installing
PlatformIO or waiting for the firmware build matrix. Native and bridge test-only
changes run Host checks without compiling the ESP32 apps. Changes spanning code
and docs can run both independently; the guide initially keeps the last successful
firmware and republishes when the new main firmware build succeeds.

## How the guide keeps installable firmware

Build firmware retains exact per-environment fingerprint caching. It verifies and
uploads a complete `firmware-catalog` artifact with 90-day retention. The guide
uses `scripts/fetch_firmware_catalog.py` to select the latest successful main
firmware run that has a complete, unexpired catalog. During migration it also
accepts complete per-environment artifacts from the former combined workflow.
Pull-request, failed and unrelated workflow runs are never firmware sources.

Every image is checked against its bundle SHA256 metadata before packaging.
New bundles retain their original flash offsets and partition limits as well, so
new guide sources cannot change where an older image is flashed. Firmware
manifests preserve their original build revisions; the guide catalog
records the current website revision. A newer guide does not relabel older
firmware as freshly compiled. The full catalog and local links are checked before
publishing. Firmware remains excluded from the browser service-worker cache.

The published guide also retains each firmware bundle's hash/provenance metadata.
If all Actions artifacts expire, the publisher downloads these verified images
from the existing Pages site instead. This durable fallback preserves their
original build revisions and still never compiles firmware. If neither source
exists (for example, the first migration publication), run **Build firmware** on
main once, then retry **Publish board guide**. A missing or corrupt source fails
publication rather than deploying a partial installer; existing Pages stays
available. The old deployed site gains this fallback metadata on the first
successful publication of the split workflow.

Firmware success on main triggers the publisher through `workflow_run`. The
publisher checks out current main for that event, so finishing an older build
cannot restore an older documentation checkout. Publication is serialized.
Pull requests build and validate the guide but do not publish it. The old combined
workflow name is temporarily recognized to pick up an in-flight migration build.

## Validation

Run `python3 -m unittest discover -s test/host` and
`actionlint .github/workflows/*.yml`. Source-selection tests reject non-main,
failed, unrelated, incomplete and expired sources, check every downloaded image,
verify that expired artifacts use the published catalog, reject corrupt or unsafe
published images, and ensure missing artifacts never invoke a firmware build.

For an end-to-end packaging check, first build the browser assets and guide, then:

```sh
python3 scripts/fetch_firmware_catalog.py --repository cmwen/FNK0104B --output /tmp/fnk-firmware-catalog
python3 scripts/package_web_firmware.py --reuse-dir /tmp/fnk-firmware-catalog --output /tmp/fnk-guide-pages
GUIDE_DIST_DIR=/tmp/fnk-guide-pages npm test --prefix site
```

GitHub's [workflow events](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows)
and [artifact downloads](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/download-workflow-artifacts)
document the cross-workflow trigger and download mechanisms.
