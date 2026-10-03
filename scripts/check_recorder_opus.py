#!/usr/bin/env python3
"""Optional host interoperability check; needs C++, pkg-config, libopus, FFmpeg."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="fnk-recorder-") as folder:
        fixture = pathlib.Path(folder) / "fixture"
        flags = subprocess.check_output(["pkg-config", "--cflags", "--libs", "opus"], text=True).split()
        subprocess.run(["c++", "-std=c++17", "-I" + str(ROOT / "lib/recorder/src"),
                        str(ROOT / "test/native/recorder_opus_fixture.cpp"), *flags, "-o", str(fixture)], check=True)
        for samples in (1, 104, 200, 216, 217, 320, 512, 16001, 47213):
            output = pathlib.Path(folder) / f"{samples}.opus"
            subprocess.run([str(fixture), str(output), str(samples)], check=True)
            # Ogg granules and pre-skip use 48 kHz. Avoid resampler delay for
            # the single-sample fixture by verifying decoder output at 48 kHz.
            result = subprocess.run(["ffmpeg", "-v", "error", "-i", str(output), "-ar", "48000",
                                     "-ac", "1", "-f", "s16le", "-"], check=True, stdout=subprocess.PIPE)
            assert len(result.stdout) == samples * 3 * 2, (samples, len(result.stdout))
    print("9 Opus fixtures passed FFmpeg decoding and exact duration checks")


if __name__ == "__main__":
    main()
