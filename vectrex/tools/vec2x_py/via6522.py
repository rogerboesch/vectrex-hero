"""VIA 6522 + analog beam logic + sound register stubs."""

ALG_MAX_X = 33000
ALG_MAX_Y = 41000
VECTREX_COLORS = 128
VECTOR_HASH = 65521


class VIA6522:
    __slots__ = (
        'ora', 'orb', 'ddra', 'ddrb',
        't1on', 't1int', 't1c', 't1ll', 't1lh', 't1pb7',
        't2on', 't2int', 't2c', 't2ll',
        'sr', 'srb', 'src', 'srclk',
        'acr', 'pcr', 'ifr', 'ier',
        'ca2', 'cb2h', 'cb2s',
        'alg_rsh', 'alg_xsh', 'alg_ysh', 'alg_zsh',
        'alg_jch0', 'alg_jch1', 'alg_jch2', 'alg_jch3',
        'alg_jsh', 'alg_compare',
        'alg_dx', 'alg_dy', 'alg_curr_x', 'alg_curr_y',
        'alg_vectoring',
        'alg_vector_x0', 'alg_vector_y0',
        'alg_vector_x1', 'alg_vector_y1',
        'alg_vector_dx', 'alg_vector_dy', 'alg_vector_color',
        'snd_regs', 'snd_select',
        'vectors_draw', 'vectors_erse',
        'vector_draw_cnt', 'vector_erse_cnt',
        'vector_hash',
    )

    def __init__(self):
        self.snd_regs = [0] * 16
        self.vectors_draw = []
        self.vectors_erse = []
        self.vector_hash = {}
        self.reset()

    def reset(self):
        self.ora = 0
        self.orb = 0
        self.ddra = 0
        self.ddrb = 0
        self.t1on = 0
        self.t1int = 0
        self.t1c = 0
        self.t1ll = 0
        self.t1lh = 0
        self.t1pb7 = 0x80
        self.t2on = 0
        self.t2int = 0
        self.t2c = 0
        self.t2ll = 0
        self.sr = 0
        self.srb = 8
        self.src = 0
        self.srclk = 0
        self.acr = 0
        self.pcr = 0
        self.ifr = 0
        self.ier = 0
        self.ca2 = 1
        self.cb2h = 1
        self.cb2s = 0

        self.alg_rsh = 128
        self.alg_xsh = 128
        self.alg_ysh = 128
        self.alg_zsh = 0
        self.alg_jch0 = 128
        self.alg_jch1 = 128
        self.alg_jch2 = 128
        self.alg_jch3 = 128
        self.alg_jsh = 128
        self.alg_compare = 0

        self.alg_dx = 0
        self.alg_dy = 0
        self.alg_curr_x = ALG_MAX_X // 2
        self.alg_curr_y = ALG_MAX_Y // 2

        self.alg_vectoring = 0
        self.alg_vector_x0 = 0
        self.alg_vector_y0 = 0
        self.alg_vector_x1 = 0
        self.alg_vector_y1 = 0
        self.alg_vector_dx = 0
        self.alg_vector_dy = 0
        self.alg_vector_color = 0

        for i in range(16):
            self.snd_regs[i] = 0
        self.snd_regs[14] = 0xFF
        self.snd_select = 0

        self.vectors_draw = []
        self.vectors_erse = []
        self.vector_draw_cnt = 0
        self.vector_erse_cnt = 0
        self.vector_hash = {}

    # --- Sound update (stub - no actual audio) ---

    def _snd_update(self):
        orb = self.orb
        s = orb & 0x18
        if s == 0x00:
            pass  # disabled
        elif s == 0x08:
            pass  # sending data
        elif s == 0x10:
            # receiving data
            if self.snd_select != 14:
                self.snd_regs[self.snd_select] = self.ora
        elif s == 0x18:
            # latching address
            if (self.ora & 0xF0) == 0x00:
                self.snd_select = self.ora & 0x0F

    # --- Analog update ---

    def _alg_update(self):
        orb = self.orb
        s = orb & 0x06

        if s == 0x00:
            self.alg_jsh = self.alg_jch0
            if (orb & 0x01) == 0x00:
                self.alg_ysh = self.alg_xsh
        elif s == 0x02:
            self.alg_jsh = self.alg_jch1
            if (orb & 0x01) == 0x00:
                self.alg_rsh = self.alg_xsh
        elif s == 0x04:
            self.alg_jsh = self.alg_jch2
            if (orb & 0x01) == 0x00:
                if self.alg_xsh > 0x80:
                    self.alg_zsh = self.alg_xsh - 0x80
                else:
                    self.alg_zsh = 0
        else:  # 0x06
            self.alg_jsh = self.alg_jch3

        if self.alg_jsh > self.alg_xsh:
            self.alg_compare = 0x20
        else:
            self.alg_compare = 0

        self.alg_dx = self.alg_xsh - self.alg_rsh
        self.alg_dy = self.alg_rsh - self.alg_ysh

    # --- Interrupt flag update ---

    def _int_update(self):
        if (self.ifr & 0x7F) & (self.ier & 0x7F):
            self.ifr |= 0x80
        else:
            self.ifr &= 0x7F

    # --- Register read ---

    def read(self, reg):
        if reg == 0x0:
            # ORB - compare signal is input
            if self.acr & 0x80:
                return (self.orb & 0x5F) | self.t1pb7 | self.alg_compare
            else:
                return (self.orb & 0xDF) | self.alg_compare

        elif reg == 0x1:
            # ORA with handshake
            if (self.pcr & 0x0E) == 0x08:
                self.ca2 = 0
            # fall through to 0xF
            if (self.orb & 0x18) == 0x08:
                return self.snd_regs[self.snd_select]
            return self.ora

        elif reg == 0x2:
            return self.ddrb
        elif reg == 0x3:
            return self.ddra

        elif reg == 0x4:
            # T1 low counter - clears interrupt
            data = self.t1c & 0xFF
            self.ifr &= 0xBF
            self.t1on = 0
            self.t1int = 0
            self.t1pb7 = 0x80
            self._int_update()
            return data

        elif reg == 0x5:
            return (self.t1c >> 8) & 0xFF

        elif reg == 0x6:
            return self.t1ll & 0xFF
        elif reg == 0x7:
            return self.t1lh & 0xFF

        elif reg == 0x8:
            # T2 low counter - clears interrupt
            data = self.t2c & 0xFF
            self.ifr &= 0xDF
            self.t2on = 0
            self.t2int = 0
            self._int_update()
            return data

        elif reg == 0x9:
            return (self.t2c >> 8) & 0xFF

        elif reg == 0xA:
            # Shift register
            data = self.sr & 0xFF
            self.ifr &= 0xFB
            self.srb = 0
            self.srclk = 1
            self._int_update()
            return data

        elif reg == 0xB:
            return self.acr
        elif reg == 0xC:
            return self.pcr
        elif reg == 0xD:
            return self.ifr
        elif reg == 0xE:
            return self.ier | 0x80

        elif reg == 0xF:
            # ORA without handshake
            if (self.orb & 0x18) == 0x08:
                return self.snd_regs[self.snd_select]
            return self.ora

        return 0

    # --- Register write ---

    def write(self, reg, data):
        data &= 0xFF

        if reg == 0x0:
            self.orb = data
            self._snd_update()
            self._alg_update()
            if (self.pcr & 0xE0) == 0x80:
                self.cb2h = 0

        elif reg == 0x1:
            # ORA with handshake
            if (self.pcr & 0x0E) == 0x08:
                self.ca2 = 0
            # fall through
            self.ora = data
            self._snd_update()
            self.alg_xsh = data ^ 0x80
            self._alg_update()

        elif reg == 0x2:
            self.ddrb = data
        elif reg == 0x3:
            self.ddra = data

        elif reg == 0x4:
            self.t1ll = data
        elif reg == 0x5:
            # T1 high counter - starts timer
            self.t1lh = data
            self.t1c = (self.t1lh << 8) | self.t1ll
            self.ifr &= 0xBF
            self.t1on = 1
            self.t1int = 1
            self.t1pb7 = 0
            self._int_update()

        elif reg == 0x6:
            self.t1ll = data
        elif reg == 0x7:
            self.t1lh = data

        elif reg == 0x8:
            self.t2ll = data
        elif reg == 0x9:
            # T2 high - starts timer
            self.t2c = (data << 8) | self.t2ll
            self.ifr &= 0xDF
            self.t2on = 1
            self.t2int = 1
            self._int_update()

        elif reg == 0xA:
            self.sr = data
            self.ifr &= 0xFB
            self.srb = 0
            self.srclk = 1
            self._int_update()

        elif reg == 0xB:
            self.acr = data

        elif reg == 0xC:
            self.pcr = data
            if (self.pcr & 0x0E) == 0x0C:
                self.ca2 = 0
            else:
                self.ca2 = 1
            if (self.pcr & 0xE0) == 0xC0:
                self.cb2h = 0
            else:
                self.cb2h = 1

        elif reg == 0xD:
            self.ifr &= ~(data & 0x7F)
            self._int_update()

        elif reg == 0xE:
            if data & 0x80:
                self.ier |= data & 0x7F
            else:
                self.ier &= ~(data & 0x7F)
            self._int_update()

        elif reg == 0xF:
            # ORA without handshake
            self.ora = data
            self._snd_update()
            self.alg_xsh = data ^ 0x80
            self._alg_update()

    # --- Per-cycle VIA step 0 (timers + shift register) ---

    def sstep0(self):
        # Timer 1
        if self.t1on:
            self.t1c = (self.t1c - 1) & 0xFFFF
            if self.t1c == 0xFFFF:
                if self.acr & 0x40:
                    # continuous mode
                    self.ifr |= 0x40
                    self._int_update()
                    self.t1pb7 = 0x80 - self.t1pb7
                    self.t1c = (self.t1lh << 8) | self.t1ll
                else:
                    # one-shot
                    if self.t1int:
                        self.ifr |= 0x40
                        self._int_update()
                        self.t1pb7 = 0x80
                        self.t1int = 0

        # Timer 2
        if self.t2on and (self.acr & 0x20) == 0x00:
            self.t2c = (self.t2c - 1) & 0xFFFF
            if self.t2c == 0xFFFF:
                if self.t2int:
                    self.ifr |= 0x20
                    self._int_update()
                    self.t2int = 0

        # Shift counter
        self.src = (self.src - 1) & 0xFF
        if self.src == 0xFF:
            self.src = self.t2ll
            if self.srclk:
                t2shift = 1
                self.srclk = 0
            else:
                t2shift = 0
                self.srclk = 1
        else:
            t2shift = 0

        # Shift register
        if self.srb < 8:
            mode = self.acr & 0x1C
            if mode == 0x00:
                pass  # disabled
            elif mode == 0x04:
                # shift in under T2
                if t2shift:
                    self.sr = (self.sr << 1) & 0xFF
                    self.srb += 1
            elif mode == 0x08:
                # shift in under system clock
                self.sr = (self.sr << 1) & 0xFF
                self.srb += 1
            elif mode == 0x0C:
                pass  # shift in under CB1
            elif mode == 0x10:
                # shift out T2 free-run
                if t2shift:
                    self.cb2s = (self.sr >> 7) & 1
                    self.sr = ((self.sr << 1) | self.cb2s) & 0xFF
            elif mode == 0x14:
                # shift out T2
                if t2shift:
                    self.cb2s = (self.sr >> 7) & 1
                    self.sr = ((self.sr << 1) | self.cb2s) & 0xFF
                    self.srb += 1
            elif mode == 0x18:
                # shift out system clock
                self.cb2s = (self.sr >> 7) & 1
                self.sr = ((self.sr << 1) | self.cb2s) & 0xFF
                self.srb += 1
            elif mode == 0x1C:
                pass  # shift out CB1

            if self.srb == 8:
                self.ifr |= 0x04
                self._int_update()

    # --- Per-cycle analog step ---

    def alg_sstep(self):
        if self.acr & 0x10:
            sig_blank = self.cb2s
        else:
            sig_blank = self.cb2h

        if self.ca2 == 0:
            sig_dx = ALG_MAX_X // 2 - self.alg_curr_x
            sig_dy = ALG_MAX_Y // 2 - self.alg_curr_y
        else:
            if self.acr & 0x80:
                sig_ramp = self.t1pb7
            else:
                sig_ramp = self.orb & 0x80

            if sig_ramp == 0:
                sig_dx = self.alg_dx
                sig_dy = self.alg_dy
            else:
                sig_dx = 0
                sig_dy = 0

        if self.alg_vectoring == 0:
            if (sig_blank == 1 and
                    0 <= self.alg_curr_x < ALG_MAX_X and
                    0 <= self.alg_curr_y < ALG_MAX_Y):
                self.alg_vectoring = 1
                self.alg_vector_x0 = self.alg_curr_x
                self.alg_vector_y0 = self.alg_curr_y
                self.alg_vector_x1 = self.alg_curr_x
                self.alg_vector_y1 = self.alg_curr_y
                self.alg_vector_dx = sig_dx
                self.alg_vector_dy = sig_dy
                self.alg_vector_color = self.alg_zsh & 0xFF
        else:
            if sig_blank == 0:
                self.alg_vectoring = 0
                self._alg_addline(
                    self.alg_vector_x0, self.alg_vector_y0,
                    self.alg_vector_x1, self.alg_vector_y1,
                    self.alg_vector_color)
            elif (sig_dx != self.alg_vector_dx or
                  sig_dy != self.alg_vector_dy or
                  (self.alg_zsh & 0xFF) != self.alg_vector_color):
                self._alg_addline(
                    self.alg_vector_x0, self.alg_vector_y0,
                    self.alg_vector_x1, self.alg_vector_y1,
                    self.alg_vector_color)
                if (0 <= self.alg_curr_x < ALG_MAX_X and
                        0 <= self.alg_curr_y < ALG_MAX_Y):
                    self.alg_vector_x0 = self.alg_curr_x
                    self.alg_vector_y0 = self.alg_curr_y
                    self.alg_vector_x1 = self.alg_curr_x
                    self.alg_vector_y1 = self.alg_curr_y
                    self.alg_vector_dx = sig_dx
                    self.alg_vector_dy = sig_dy
                    self.alg_vector_color = self.alg_zsh & 0xFF
                else:
                    self.alg_vectoring = 0

        self.alg_curr_x += sig_dx
        self.alg_curr_y += sig_dy

        if (self.alg_vectoring == 1 and
                0 <= self.alg_curr_x < ALG_MAX_X and
                0 <= self.alg_curr_y < ALG_MAX_Y):
            self.alg_vector_x1 = self.alg_curr_x
            self.alg_vector_y1 = self.alg_curr_y

    # --- Per-cycle VIA step 1 (pulse restoration) ---

    def sstep1(self):
        if (self.pcr & 0x0E) == 0x0A:
            self.ca2 = 1
        if (self.pcr & 0xE0) == 0xA0:
            self.cb2h = 1

    # --- Add line with deduplication ---

    def _alg_addline(self, x0, y0, x1, y1, color):
        key = (x0, y0, x1, y1)

        if key in self.vector_hash:
            idx = self.vector_hash[key]
            if idx < self.vector_draw_cnt:
                v = self.vectors_draw[idx]
                if v[0] == x0 and v[1] == y0 and v[2] == x1 and v[3] == y1:
                    self.vectors_draw[idx] = (x0, y0, x1, y1, color)
                    return

            if idx < self.vector_erse_cnt:
                v = self.vectors_erse[idx]
                if v[0] == x0 and v[1] == y0 and v[2] == x1 and v[3] == y1:
                    self.vectors_erse[idx] = (x0, y0, x1, y1, VECTREX_COLORS)

        self.vectors_draw.append((x0, y0, x1, y1, color))
        self.vector_hash[key] = self.vector_draw_cnt
        self.vector_draw_cnt += 1

    # --- Frame swap ---

    def swap_frame(self):
        self.vectors_erse = self.vectors_draw
        self.vector_erse_cnt = self.vector_draw_cnt
        self.vectors_draw = []
        self.vector_draw_cnt = 0
        self.vector_hash = {}
