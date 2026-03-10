"""Main emulator loop, Tk window, and keyboard input."""

import time
import tkinter as tk

from .renderer import VectrexRenderer, SCREEN_WIDTH, SCREEN_HEIGHT

try:
    from . import _vec2x
    _USE_C_EXT = True
except ImportError:
    _USE_C_EXT = False

VECTREX_MHZ = 1500000
EMU_TIMER = 20  # ms per tick
CYCLES_PER_TICK = (VECTREX_MHZ // 1000) * EMU_TIMER  # 30000
VECTREX_PDECAY = 30
FCYCLES_INIT = VECTREX_MHZ // VECTREX_PDECAY  # 50000 — frame swap interval


class Emulator:
    def __init__(self, rom_path, cart_path=None):
        # Read ROM/cart as bytes for both paths
        with open(rom_path, 'rb') as f:
            rom_data = f.read()
        if cart_path:
            with open(cart_path, 'rb') as f:
                cart_data = f.read()
        else:
            cart_data = b'\x00'

        if _USE_C_EXT:
            _vec2x.init(rom_data, cart_data)
            # Local input state for C ext path
            self._buttons = 0xFF
            self._jx = 0x80
            self._jy = 0x80
        else:
            from .via6522 import VIA6522
            from .memory import MemoryBus
            from .cpu6809 import CPU6809

            self.via = VIA6522()
            self.mem = MemoryBus(self.via)
            self.cpu = CPU6809()
            self.cpu.mem = self.mem

            self.mem.load_rom(rom_path)
            if cart_path:
                self.mem.load_cart(cart_path)

            self.fcycles = FCYCLES_INIT

        # Tk setup
        self.root = tk.Tk()
        self.root.title('Vec2X-Py')
        self.root.resizable(False, False)

        self.canvas = tk.Canvas(
            self.root, width=SCREEN_WIDTH, height=SCREEN_HEIGHT,
            bg='black', highlightthickness=0)
        self.canvas.pack()

        self.renderer = VectrexRenderer(self.canvas)

        self.root.bind('<KeyPress>', self._key_down)
        self.root.bind('<KeyRelease>', self._key_up)

    def _key_down(self, event):
        k = event.keysym.lower()
        if k == 'escape':
            self.root.destroy()
            return

        if _USE_C_EXT:
            if   k == 'a':     self._buttons &= ~0x01
            elif k == 's':     self._buttons &= ~0x02
            elif k == 'd':     self._buttons &= ~0x04
            elif k == 'f':     self._buttons &= ~0x08
            elif k == 'left':  self._jx = 0x00
            elif k == 'right': self._jx = 0xFF
            elif k == 'up':    self._jy = 0xFF
            elif k == 'down':  self._jy = 0x00
            else: return
            _vec2x.set_input(self._buttons, self._jx, self._jy)
        else:
            via = self.via
            if   k == 'a':     via.snd_regs[14] &= ~0x01
            elif k == 's':     via.snd_regs[14] &= ~0x02
            elif k == 'd':     via.snd_regs[14] &= ~0x04
            elif k == 'f':     via.snd_regs[14] &= ~0x08
            elif k == 'left':  via.alg_jch0 = 0x00
            elif k == 'right': via.alg_jch0 = 0xFF
            elif k == 'up':    via.alg_jch1 = 0xFF
            elif k == 'down':  via.alg_jch1 = 0x00

    def _key_up(self, event):
        k = event.keysym.lower()

        if _USE_C_EXT:
            if   k == 'a':     self._buttons |= 0x01
            elif k == 's':     self._buttons |= 0x02
            elif k == 'd':     self._buttons |= 0x04
            elif k == 'f':     self._buttons |= 0x08
            elif k == 'left' or k == 'right': self._jx = 0x80
            elif k == 'up' or k == 'down':    self._jy = 0x80
            else: return
            _vec2x.set_input(self._buttons, self._jx, self._jy)
        else:
            via = self.via
            if   k == 'a':     via.snd_regs[14] |= 0x01
            elif k == 's':     via.snd_regs[14] |= 0x02
            elif k == 'd':     via.snd_regs[14] |= 0x04
            elif k == 'f':     via.snd_regs[14] |= 0x08
            elif k == 'left' or k == 'right': via.alg_jch0 = 0x80
            elif k == 'up' or k == 'down':    via.alg_jch1 = 0x80

    def reset(self):
        if _USE_C_EXT:
            _vec2x.reset()
            self._buttons = 0xFF
            self._jx = 0x80
            self._jy = 0x80
        else:
            self.via.reset()
            self.mem.reset()
            self.cpu.reset()
            self.fcycles = FCYCLES_INIT

    def _tick(self):
        if _USE_C_EXT:
            vectors = _vec2x.emu_tick(CYCLES_PER_TICK)
            if vectors is not None:
                # DEBUG: dump vectors every ~2s
                if not hasattr(self, '_dbg_cnt'):
                    self._dbg_cnt = 0
                self._dbg_cnt += 1
                if self._dbg_cnt % 100 == 1:
                    colors = {}
                    for v in vectors:
                        c = v[4]
                        colors[c] = colors.get(c, 0) + 1
                    print(f'[frame {self._dbg_cnt}] {len(vectors)} vectors, colors: {dict(sorted(colors.items()))}')
                    scl = self.renderer.scl_factor
                    ox, oy = self.renderer.offx, self.renderer.offy
                    # Show ALL color-95 vectors for one frame
                    c95 = [(v, ox+v[0]/scl, oy+v[1]/scl, ox+v[2]/scl, oy+v[3]/scl) for v in vectors if v[4] == 95]
                    c127 = [(v, ox+v[0]/scl, oy+v[1]/scl, ox+v[2]/scl, oy+v[3]/scl) for v in vectors if v[4] == 127]
                    print(f'  c95 ({len(c95)} vecs):')
                    for v, sx0, sy0, sx1, sy1 in c95[:8]:
                        length = ((sx1-sx0)**2 + (sy1-sy0)**2)**0.5
                        print(f'    scr=({sx0:.1f},{sy0:.1f})->({sx1:.1f},{sy1:.1f}) len={length:.1f}')
                    print(f'  c127 ({len(c127)} vecs):')
                    for v, sx0, sy0, sx1, sy1 in c127[:5]:
                        length = ((sx1-sx0)**2 + (sy1-sy0)**2)**0.5
                        print(f'    scr=({sx0:.1f},{sy0:.1f})->({sx1:.1f},{sy1:.1f}) len={length:.1f}')
                    # Show longest color-0 vectors (likely cave walls at wrong intensity)
                    c0 = [(v, ox+v[0]/scl, oy+v[1]/scl, ox+v[2]/scl, oy+v[3]/scl) for v in vectors if v[4] == 0]
                    c0_with_len = [(t, ((t[3]-t[1])**2 + (t[4]-t[2])**2)**0.5) for t in c0]
                    c0_with_len.sort(key=lambda x: -x[1])
                    print(f'  c0 longest ({len(c0)} vecs):')
                    for (v, sx0, sy0, sx1, sy1), length in c0_with_len[:8]:
                        print(f'    scr=({sx0:.1f},{sy0:.1f})->({sx1:.1f},{sy1:.1f}) len={length:.1f} raw=({v[0]},{v[1]})->({v[2]},{v[3]})')
                self.renderer.render(vectors, len(vectors))
                self.canvas.update_idletasks()
        else:
            cpu = self.cpu
            via = self.via
            remaining = CYCLES_PER_TICK
            rendered = False

            while remaining > 0:
                irq = 1 if (via.ifr & 0x80) else 0
                cycles = cpu.sstep(irq, 0)
                remaining -= cycles

                for _ in range(cycles):
                    via.sstep0()
                    via.alg_sstep()
                    via.sstep1()

                self.fcycles -= cycles
                if self.fcycles < 0:
                    self.fcycles += FCYCLES_INIT
                    self.renderer.render(via.vectors_draw, via.vector_draw_cnt)
                    via.swap_frame()
                    rendered = True

            if rendered:
                self.canvas.update_idletasks()

        self.root.after(EMU_TIMER, self._tick)

    def run(self):
        self.reset()
        self.root.after(EMU_TIMER, self._tick)
        self.root.mainloop()
