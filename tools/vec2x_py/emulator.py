"""Main emulator loop, Tk window, and keyboard input."""

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
    def __init__(self, rom_path, cart_path=None, parent=None):
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

        self._standalone = (parent is None)
        self._after_id = None

        if self._standalone:
            self.root = tk.Tk()
            self.root.title('Vec2X-Py')
            self.root.resizable(False, False)
            container = self.root
        else:
            self.root = parent.winfo_toplevel()
            container = parent

        self.canvas = tk.Canvas(
            container, width=SCREEN_WIDTH, height=SCREEN_HEIGHT,
            bg='black', highlightthickness=0)
        self.canvas.pack()

        self.renderer = VectrexRenderer(self.canvas)

        self.root.bind('<KeyPress>', self._key_down)
        self.root.bind('<KeyRelease>', self._key_up)

    def _key_down(self, event):
        k = event.keysym.lower()
        if k == 'escape' and self._standalone:
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

        self._after_id = self.root.after(EMU_TIMER, self._tick)

    def get_state(self):
        if _USE_C_EXT:
            return _vec2x.get_state()
        return None

    def start(self):
        self.reset()
        self._after_id = self.root.after(EMU_TIMER, self._tick)

    def stop(self):
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
            self._after_id = None

    def run(self):
        self.start()
        if self._standalone:
            self.root.mainloop()
