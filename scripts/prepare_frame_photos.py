#!/usr/bin/env python3
"""Prepare 320x240 BMP, JPEG, PNG or WebP photos for the SD photo frame."""
import argparse
from pathlib import Path

from PIL import Image, ImageOps


def prepare(source: Path, destination: Path, output_format: str = "bmp") -> None:
    if destination.exists():
        raise FileExistsError(f"Refusing to overwrite {destination}")
    with Image.open(source) as image:
        oriented = ImageOps.exif_transpose(image).convert("RGB")
        fitted = ImageOps.fit(oriented, (320, 240), method=Image.Resampling.LANCZOS)
        destination.parent.mkdir(parents=True, exist_ok=True)
        formats = {"bmp": "BMP", "jpg": "JPEG", "png": "PNG", "webp": "WEBP"}
        options = {"quality": 90} if output_format in ("jpg", "webp") else {}
        fitted.save(destination, format=formats[output_format], **options)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", nargs="+", type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--format", choices=("bmp", "jpg", "png", "webp"), default="bmp")
    args = parser.parse_args()
    for source in args.sources:
        destination = args.output_dir / (source.stem + "." + args.format)
        prepare(source, destination, args.format)
        print(destination)


if __name__ == "__main__":
    main()
