#!/usr/bin/env python3
"""Verify monitor SD transfers and actual LCD output through PlatformIO serial.

Writes/replaces the two bundled photos on SD. Requires Pillow. Uses temporary
photo previews and leaves the saved idle settings unchanged.
"""
import argparse
import os, pty, select, subprocess, time, json, struct, zlib, runpy
from pathlib import Path
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='/dev/ttyACM0')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--skip-upload', action='store_true', help='Read back existing photos without replacing SD files')
parser.add_argument('--transfer-only', action='store_true', help='Check uploads without lengthy LCD readbacks')
args = parser.parse_args()
out = args.output
out.mkdir(parents=True, exist_ok=True)
helper = runpy.run_path(str(root / 'scripts/capture_monitor_screen.py'))
master, slave = pty.openpty()
env = dict(os.environ, PLATFORMIO_SETTING_ENABLE_TELEMETRY='no')
p = subprocess.Popen(['pio', 'device', 'monitor', '-p', args.port, '-b', '115200', '--dtr', '1', '--rts', '1'], stdin=slave, stdout=slave, stderr=slave, env=env)
os.close(slave)
buf = b''
lines = []
seq = 0
log = (out / 'serial.log').open('w')

def read(timeout=1):
    global buf
    end = time.monotonic() + timeout
    got = []
    while time.monotonic() < end:
        if select.select([master], [], [], 0.05)[0]:
            buf += os.read(master, 65536)
            while b'\n' in buf:
                line, buf = buf.split(b'\n', 1)
                s = line.decode(errors='replace').strip()
                lines.append(s)
                got.append(s)
                log.write(s + '\n')
                log.flush()
    return got

def send(cmd):
    os.write(master, (cmd + '\n').encode())

def request(op, payload=b'', expected=None):
    global seq
    seq += 1
    send('photo-packet ' + (bytes([op]) + struct.pack('<H', seq) + payload).hex())
    end = time.monotonic() + 10
    while time.monotonic() < end:
        for s in read(0.02):
            if s.startswith('monitor_photo_reply '):
                reply = json.loads(s.split(' ', 1)[1])
                if reply['seq'] == seq:
                    if expected and reply['state'] != expected:
                        raise RuntimeError(str(reply))
                    return reply
    raise TimeoutError('packet ' + str(seq))

def capture(name):
    start = len(lines)
    send('screenshot')
    end = time.monotonic() + 35
    while time.monotonic() < end:
        if 'monitor_screenshot end' in read(0.2):
            helper['save_png'](out / (name + '.png'), helper['decode_rows'](lines[start:]))
            print('captured', name, flush=True)
            return
    raise TimeoutError('screenshot')
try:
    read(3)
    send('health')
    send('photo-list')
    read(2)
    print('\n'.join((s for s in lines if s.startswith(('monitor_health', 'monitor_photos', 'sd_', '{')))), flush=True)
    request(6, expected='cancelled')
    state = request(4, expected='ready')
    print('list', state, flush=True)
    if not state.get('mounted'):
        raise RuntimeError('SD directory unavailable')
    from PIL import Image, ImageOps
    for number, name in enumerate([] if args.skip_upload else ['01-alpine-lake', '02-red-poppy']):
        im = ImageOps.fit(Image.open(root / 'apps/27-photo-frame/assets' / f'{name}.png').convert('RGB'), (320, 196))
        pixels = b''.join((struct.pack('<H', (r & 248) << 8 | (g & 252) << 3 | b >> 3) for r, g, b in (im.get_flattened_data() if hasattr(im, 'get_flattened_data') else im.getdata())))
        frame = b'FPF1' + struct.pack('<HHII', 320, 196, len(pixels), zlib.crc32(pixels)) + pixels
        if number == 0:
            request(1, struct.pack('<II', len(frame), zlib.crc32(frame)) + b'bad-test.fpf', expected='receiving')
            request(2, struct.pack('<I', 0) + frame[:100], expected='receiving')
            request(3, expected='checksum_or_size_failed')
        start = time.monotonic()
        request(1, struct.pack('<II', len(frame), zlib.crc32(frame)) + f'{name}.fpf'.encode(), expected='receiving')
        for offset in range(0, len(frame), 100):
            request(2, struct.pack('<I', offset) + frame[offset:offset + 100], expected='receiving')
        request(3, expected='complete')
        print('uploaded', name, 'seconds', round(time.monotonic() - start, 1), flush=True)
    if args.transfer_only:
        send('health')
        read(2)
        print('SUCCESS: transfers', flush=True)
        raise SystemExit(0)
    send('photo-preview')
    read(3)
    capture('photo-1')
    send('health')
    read(2)
    send('photo-preview')
    read(1)
    capture('photo-2')
    send('photo-off')
    read(2)
    capture('normal-ui')
    send('health')
    read(3)
    print('\n'.join((s for s in lines if s.startswith(('monitor_health', 'monitor_photos frame', 'monitor_photos idle', 'monitor_audio'))))[-4500:], flush=True)
    print('SUCCESS', flush=True)
finally:
    os.write(master, b'\x03')
    try:
        p.wait(timeout=3)
    except subprocess.TimeoutExpired:
        p.terminate()
        p.wait(timeout=3)
    os.close(master)
    log.close()
