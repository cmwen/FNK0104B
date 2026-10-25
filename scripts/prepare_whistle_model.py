#!/usr/bin/env python3
"""Download a pinned Whistle model, verify it, and prepare the depth-4 image.

Requires numpy. Downloads are explicit; ordinary firmware builds stay offline.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REVISION = '741118687e1f008f06d1351a402a731274789790'
SOURCE_SHA256 = 'b6e02f048568ac5d01a2042556c658061e699acbc0aa2a1439f52f3d461dffeb'
IMAGE_SHA256 = 'e2ccbe2d342a8438ec9a7610809ad095acdbcc359b959202fe18dc4fef66dbb1'
MODEL_SIZE = 14515887
PARTITION_SIZE = 0xdf0000
DEFAULT_DIRECTORY = ROOT / '.pio/models/whistle'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(directory):
    image = directory / 'whistle_d4.cact'
    manifest = json.loads((directory / 'manifest.json').read_text())
    if (manifest['source_revision'] != REVISION or manifest['source_sha256'] != SOURCE_SHA256
            or image.stat().st_size != MODEL_SIZE or image.stat().st_size > PARTITION_SIZE
            or manifest['depth'] != 4 or manifest['sha256'] != IMAGE_SHA256
            or digest(image) != IMAGE_SHA256):
        raise ValueError('Whistle model identity, size or checksum mismatch; prepare it again')
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=DEFAULT_DIRECTORY)
    parser.add_argument('--source', type=Path, help='Use an already downloaded official whistle.cact')
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if args.verify:
        print(verify(args.directory))
        return
    args.directory.mkdir(parents=True, exist_ok=True)
    source = args.source or args.directory / 'whistle.cact'
    if not source.exists():
        if args.source:
            parser.error('--source does not exist')
        url = f'https://huggingface.co/Cactus-Compute/whistle/resolve/{REVISION}/whistle.cact'
        temporary = source.with_suffix('.download')
        try:
            with urllib.request.urlopen(url, timeout=120) as response, temporary.open('wb') as output:
                while block := response.read(1024 * 1024):
                    output.write(block)
            if digest(temporary) != SOURCE_SHA256:
                raise ValueError('Downloaded source model checksum mismatch')
            temporary.replace(source)
        finally:
            temporary.unlink(missing_ok=True)
    if digest(source) != SOURCE_SHA256:
        raise ValueError('Source model checksum mismatch')
    image = args.directory / 'whistle_d4.cact'
    subprocess.run([sys.executable, str(ROOT / 'apps/29-whistle/tools/whistle_strip.py'),
                    str(source), '4', str(image)], check=True)
    manifest = {'source_revision': REVISION, 'source_sha256': SOURCE_SHA256,
                'depth': 4, 'sha256': digest(image), 'size': image.stat().st_size}
    (args.directory / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(verify(args.directory))


if __name__ == '__main__':
    main()
