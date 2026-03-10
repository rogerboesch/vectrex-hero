"""Tkinter Canvas vector renderer for Vectrex emulation."""

import tkinter as tk

from .via6522 import ALG_MAX_X, ALG_MAX_Y, VECTREX_COLORS

SCREEN_WIDTH = 330 * 3 // 2   # 495
SCREEN_HEIGHT = 410 * 3 // 2  # 615

# Pre-compute color table: brightness index → hex color string
_COLOR_TABLE = []
for _i in range(VECTREX_COLORS):
    _c = _i * 256 // VECTREX_COLORS
    if _c > 255:
        _c = 255
    _h = '#{0:02x}{0:02x}{0:02x}'.format(_c)
    _COLOR_TABLE.append(_h)


class VectrexRenderer:
    __slots__ = ('canvas', 'scl_factor', 'offx', 'offy', '_pool', '_pool_used')

    def __init__(self, canvas):
        self.canvas = canvas

        sclx = ALG_MAX_X / SCREEN_WIDTH
        scly = ALG_MAX_Y / SCREEN_HEIGHT
        self.scl_factor = max(sclx, scly)

        self.offx = (SCREEN_WIDTH - ALG_MAX_X / self.scl_factor) / 2
        self.offy = (SCREEN_HEIGHT - ALG_MAX_Y / self.scl_factor) / 2

        self._pool = []
        self._pool_used = 0

    def render(self, vectors, count):
        canvas = self.canvas
        pool = self._pool
        scl = self.scl_factor
        ox = self.offx
        oy = self.offy
        old_used = self._pool_used

        drawn = 0
        for i in range(count):
            v = vectors[i]
            color_idx = v[4]
            if color_idx == 0 or color_idx >= VECTREX_COLORS:
                continue

            x0 = ox + v[0] / scl
            y0 = oy + v[1] / scl
            x1 = ox + v[2] / scl
            y1 = oy + v[3] / scl
            fill = _COLOR_TABLE[color_idx]

            if drawn < len(pool):
                item = pool[drawn]
                canvas.coords(item, x0, y0, x1, y1)
                canvas.itemconfigure(item, fill=fill, state='normal')
            else:
                item = canvas.create_line(x0, y0, x1, y1, fill=fill, width=1)
                pool.append(item)

            drawn += 1

        for j in range(drawn, old_used):
            canvas.itemconfigure(pool[j], state='hidden')

        self._pool_used = drawn
