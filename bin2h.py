import sys

import os
ROOT = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(ROOT, 'daemon_src', 'daemon'), 'rb') as f:
    data = f.read()

with open(os.path.join(ROOT, 'daemon_raw.h'), 'w') as out:
    out.write('unsigned char g_daemon_raw[] = {\n')
    for i in range(0, len(data), 16):
        row = data[i:i+16]
        hex_vals = ', '.join(f'0x{b:02x}' for b in row)
        out.write('    ' + hex_vals + ',\n')
    out.write('};\n')
    out.write(f'#define DAEMON_RAW_SIZE {len(data)}\n')

print(f'daemon_raw.h created, size={len(data)}')
