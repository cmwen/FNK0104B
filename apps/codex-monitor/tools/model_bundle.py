"""Canonicalize the pinned ESP-SR pack_model.py format for app-only OTA.

The upstream packer walks directories without sorting. Keep its format and file
contents, but sort model/file names so identical weights have identical hashes.
"""
import struct


def canonicalize(data):
    data = bytes(data)
    if len(data) < 4:
        raise ValueError('Truncated speech model bundle')
    count, = struct.unpack_from('<I', data)
    if not 0 < count <= 64:
        raise ValueError('Invalid model count')
    position = 4
    models = {}
    def name_at(offset):
        field = data[offset:offset + 32]
        name = field.split(b'\0', 1)[0]
        if len(field) != 32 or not name or len(name) > 31 or b'/' in name or b'\\' in name:
            raise ValueError('Invalid model/file name')
        name.decode('ascii')
        return name
    for _ in range(count):
        if position + 36 > len(data):
            raise ValueError('Truncated model header')
        model = name_at(position)
        files, = struct.unpack_from('<I', data, position + 32)
        position += 36
        if model in models or not 0 < files <= 256:
            raise ValueError('Duplicate model or invalid file count')
        entries = {}
        for _ in range(files):
            if position + 40 > len(data):
                raise ValueError('Truncated file header')
            name = name_at(position)
            offset, size = struct.unpack_from('<II', data, position + 32)
            position += 40
            if name in entries or offset > len(data) or size > len(data) - offset:
                raise ValueError('Duplicate file or invalid data range')
            entries[name] = (offset, size)
        models[model] = entries
    ranges = sorted(value for files in models.values() for value in files.values())
    end = position
    for offset, size in ranges:
        if offset != end:
            raise ValueError('Overlapping or incomplete model data')
        end += size
    if end != len(data):
        raise ValueError('Trailing model data')
    header = bytearray(struct.pack('<I', count))
    payload = bytearray()
    for model, entries in sorted(models.items()):
        header += struct.pack('<32sI', model, len(entries))
        for name, (offset, size) in sorted(entries.items()):
            header += struct.pack('<32sII', name, position + len(payload), size)
            payload += data[offset:offset + size]
    return bytes(header + payload)
