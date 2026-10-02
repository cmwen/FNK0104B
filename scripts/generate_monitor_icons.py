#!/usr/bin/env python3
"""Convert supplied monitor references into small, tintable firmware masks."""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--status-reference', required=True, type=Path)
    parser.add_argument('--screen-reference', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=Path('lib/ui/src/ui/monitor_icons.hpp'))
    args = parser.parse_args()
    # Pixel bounds measured in the owner's 1774x887 status / 1448x1086 screen refs.
    crops = [('Wifi', args.status_reference, '88x65+112+263', 24, 18),
             ('Robot', args.status_reference, '78x75+465+259', 24, 24),
             ('Clock', args.screen_reference, '88x87+93+232', 16, 16),
             ('Calendar', args.screen_reference, '81x77+797+235', 16, 16),
             ('Microphone', args.screen_reference, '92x145+675+751', 18, 28)]
    lines = ['#pragma once', '// Tintable masks derived from owner-supplied UI references.',
             '#include <stdint.h>', '#ifdef ARDUINO', '#include <pgmspace.h>',
             '#else', '#define PROGMEM', '#endif', 'namespace ui { namespace monitor_icons {']
    for name, path, crop, width, height in crops:
        raw = subprocess.check_output(['magick', str(path), '-crop', crop, '+repage',
                                       '-resize', f'{width}x{height}!', '-colorspace', 'gray',
                                       '-auto-level', '-depth', '8', 'gray:-'])
        assert len(raw) == width * height
        values = [0 if b < 35 else b for b in raw]
        lines.append(f'static constexpr int k{name}Width = {width}, k{name}Height = {height};')
        lines.append(f'static const uint8_t k{name}[] PROGMEM = {{')
        lines.extend('  ' + ','.join(map(str, values[i:i+width])) + ',' for i in range(0, len(values), width))
        lines.append('};')
    lines.append('} }  // namespace ui::monitor_icons')
    args.output.write_text('\n'.join(lines)+'\n')


if __name__ == '__main__':
    main()
