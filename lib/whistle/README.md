# Whistle engine attribution

Vendored from [andrisgauracs/whistle-esp32](https://github.com/andrisgauracs/whistle-esp32),
commit [`383e0088d7140a89a9111aa985671190a9048aa3`](https://github.com/andrisgauracs/whistle-esp32/commit/383e0088d7140a89a9111aa985671190a9048aa3).
The portable C99 engine, ESP32-S3 int8 kernels and model container support are
upstream work. The selected engine source/header files are copied unchanged;
Needle-only implementation files are omitted. `nd_model.h` provides shared
allocation macros; `nd_ledcmd.c` provides the UTF-8 lowercase utility used by
Whistle. The firmware does not enable LED commands or color keyword bias.

Upstream code is Apache-2.0; the full license is retained in [LICENSE](LICENSE).
The model and weights are by **Cactus Compute**, distributed separately at
[Cactus-Compute/whistle](https://huggingface.co/Cactus-Compute/whistle), also under
Apache-2.0 according to its model card and the upstream project. Weights are not
committed to this repository. The copied model tools in `apps/29-whistle/tools/`
come from the same upstream commit and use the same license.

FNK0104B integration adds the existing ES8311 microphone path, touch UI,
PlatformIO environment, conservative recording limit and verified model upload.
See [the app guide](../../apps/29-whistle/README.md).
