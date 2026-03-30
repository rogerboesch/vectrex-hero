#!/usr/bin/env python3
"""Quick test of the _iql extension module."""

import sys
import time

sys.path.insert(0, '.')
import _iql

print('Initializing emulator...')
_iql.init('/Users/roger/Documents/iQLmac/')
time.sleep(0.5)

# Send a few ticks
for i in range(25):
    _iql.tick()
    time.sleep(0.02)

print('Running:', _iql.is_running())
fb = _iql.get_framebuffer()
if fb:
    print('Framebuffer size:', len(fb), 'bytes')
    nz = sum(1 for b in fb if b != 0)
    print('Non-zero bytes:', nz)
else:
    print('No framebuffer yet')

_iql.stop()
print('Stopped. Test passed.')
