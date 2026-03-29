"""Motorola 6809 CPU emulator -- faithful port of e6809.c

The memory bus object is assigned externally via ``self.mem`` and must
provide ``read8(address) -> int`` and ``write8(address, data)`` methods.
"""


class CPU6809:
    """Motorola 6809 CPU emulator.

    After construction, assign a memory-bus object to ``self.mem`` before
    calling :meth:`reset` or :meth:`sstep`.  The bus must expose
    ``read8(addr)`` and ``write8(addr, data)``.
    """

    __slots__ = (
        'reg_x', 'reg_y', 'reg_u', 'reg_s', 'reg_pc',
        'reg_a', 'reg_b', 'reg_dp', 'reg_cc',
        'irq_status',
        'mem',
    )

    # Condition code flag masks
    FLAG_E = 0x80
    FLAG_F = 0x40
    FLAG_H = 0x20
    FLAG_I = 0x10
    FLAG_N = 0x08
    FLAG_Z = 0x04
    FLAG_V = 0x02
    FLAG_C = 0x01

    # IRQ status values
    IRQ_NORMAL = 0
    IRQ_SYNC   = 1
    IRQ_CWAI   = 2

    def __init__(self):
        self.reg_x  = 0
        self.reg_y  = 0
        self.reg_u  = 0
        self.reg_s  = 0
        self.reg_pc = 0
        self.reg_a  = 0
        self.reg_b  = 0
        self.reg_dp = 0
        self.reg_cc = 0
        self.irq_status = self.IRQ_NORMAL
        self.mem = None  # assigned externally

    # ------------------------------------------------------------------
    # Condition code helpers
    # ------------------------------------------------------------------

    def _get_cc(self, flag):
        """Return 0 or 1 for the given CC flag bit."""
        return (self.reg_cc // flag) & 1

    def _set_cc(self, flag, value):
        """Set a CC flag bit.  *value* must be 0 or 1."""
        self.reg_cc = (self.reg_cc & ~flag) | (value * flag)
        self.reg_cc &= 0xFF

    # ------------------------------------------------------------------
    # Arithmetic test helpers  (match C implementation exactly)
    # ------------------------------------------------------------------

    @staticmethod
    def _test_c(i0, i1, r, sub):
        """Test carry for 8-bit operands (bits examined at bit 7)."""
        flag  = (i0 | i1) & ~r
        flag |= (i0 & i1)
        flag  = (flag >> 7) & 1
        flag ^= sub
        return flag

    @staticmethod
    def _test_n(r):
        return (r >> 7) & 1

    @staticmethod
    def _test_z8(r):
        flag = ~r & 0xFF
        flag = (flag >> 4) & (flag & 0xF)
        flag = (flag >> 2) & (flag & 0x3)
        flag = (flag >> 1) & (flag & 0x1)
        return flag

    @staticmethod
    def _test_z16(r):
        flag = ~r & 0xFFFF
        flag = (flag >> 8) & (flag & 0xFF)
        flag = (flag >> 4) & (flag & 0xF)
        flag = (flag >> 2) & (flag & 0x3)
        flag = (flag >> 1) & (flag & 0x1)
        return flag

    @staticmethod
    def _test_v(i0, i1, r):
        flag  = ~(i0 ^ i1)
        flag &=  (i0 ^ r)
        flag  = (flag >> 7) & 1
        return flag

    # ------------------------------------------------------------------
    # D register (A:B pair)
    # ------------------------------------------------------------------

    def _get_reg_d(self):
        return ((self.reg_a & 0xFF) << 8) | (self.reg_b & 0xFF)

    def _set_reg_d(self, value):
        self.reg_a = (value >> 8) & 0xFF
        self.reg_b = value & 0xFF

    # ------------------------------------------------------------------
    # Memory access wrappers
    # ------------------------------------------------------------------

    def _read8(self, address):
        return self.mem.read8(address & 0xFFFF)

    def _write8(self, address, data):
        self.mem.write8(address & 0xFFFF, data & 0xFF)

    def _read16(self, address):
        hi = self._read8(address)
        lo = self._read8(address + 1)
        return (hi << 8) | lo

    def _write16(self, address, data):
        self._write8(address, (data >> 8) & 0xFF)
        self._write8(address + 1, data & 0xFF)

    # ------------------------------------------------------------------
    # Stack helpers
    # ------------------------------------------------------------------

    def _push8_s(self, data):
        self.reg_s = (self.reg_s - 1) & 0xFFFF
        self._write8(self.reg_s, data)

    def _pull8_s(self):
        data = self._read8(self.reg_s)
        self.reg_s = (self.reg_s + 1) & 0xFFFF
        return data

    def _push16_s(self, data):
        self._push8_s(data & 0xFF)
        self._push8_s((data >> 8) & 0xFF)

    def _pull16_s(self):
        hi = self._pull8_s()
        lo = self._pull8_s()
        return (hi << 8) | lo

    def _push8_u(self, data):
        self.reg_u = (self.reg_u - 1) & 0xFFFF
        self._write8(self.reg_u, data)

    def _pull8_u(self):
        data = self._read8(self.reg_u)
        self.reg_u = (self.reg_u + 1) & 0xFFFF
        return data

    def _push16_u(self, data):
        self._push8_u(data & 0xFF)
        self._push8_u((data >> 8) & 0xFF)

    def _pull16_u(self):
        hi = self._pull8_u()
        lo = self._pull8_u()
        return (hi << 8) | lo

    # ------------------------------------------------------------------
    # PC-relative reads
    # ------------------------------------------------------------------

    def _pc_read8(self):
        data = self._read8(self.reg_pc)
        self.reg_pc = (self.reg_pc + 1) & 0xFFFF
        return data

    def _pc_read16(self):
        data = self._read16(self.reg_pc)
        self.reg_pc = (self.reg_pc + 2) & 0xFFFF
        return data

    # ------------------------------------------------------------------
    # Sign extension
    # ------------------------------------------------------------------

    @staticmethod
    def _sign_extend(data):
        """Sign-extend an 8-bit value to 16 bits (matching C implementation)."""
        return (~(data & 0x80) + 1) | (data & 0xFF)

    # ------------------------------------------------------------------
    # Addressing modes
    # ------------------------------------------------------------------

    def _ea_direct(self):
        return ((self.reg_dp << 8) | self._pc_read8()) & 0xFFFF

    def _ea_extended(self):
        return self._pc_read16()

    def _get_xyus(self, r):
        """Read indexed register by number 0-3 -> X,Y,U,S."""
        if r == 0:
            return self.reg_x
        elif r == 1:
            return self.reg_y
        elif r == 2:
            return self.reg_u
        else:
            return self.reg_s

    def _set_xyus(self, r, val):
        """Write indexed register by number 0-3 -> X,Y,U,S."""
        val &= 0xFFFF
        if r == 0:
            self.reg_x = val
        elif r == 1:
            self.reg_y = val
        elif r == 2:
            self.reg_u = val
        else:
            self.reg_s = val

    def _ea_indexed(self):
        """Indexed addressing.  Returns (ea, extra_cycles)."""
        op = self._pc_read8()
        r = (op >> 5) & 3
        ecyc = 0

        # 5-bit offset: positive range 0x00..0x0F, 0x20..0x2F, 0x40..0x4F, 0x60..0x6F
        if op < 0x80:
            if op & 0x10:
                # negative offset: R + (op & 0xf) - 0x10  i.e. -16..-1
                ea = (self._get_xyus(r) + (op & 0xF) - 0x10) & 0xFFFF
            else:
                # positive offset: R + (op & 0xf)  i.e. 0..15
                ea = (self._get_xyus(r) + (op & 0xF)) & 0xFFFF
            ecyc = 1
            return ea, ecyc

        # Determine sub-mode from lower 4 bits (with bit 4 = indirect flag)
        idx = op & 0x1F  # 5 low bits determine the mode

        if idx == 0x00 or idx == 0x01:
            # ,R+  /  ,R++
            ea = self._get_xyus(r)
            self._set_xyus(r, ea + 1 + (op & 1))
            ecyc = 2 + (op & 1)
            return ea & 0xFFFF, ecyc

        if idx == 0x10 or idx == 0x11:
            # [,R+] ??? / [,R++]
            rval = self._get_xyus(r)
            ea = self._read16(rval)
            self._set_xyus(r, rval + 1 + (op & 1))
            ecyc = 5 + (op & 1)
            return ea & 0xFFFF, ecyc

        if idx == 0x02 or idx == 0x03:
            # ,-R  /  ,--R
            rval = self._get_xyus(r) - (1 + (op & 1))
            self._set_xyus(r, rval)
            ea = rval
            ecyc = 2 + (op & 1)
            return ea & 0xFFFF, ecyc

        if idx == 0x12 or idx == 0x13:
            # [,-R] ??? / [,--R]
            rval = self._get_xyus(r) - (1 + (op & 1))
            self._set_xyus(r, rval)
            ea = self._read16(rval & 0xFFFF)
            ecyc = 5 + (op & 1)
            return ea & 0xFFFF, ecyc

        if idx == 0x04:
            # ,R  (no offset)
            ea = self._get_xyus(r)
            return ea & 0xFFFF, 0

        if idx == 0x14:
            # [,R]
            ea = self._read16(self._get_xyus(r))
            return ea & 0xFFFF, 3

        if idx == 0x05:
            # B,R
            ea = (self._get_xyus(r) + self._sign_extend(self.reg_b)) & 0xFFFF
            return ea, 1

        if idx == 0x15:
            # [B,R]
            ea = self._read16((self._get_xyus(r) + self._sign_extend(self.reg_b)) & 0xFFFF)
            return ea & 0xFFFF, 4

        if idx == 0x06:
            # A,R
            ea = (self._get_xyus(r) + self._sign_extend(self.reg_a)) & 0xFFFF
            return ea, 1

        if idx == 0x16:
            # [A,R]
            ea = self._read16((self._get_xyus(r) + self._sign_extend(self.reg_a)) & 0xFFFF)
            return ea & 0xFFFF, 4

        if idx == 0x08:
            # byte,R
            ea = (self._get_xyus(r) + self._sign_extend(self._pc_read8())) & 0xFFFF
            return ea, 1

        if idx == 0x18:
            # [byte,R]
            ea = self._read16((self._get_xyus(r) + self._sign_extend(self._pc_read8())) & 0xFFFF)
            return ea & 0xFFFF, 4

        if idx == 0x09:
            # word,R
            ea = (self._get_xyus(r) + self._pc_read16()) & 0xFFFF
            return ea, 4

        if idx == 0x19:
            # [word,R]
            ea = self._read16((self._get_xyus(r) + self._pc_read16()) & 0xFFFF)
            return ea & 0xFFFF, 7

        if idx == 0x0B:
            # D,R
            ea = (self._get_xyus(r) + self._get_reg_d()) & 0xFFFF
            return ea, 4

        if idx == 0x1B:
            # [D,R]
            ea = self._read16((self._get_xyus(r) + self._get_reg_d()) & 0xFFFF)
            return ea & 0xFFFF, 7

        if idx == 0x0C:
            # byte,PC
            off = self._sign_extend(self._pc_read8())
            ea = (self.reg_pc + off) & 0xFFFF
            return ea, 1

        if idx == 0x1C:
            # [byte,PC]
            off = self._sign_extend(self._pc_read8())
            ea = self._read16((self.reg_pc + off) & 0xFFFF)
            return ea & 0xFFFF, 4

        if idx == 0x0D:
            # word,PC
            off = self._pc_read16()
            ea = (self.reg_pc + off) & 0xFFFF
            return ea, 5

        if idx == 0x1D:
            # [word,PC]
            off = self._pc_read16()
            ea = self._read16((self.reg_pc + off) & 0xFFFF)
            return ea & 0xFFFF, 8

        if idx == 0x1F:
            # [address]  (only indirect form exists: 0x9F)
            ea = self._read16(self._pc_read16())
            return ea & 0xFFFF, 5

        # undefined post-byte -- return 0
        return 0, 0

    # ------------------------------------------------------------------
    # ALU instructions
    # ------------------------------------------------------------------

    def _inst_neg(self, data):
        i0 = 0
        i1 = (~data) & 0xFF
        r = (i0 + i1 + 1) & 0xFF

        self._set_cc(self.FLAG_H, self._test_c((i0 << 4) & 0xFF, (i1 << 4) & 0xFF, (r << 4) & 0xFF, 0))
        self._set_cc(self.FLAG_N, self._test_n(r))
        self._set_cc(self.FLAG_Z, self._test_z8(r))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 1))
        return r

    def _inst_com(self, data):
        r = (~data) & 0xFF
        self._set_cc(self.FLAG_N, self._test_n(r))
        self._set_cc(self.FLAG_Z, self._test_z8(r))
        self._set_cc(self.FLAG_V, 0)
        self._set_cc(self.FLAG_C, 1)
        return r

    def _inst_lsr(self, data):
        r = (data >> 1) & 0x7F
        self._set_cc(self.FLAG_N, 0)
        self._set_cc(self.FLAG_Z, self._test_z8(r))
        self._set_cc(self.FLAG_C, data & 1)
        return r

    def _inst_ror(self, data):
        c = self._get_cc(self.FLAG_C)
        r = ((data >> 1) & 0x7F) | (c << 7)
        self._set_cc(self.FLAG_N, self._test_n(r))
        self._set_cc(self.FLAG_Z, self._test_z8(r))
        self._set_cc(self.FLAG_C, data & 1)
        return r

    def _inst_asr(self, data):
        r = ((data >> 1) & 0x7F) | (data & 0x80)
        self._set_cc(self.FLAG_N, self._test_n(r))
        self._set_cc(self.FLAG_Z, self._test_z8(r))
        self._set_cc(self.FLAG_C, data & 1)
        return r

    def _inst_asl(self, data):
        i0 = data & 0xFF
        i1 = data & 0xFF
        r = (i0 + i1) & 0x1FF  # keep 9 bits for carry detection

        self._set_cc(self.FLAG_H, self._test_c((i0 << 4) & 0xFF, (i1 << 4) & 0xFF, (r << 4) & 0xFF, 0))
        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 0))
        return r & 0xFF

    def _inst_rol(self, data):
        i0 = data & 0xFF
        i1 = data & 0xFF
        c = self._get_cc(self.FLAG_C)
        r = i0 + i1 + c

        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 0))
        return r & 0xFF

    def _inst_dec(self, data):
        i0 = data & 0xFF
        i1 = 0xFF
        r = (i0 + i1) & 0x1FF

        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        return r & 0xFF

    def _inst_inc(self, data):
        i0 = data & 0xFF
        i1 = 1
        r = (i0 + i1) & 0x1FF

        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        return r & 0xFF

    def _inst_tst8(self, data):
        self._set_cc(self.FLAG_N, self._test_n(data))
        self._set_cc(self.FLAG_Z, self._test_z8(data))
        self._set_cc(self.FLAG_V, 0)

    def _inst_tst16(self, data):
        self._set_cc(self.FLAG_N, self._test_n((data >> 8) & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z16(data))
        self._set_cc(self.FLAG_V, 0)

    def _inst_clr(self):
        self._set_cc(self.FLAG_N, 0)
        self._set_cc(self.FLAG_Z, 1)
        self._set_cc(self.FLAG_V, 0)
        self._set_cc(self.FLAG_C, 0)

    def _inst_sub8(self, data0, data1):
        i0 = data0 & 0xFF
        i1 = (~data1) & 0xFF
        r = i0 + i1 + 1

        self._set_cc(self.FLAG_H, self._test_c((i0 << 4) & 0xFF, (i1 << 4) & 0xFF, (r << 4) & 0xFF, 0))
        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 1))
        return r & 0xFF

    def _inst_sbc(self, data0, data1):
        i0 = data0 & 0xFF
        i1 = (~data1) & 0xFF
        c = 1 - self._get_cc(self.FLAG_C)
        r = i0 + i1 + c

        self._set_cc(self.FLAG_H, self._test_c((i0 << 4) & 0xFF, (i1 << 4) & 0xFF, (r << 4) & 0xFF, 0))
        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 1))
        return r & 0xFF

    def _inst_and(self, data0, data1):
        r = (data0 & data1) & 0xFF
        self._inst_tst8(r)
        return r

    def _inst_eor(self, data0, data1):
        r = (data0 ^ data1) & 0xFF
        self._inst_tst8(r)
        return r

    def _inst_adc(self, data0, data1):
        i0 = data0 & 0xFF
        i1 = data1 & 0xFF
        c = self._get_cc(self.FLAG_C)
        r = i0 + i1 + c

        self._set_cc(self.FLAG_H, self._test_c((i0 << 4) & 0xFF, (i1 << 4) & 0xFF, (r << 4) & 0xFF, 0))
        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 0))
        return r & 0xFF

    def _inst_or(self, data0, data1):
        r = (data0 | data1) & 0xFF
        self._inst_tst8(r)
        return r

    def _inst_add8(self, data0, data1):
        i0 = data0 & 0xFF
        i1 = data1 & 0xFF
        r = i0 + i1

        self._set_cc(self.FLAG_H, self._test_c((i0 << 4) & 0xFF, (i1 << 4) & 0xFF, (r << 4) & 0xFF, 0))
        self._set_cc(self.FLAG_N, self._test_n(r & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z8(r & 0xFF))
        self._set_cc(self.FLAG_V, self._test_v(i0, i1, r))
        self._set_cc(self.FLAG_C, self._test_c(i0, i1, r, 0))
        return r & 0xFF

    def _inst_add16(self, data0, data1):
        i0 = data0 & 0xFFFF
        i1 = data1 & 0xFFFF
        r = i0 + i1

        self._set_cc(self.FLAG_N, self._test_n((r >> 8) & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z16(r & 0xFFFF))
        self._set_cc(self.FLAG_V, self._test_v((i0 >> 8) & 0xFF, (i1 >> 8) & 0xFF, (r >> 8) & 0xFF))
        self._set_cc(self.FLAG_C, self._test_c((i0 >> 8) & 0xFF, (i1 >> 8) & 0xFF, (r >> 8) & 0xFF, 0))
        return r & 0xFFFF

    def _inst_sub16(self, data0, data1):
        i0 = data0 & 0xFFFF
        i1 = (~data1) & 0xFFFF
        r = i0 + i1 + 1

        self._set_cc(self.FLAG_N, self._test_n((r >> 8) & 0xFF))
        self._set_cc(self.FLAG_Z, self._test_z16(r & 0xFFFF))
        self._set_cc(self.FLAG_V, self._test_v((i0 >> 8) & 0xFF, (i1 >> 8) & 0xFF, (r >> 8) & 0xFF))
        self._set_cc(self.FLAG_C, self._test_c((i0 >> 8) & 0xFF, (i1 >> 8) & 0xFF, (r >> 8) & 0xFF, 1))
        return r & 0xFFFF

    # ------------------------------------------------------------------
    # Branch instructions
    # ------------------------------------------------------------------

    def _inst_bra8(self, test, op):
        offset = self._pc_read8()
        # mask is 0xFFFF when taken, 0 when not taken
        mask = ((test ^ (op & 1)) - 1) & 0xFFFF
        self.reg_pc = (self.reg_pc + (self._sign_extend(offset) & mask)) & 0xFFFF
        return 3

    def _inst_bra16(self, test, op):
        offset = self._pc_read16()
        mask = ((test ^ (op & 1)) - 1) & 0xFFFF
        self.reg_pc = (self.reg_pc + (offset & mask)) & 0xFFFF
        # cycles: 5 when not taken (mask=0), 6 when taken (mask=0xffff)
        # C code: *cycles += 5 - mask;  but mask is 0xffff => 5 - (-1) = 6
        # In Python mask is unsigned so: if taken (mask=0xFFFF) => +6, else +5
        if mask:
            return 6
        return 5

    # ------------------------------------------------------------------
    # Push / Pull
    # ------------------------------------------------------------------

    def _inst_psh_s(self, op, other_sp_val):
        """PSHS: push registers onto S stack. Returns extra cycles."""
        ecyc = 0
        if op & 0x80:
            self._push16_s(self.reg_pc)
            ecyc += 2
        if op & 0x40:
            self._push16_s(other_sp_val)  # U for PSHS
            ecyc += 2
        if op & 0x20:
            self._push16_s(self.reg_y)
            ecyc += 2
        if op & 0x10:
            self._push16_s(self.reg_x)
            ecyc += 2
        if op & 0x08:
            self._push8_s(self.reg_dp)
            ecyc += 1
        if op & 0x04:
            self._push8_s(self.reg_b)
            ecyc += 1
        if op & 0x02:
            self._push8_s(self.reg_a)
            ecyc += 1
        if op & 0x01:
            self._push8_s(self.reg_cc)
            ecyc += 1
        return ecyc

    def _inst_pul_s(self, op):
        """PULS: pull registers from S stack. Returns extra cycles."""
        ecyc = 0
        if op & 0x01:
            self.reg_cc = self._pull8_s()
            ecyc += 1
        if op & 0x02:
            self.reg_a = self._pull8_s()
            ecyc += 1
        if op & 0x04:
            self.reg_b = self._pull8_s()
            ecyc += 1
        if op & 0x08:
            self.reg_dp = self._pull8_s()
            ecyc += 1
        if op & 0x10:
            self.reg_x = self._pull16_s()
            ecyc += 2
        if op & 0x20:
            self.reg_y = self._pull16_s()
            ecyc += 2
        if op & 0x40:
            self.reg_u = self._pull16_s()  # other stack pointer
            ecyc += 2
        if op & 0x80:
            self.reg_pc = self._pull16_s()
            ecyc += 2
        return ecyc

    def _inst_psh_u(self, op, other_sp_val):
        """PSHU: push registers onto U stack. Returns extra cycles."""
        ecyc = 0
        if op & 0x80:
            self._push16_u(self.reg_pc)
            ecyc += 2
        if op & 0x40:
            self._push16_u(other_sp_val)  # S for PSHU
            ecyc += 2
        if op & 0x20:
            self._push16_u(self.reg_y)
            ecyc += 2
        if op & 0x10:
            self._push16_u(self.reg_x)
            ecyc += 2
        if op & 0x08:
            self._push8_u(self.reg_dp)
            ecyc += 1
        if op & 0x04:
            self._push8_u(self.reg_b)
            ecyc += 1
        if op & 0x02:
            self._push8_u(self.reg_a)
            ecyc += 1
        if op & 0x01:
            self._push8_u(self.reg_cc)
            ecyc += 1
        return ecyc

    def _inst_pul_u(self, op):
        """PULU: pull registers from U stack. Returns extra cycles."""
        ecyc = 0
        if op & 0x01:
            self.reg_cc = self._pull8_u()
            ecyc += 1
        if op & 0x02:
            self.reg_a = self._pull8_u()
            ecyc += 1
        if op & 0x04:
            self.reg_b = self._pull8_u()
            ecyc += 1
        if op & 0x08:
            self.reg_dp = self._pull8_u()
            ecyc += 1
        if op & 0x10:
            self.reg_x = self._pull16_u()
            ecyc += 2
        if op & 0x20:
            self.reg_y = self._pull16_u()
            ecyc += 2
        if op & 0x40:
            self.reg_s = self._pull16_u()  # other stack pointer
            ecyc += 2
        if op & 0x80:
            self.reg_pc = self._pull16_u()
            ecyc += 2
        return ecyc

    # ------------------------------------------------------------------
    # EXG / TFR helpers
    # ------------------------------------------------------------------

    def _exgtfr_read(self, reg):
        if reg == 0x0:
            return self._get_reg_d()
        elif reg == 0x1:
            return self.reg_x
        elif reg == 0x2:
            return self.reg_y
        elif reg == 0x3:
            return self.reg_u
        elif reg == 0x4:
            return self.reg_s
        elif reg == 0x5:
            return self.reg_pc
        elif reg == 0x8:
            return 0xFF00 | (self.reg_a & 0xFF)
        elif reg == 0x9:
            return 0xFF00 | (self.reg_b & 0xFF)
        elif reg == 0xA:
            return 0xFF00 | (self.reg_cc & 0xFF)
        elif reg == 0xB:
            return 0xFF00 | (self.reg_dp & 0xFF)
        else:
            return 0xFFFF

    def _exgtfr_write(self, reg, data):
        if reg == 0x0:
            self._set_reg_d(data)
        elif reg == 0x1:
            self.reg_x = data & 0xFFFF
        elif reg == 0x2:
            self.reg_y = data & 0xFFFF
        elif reg == 0x3:
            self.reg_u = data & 0xFFFF
        elif reg == 0x4:
            self.reg_s = data & 0xFFFF
        elif reg == 0x5:
            self.reg_pc = data & 0xFFFF
        elif reg == 0x8:
            self.reg_a = data & 0xFF
        elif reg == 0x9:
            self.reg_b = data & 0xFF
        elif reg == 0xA:
            self.reg_cc = data & 0xFF
        elif reg == 0xB:
            self.reg_dp = data & 0xFF

    def _inst_exg(self):
        op = self._pc_read8()
        tmp = self._exgtfr_read(op & 0xF)
        self._exgtfr_write(op & 0xF, self._exgtfr_read(op >> 4))
        self._exgtfr_write(op >> 4, tmp)

    def _inst_tfr(self):
        op = self._pc_read8()
        self._exgtfr_write(op & 0xF, self._exgtfr_read(op >> 4))

    # ------------------------------------------------------------------
    # Reset
    # ------------------------------------------------------------------

    def reset(self):
        self.reg_x = 0
        self.reg_y = 0
        self.reg_u = 0
        self.reg_s = 0
        self.reg_a = 0
        self.reg_b = 0
        self.reg_dp = 0
        self.reg_cc = self.FLAG_I | self.FLAG_F
        self.irq_status = self.IRQ_NORMAL
        self.reg_pc = self._read16(0xFFFE)

    # ------------------------------------------------------------------
    # Single-step: execute one instruction, return cycle count
    # ------------------------------------------------------------------

    def sstep(self, irq_i, irq_f):
        cycles = 0

        # ---- handle FIRQ ----
        if irq_f:
            if self._get_cc(self.FLAG_F) == 0:
                if self.irq_status != self.IRQ_CWAI:
                    self._set_cc(self.FLAG_E, 0)
                    cycles += self._inst_psh_s(0x81, self.reg_u)
                self._set_cc(self.FLAG_I, 1)
                self._set_cc(self.FLAG_F, 1)
                self.reg_pc = self._read16(0xFFF6)
                self.irq_status = self.IRQ_NORMAL
                cycles += 7
            else:
                if self.irq_status == self.IRQ_SYNC:
                    self.irq_status = self.IRQ_NORMAL

        # ---- handle IRQ ----
        if irq_i:
            if self._get_cc(self.FLAG_I) == 0:
                if self.irq_status != self.IRQ_CWAI:
                    self._set_cc(self.FLAG_E, 1)
                    cycles += self._inst_psh_s(0xFF, self.reg_u)
                self._set_cc(self.FLAG_I, 1)
                self.reg_pc = self._read16(0xFFF8)
                self.irq_status = self.IRQ_NORMAL
                cycles += 7
            else:
                if self.irq_status == self.IRQ_SYNC:
                    self.irq_status = self.IRQ_NORMAL

        if self.irq_status != self.IRQ_NORMAL:
            return cycles + 1

        op = self._pc_read8()

        # ==============================================================
        # Page 0 instructions
        # ==============================================================

        # --- NEG  (direct / inherent A / inherent B / indexed / extended) ---
        if op == 0x00:
            ea = self._ea_direct()
            r = self._inst_neg(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x40:
            self.reg_a = self._inst_neg(self.reg_a)
            cycles += 2
        elif op == 0x50:
            self.reg_b = self._inst_neg(self.reg_b)
            cycles += 2
        elif op == 0x60:
            ea, ec = self._ea_indexed()
            r = self._inst_neg(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x70:
            ea = self._ea_extended()
            r = self._inst_neg(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- COM ---
        elif op == 0x03:
            ea = self._ea_direct()
            r = self._inst_com(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x43:
            self.reg_a = self._inst_com(self.reg_a)
            cycles += 2
        elif op == 0x53:
            self.reg_b = self._inst_com(self.reg_b)
            cycles += 2
        elif op == 0x63:
            ea, ec = self._ea_indexed()
            r = self._inst_com(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x73:
            ea = self._ea_extended()
            r = self._inst_com(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- LSR ---
        elif op == 0x04:
            ea = self._ea_direct()
            r = self._inst_lsr(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x44:
            self.reg_a = self._inst_lsr(self.reg_a)
            cycles += 2
        elif op == 0x54:
            self.reg_b = self._inst_lsr(self.reg_b)
            cycles += 2
        elif op == 0x64:
            ea, ec = self._ea_indexed()
            r = self._inst_lsr(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x74:
            ea = self._ea_extended()
            r = self._inst_lsr(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- ROR ---
        elif op == 0x06:
            ea = self._ea_direct()
            r = self._inst_ror(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x46:
            self.reg_a = self._inst_ror(self.reg_a)
            cycles += 2
        elif op == 0x56:
            self.reg_b = self._inst_ror(self.reg_b)
            cycles += 2
        elif op == 0x66:
            ea, ec = self._ea_indexed()
            r = self._inst_ror(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x76:
            ea = self._ea_extended()
            r = self._inst_ror(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- ASR ---
        elif op == 0x07:
            ea = self._ea_direct()
            r = self._inst_asr(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x47:
            self.reg_a = self._inst_asr(self.reg_a)
            cycles += 2
        elif op == 0x57:
            self.reg_b = self._inst_asr(self.reg_b)
            cycles += 2
        elif op == 0x67:
            ea, ec = self._ea_indexed()
            r = self._inst_asr(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x77:
            ea = self._ea_extended()
            r = self._inst_asr(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- ASL ---
        elif op == 0x08:
            ea = self._ea_direct()
            r = self._inst_asl(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x48:
            self.reg_a = self._inst_asl(self.reg_a)
            cycles += 2
        elif op == 0x58:
            self.reg_b = self._inst_asl(self.reg_b)
            cycles += 2
        elif op == 0x68:
            ea, ec = self._ea_indexed()
            r = self._inst_asl(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x78:
            ea = self._ea_extended()
            r = self._inst_asl(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- ROL ---
        elif op == 0x09:
            ea = self._ea_direct()
            r = self._inst_rol(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x49:
            self.reg_a = self._inst_rol(self.reg_a)
            cycles += 2
        elif op == 0x59:
            self.reg_b = self._inst_rol(self.reg_b)
            cycles += 2
        elif op == 0x69:
            ea, ec = self._ea_indexed()
            r = self._inst_rol(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x79:
            ea = self._ea_extended()
            r = self._inst_rol(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- DEC ---
        elif op == 0x0A:
            ea = self._ea_direct()
            r = self._inst_dec(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x4A:
            self.reg_a = self._inst_dec(self.reg_a)
            cycles += 2
        elif op == 0x5A:
            self.reg_b = self._inst_dec(self.reg_b)
            cycles += 2
        elif op == 0x6A:
            ea, ec = self._ea_indexed()
            r = self._inst_dec(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x7A:
            ea = self._ea_extended()
            r = self._inst_dec(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- INC ---
        elif op == 0x0C:
            ea = self._ea_direct()
            r = self._inst_inc(self._read8(ea))
            self._write8(ea, r)
            cycles += 6
        elif op == 0x4C:
            self.reg_a = self._inst_inc(self.reg_a)
            cycles += 2
        elif op == 0x5C:
            self.reg_b = self._inst_inc(self.reg_b)
            cycles += 2
        elif op == 0x6C:
            ea, ec = self._ea_indexed()
            r = self._inst_inc(self._read8(ea))
            self._write8(ea, r)
            cycles += 6 + ec
        elif op == 0x7C:
            ea = self._ea_extended()
            r = self._inst_inc(self._read8(ea))
            self._write8(ea, r)
            cycles += 7

        # --- TST ---
        elif op == 0x0D:
            ea = self._ea_direct()
            self._inst_tst8(self._read8(ea))
            cycles += 6
        elif op == 0x4D:
            self._inst_tst8(self.reg_a)
            cycles += 2
        elif op == 0x5D:
            self._inst_tst8(self.reg_b)
            cycles += 2
        elif op == 0x6D:
            ea, ec = self._ea_indexed()
            self._inst_tst8(self._read8(ea))
            cycles += 6 + ec
        elif op == 0x7D:
            ea = self._ea_extended()
            self._inst_tst8(self._read8(ea))
            cycles += 7

        # --- JMP ---
        elif op == 0x0E:
            self.reg_pc = self._ea_direct()
            cycles += 3
        elif op == 0x6E:
            ea, ec = self._ea_indexed()
            self.reg_pc = ea
            cycles += 3 + ec
        elif op == 0x7E:
            self.reg_pc = self._ea_extended()
            cycles += 4

        # --- CLR ---
        elif op == 0x0F:
            ea = self._ea_direct()
            self._inst_clr()
            self._write8(ea, 0)
            cycles += 6
        elif op == 0x4F:
            self._inst_clr()
            self.reg_a = 0
            cycles += 2
        elif op == 0x5F:
            self._inst_clr()
            self.reg_b = 0
            cycles += 2
        elif op == 0x6F:
            ea, ec = self._ea_indexed()
            self._inst_clr()
            self._write8(ea, 0)
            cycles += 6 + ec
        elif op == 0x7F:
            ea = self._ea_extended()
            self._inst_clr()
            self._write8(ea, 0)
            cycles += 7

        # --- SUBA ---
        elif op == 0x80:
            self.reg_a = self._inst_sub8(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x90:
            ea = self._ea_direct()
            self.reg_a = self._inst_sub8(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA0:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_sub8(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB0:
            ea = self._ea_extended()
            self.reg_a = self._inst_sub8(self.reg_a, self._read8(ea))
            cycles += 5

        # --- SUBB ---
        elif op == 0xC0:
            self.reg_b = self._inst_sub8(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD0:
            ea = self._ea_direct()
            self.reg_b = self._inst_sub8(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE0:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_sub8(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF0:
            ea = self._ea_extended()
            self.reg_b = self._inst_sub8(self.reg_b, self._read8(ea))
            cycles += 5

        # --- CMPA ---
        elif op == 0x81:
            self._inst_sub8(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x91:
            ea = self._ea_direct()
            self._inst_sub8(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA1:
            ea, ec = self._ea_indexed()
            self._inst_sub8(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB1:
            ea = self._ea_extended()
            self._inst_sub8(self.reg_a, self._read8(ea))
            cycles += 5

        # --- CMPB ---
        elif op == 0xC1:
            self._inst_sub8(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD1:
            ea = self._ea_direct()
            self._inst_sub8(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE1:
            ea, ec = self._ea_indexed()
            self._inst_sub8(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF1:
            ea = self._ea_extended()
            self._inst_sub8(self.reg_b, self._read8(ea))
            cycles += 5

        # --- SBCA ---
        elif op == 0x82:
            self.reg_a = self._inst_sbc(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x92:
            ea = self._ea_direct()
            self.reg_a = self._inst_sbc(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA2:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_sbc(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB2:
            ea = self._ea_extended()
            self.reg_a = self._inst_sbc(self.reg_a, self._read8(ea))
            cycles += 5

        # --- SBCB ---
        elif op == 0xC2:
            self.reg_b = self._inst_sbc(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD2:
            ea = self._ea_direct()
            self.reg_b = self._inst_sbc(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE2:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_sbc(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF2:
            ea = self._ea_extended()
            self.reg_b = self._inst_sbc(self.reg_b, self._read8(ea))
            cycles += 5

        # --- ANDA ---
        elif op == 0x84:
            self.reg_a = self._inst_and(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x94:
            ea = self._ea_direct()
            self.reg_a = self._inst_and(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA4:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_and(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB4:
            ea = self._ea_extended()
            self.reg_a = self._inst_and(self.reg_a, self._read8(ea))
            cycles += 5

        # --- ANDB ---
        elif op == 0xC4:
            self.reg_b = self._inst_and(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD4:
            ea = self._ea_direct()
            self.reg_b = self._inst_and(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE4:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_and(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF4:
            ea = self._ea_extended()
            self.reg_b = self._inst_and(self.reg_b, self._read8(ea))
            cycles += 5

        # --- BITA ---
        elif op == 0x85:
            self._inst_and(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x95:
            ea = self._ea_direct()
            self._inst_and(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA5:
            ea, ec = self._ea_indexed()
            self._inst_and(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB5:
            ea = self._ea_extended()
            self._inst_and(self.reg_a, self._read8(ea))
            cycles += 5

        # --- BITB ---
        elif op == 0xC5:
            self._inst_and(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD5:
            ea = self._ea_direct()
            self._inst_and(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE5:
            ea, ec = self._ea_indexed()
            self._inst_and(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF5:
            ea = self._ea_extended()
            self._inst_and(self.reg_b, self._read8(ea))
            cycles += 5

        # --- LDA ---
        elif op == 0x86:
            self.reg_a = self._pc_read8()
            self._inst_tst8(self.reg_a)
            cycles += 2
        elif op == 0x96:
            ea = self._ea_direct()
            self.reg_a = self._read8(ea)
            self._inst_tst8(self.reg_a)
            cycles += 4
        elif op == 0xA6:
            ea, ec = self._ea_indexed()
            self.reg_a = self._read8(ea)
            self._inst_tst8(self.reg_a)
            cycles += 4 + ec
        elif op == 0xB6:
            ea = self._ea_extended()
            self.reg_a = self._read8(ea)
            self._inst_tst8(self.reg_a)
            cycles += 5

        # --- LDB ---
        elif op == 0xC6:
            self.reg_b = self._pc_read8()
            self._inst_tst8(self.reg_b)
            cycles += 2
        elif op == 0xD6:
            ea = self._ea_direct()
            self.reg_b = self._read8(ea)
            self._inst_tst8(self.reg_b)
            cycles += 4
        elif op == 0xE6:
            ea, ec = self._ea_indexed()
            self.reg_b = self._read8(ea)
            self._inst_tst8(self.reg_b)
            cycles += 4 + ec
        elif op == 0xF6:
            ea = self._ea_extended()
            self.reg_b = self._read8(ea)
            self._inst_tst8(self.reg_b)
            cycles += 5

        # --- STA ---
        elif op == 0x97:
            ea = self._ea_direct()
            self._write8(ea, self.reg_a)
            self._inst_tst8(self.reg_a)
            cycles += 4
        elif op == 0xA7:
            ea, ec = self._ea_indexed()
            self._write8(ea, self.reg_a)
            self._inst_tst8(self.reg_a)
            cycles += 4 + ec
        elif op == 0xB7:
            ea = self._ea_extended()
            self._write8(ea, self.reg_a)
            self._inst_tst8(self.reg_a)
            cycles += 5

        # --- STB ---
        elif op == 0xD7:
            ea = self._ea_direct()
            self._write8(ea, self.reg_b)
            self._inst_tst8(self.reg_b)
            cycles += 4
        elif op == 0xE7:
            ea, ec = self._ea_indexed()
            self._write8(ea, self.reg_b)
            self._inst_tst8(self.reg_b)
            cycles += 4 + ec
        elif op == 0xF7:
            ea = self._ea_extended()
            self._write8(ea, self.reg_b)
            self._inst_tst8(self.reg_b)
            cycles += 5

        # --- EORA ---
        elif op == 0x88:
            self.reg_a = self._inst_eor(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x98:
            ea = self._ea_direct()
            self.reg_a = self._inst_eor(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA8:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_eor(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB8:
            ea = self._ea_extended()
            self.reg_a = self._inst_eor(self.reg_a, self._read8(ea))
            cycles += 5

        # --- EORB ---
        elif op == 0xC8:
            self.reg_b = self._inst_eor(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD8:
            ea = self._ea_direct()
            self.reg_b = self._inst_eor(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE8:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_eor(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF8:
            ea = self._ea_extended()
            self.reg_b = self._inst_eor(self.reg_b, self._read8(ea))
            cycles += 5

        # --- ADCA ---
        elif op == 0x89:
            self.reg_a = self._inst_adc(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x99:
            ea = self._ea_direct()
            self.reg_a = self._inst_adc(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xA9:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_adc(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xB9:
            ea = self._ea_extended()
            self.reg_a = self._inst_adc(self.reg_a, self._read8(ea))
            cycles += 5

        # --- ADCB ---
        elif op == 0xC9:
            self.reg_b = self._inst_adc(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xD9:
            ea = self._ea_direct()
            self.reg_b = self._inst_adc(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xE9:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_adc(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xF9:
            ea = self._ea_extended()
            self.reg_b = self._inst_adc(self.reg_b, self._read8(ea))
            cycles += 5

        # --- ORA ---
        elif op == 0x8A:
            self.reg_a = self._inst_or(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x9A:
            ea = self._ea_direct()
            self.reg_a = self._inst_or(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xAA:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_or(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xBA:
            ea = self._ea_extended()
            self.reg_a = self._inst_or(self.reg_a, self._read8(ea))
            cycles += 5

        # --- ORB ---
        elif op == 0xCA:
            self.reg_b = self._inst_or(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xDA:
            ea = self._ea_direct()
            self.reg_b = self._inst_or(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xEA:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_or(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xFA:
            ea = self._ea_extended()
            self.reg_b = self._inst_or(self.reg_b, self._read8(ea))
            cycles += 5

        # --- ADDA ---
        elif op == 0x8B:
            self.reg_a = self._inst_add8(self.reg_a, self._pc_read8())
            cycles += 2
        elif op == 0x9B:
            ea = self._ea_direct()
            self.reg_a = self._inst_add8(self.reg_a, self._read8(ea))
            cycles += 4
        elif op == 0xAB:
            ea, ec = self._ea_indexed()
            self.reg_a = self._inst_add8(self.reg_a, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xBB:
            ea = self._ea_extended()
            self.reg_a = self._inst_add8(self.reg_a, self._read8(ea))
            cycles += 5

        # --- ADDB ---
        elif op == 0xCB:
            self.reg_b = self._inst_add8(self.reg_b, self._pc_read8())
            cycles += 2
        elif op == 0xDB:
            ea = self._ea_direct()
            self.reg_b = self._inst_add8(self.reg_b, self._read8(ea))
            cycles += 4
        elif op == 0xEB:
            ea, ec = self._ea_indexed()
            self.reg_b = self._inst_add8(self.reg_b, self._read8(ea))
            cycles += 4 + ec
        elif op == 0xFB:
            ea = self._ea_extended()
            self.reg_b = self._inst_add8(self.reg_b, self._read8(ea))
            cycles += 5

        # --- SUBD ---
        elif op == 0x83:
            self._set_reg_d(self._inst_sub16(self._get_reg_d(), self._pc_read16()))
            cycles += 4
        elif op == 0x93:
            ea = self._ea_direct()
            self._set_reg_d(self._inst_sub16(self._get_reg_d(), self._read16(ea)))
            cycles += 6
        elif op == 0xA3:
            ea, ec = self._ea_indexed()
            self._set_reg_d(self._inst_sub16(self._get_reg_d(), self._read16(ea)))
            cycles += 6 + ec
        elif op == 0xB3:
            ea = self._ea_extended()
            self._set_reg_d(self._inst_sub16(self._get_reg_d(), self._read16(ea)))
            cycles += 7

        # --- CMPX ---
        elif op == 0x8C:
            self._inst_sub16(self.reg_x, self._pc_read16())
            cycles += 4
        elif op == 0x9C:
            ea = self._ea_direct()
            self._inst_sub16(self.reg_x, self._read16(ea))
            cycles += 6
        elif op == 0xAC:
            ea, ec = self._ea_indexed()
            self._inst_sub16(self.reg_x, self._read16(ea))
            cycles += 6 + ec
        elif op == 0xBC:
            ea = self._ea_extended()
            self._inst_sub16(self.reg_x, self._read16(ea))
            cycles += 7

        # --- LDX ---
        elif op == 0x8E:
            self.reg_x = self._pc_read16()
            self._inst_tst16(self.reg_x)
            cycles += 3
        elif op == 0x9E:
            ea = self._ea_direct()
            self.reg_x = self._read16(ea)
            self._inst_tst16(self.reg_x)
            cycles += 5
        elif op == 0xAE:
            ea, ec = self._ea_indexed()
            self.reg_x = self._read16(ea)
            self._inst_tst16(self.reg_x)
            cycles += 5 + ec
        elif op == 0xBE:
            ea = self._ea_extended()
            self.reg_x = self._read16(ea)
            self._inst_tst16(self.reg_x)
            cycles += 6

        # --- LDU ---
        elif op == 0xCE:
            self.reg_u = self._pc_read16()
            self._inst_tst16(self.reg_u)
            cycles += 3
        elif op == 0xDE:
            ea = self._ea_direct()
            self.reg_u = self._read16(ea)
            self._inst_tst16(self.reg_u)
            cycles += 5
        elif op == 0xEE:
            ea, ec = self._ea_indexed()
            self.reg_u = self._read16(ea)
            self._inst_tst16(self.reg_u)
            cycles += 5 + ec
        elif op == 0xFE:
            ea = self._ea_extended()
            self.reg_u = self._read16(ea)
            self._inst_tst16(self.reg_u)
            cycles += 6

        # --- STX ---
        elif op == 0x9F:
            ea = self._ea_direct()
            self._write16(ea, self.reg_x)
            self._inst_tst16(self.reg_x)
            cycles += 5
        elif op == 0xAF:
            ea, ec = self._ea_indexed()
            self._write16(ea, self.reg_x)
            self._inst_tst16(self.reg_x)
            cycles += 5 + ec
        elif op == 0xBF:
            ea = self._ea_extended()
            self._write16(ea, self.reg_x)
            self._inst_tst16(self.reg_x)
            cycles += 6

        # --- STU ---
        elif op == 0xDF:
            ea = self._ea_direct()
            self._write16(ea, self.reg_u)
            self._inst_tst16(self.reg_u)
            cycles += 5
        elif op == 0xEF:
            ea, ec = self._ea_indexed()
            self._write16(ea, self.reg_u)
            self._inst_tst16(self.reg_u)
            cycles += 5 + ec
        elif op == 0xFF:
            ea = self._ea_extended()
            self._write16(ea, self.reg_u)
            self._inst_tst16(self.reg_u)
            cycles += 6

        # --- ADDD ---
        elif op == 0xC3:
            self._set_reg_d(self._inst_add16(self._get_reg_d(), self._pc_read16()))
            cycles += 4
        elif op == 0xD3:
            ea = self._ea_direct()
            self._set_reg_d(self._inst_add16(self._get_reg_d(), self._read16(ea)))
            cycles += 6
        elif op == 0xE3:
            ea, ec = self._ea_indexed()
            self._set_reg_d(self._inst_add16(self._get_reg_d(), self._read16(ea)))
            cycles += 6 + ec
        elif op == 0xF3:
            ea = self._ea_extended()
            self._set_reg_d(self._inst_add16(self._get_reg_d(), self._read16(ea)))
            cycles += 7

        # --- LDD ---
        elif op == 0xCC:
            self._set_reg_d(self._pc_read16())
            self._inst_tst16(self._get_reg_d())
            cycles += 3
        elif op == 0xDC:
            ea = self._ea_direct()
            self._set_reg_d(self._read16(ea))
            self._inst_tst16(self._get_reg_d())
            cycles += 5
        elif op == 0xEC:
            ea, ec = self._ea_indexed()
            self._set_reg_d(self._read16(ea))
            self._inst_tst16(self._get_reg_d())
            cycles += 5 + ec
        elif op == 0xFC:
            ea = self._ea_extended()
            self._set_reg_d(self._read16(ea))
            self._inst_tst16(self._get_reg_d())
            cycles += 6

        # --- STD ---
        elif op == 0xDD:
            ea = self._ea_direct()
            self._write16(ea, self._get_reg_d())
            self._inst_tst16(self._get_reg_d())
            cycles += 5
        elif op == 0xED:
            ea, ec = self._ea_indexed()
            self._write16(ea, self._get_reg_d())
            self._inst_tst16(self._get_reg_d())
            cycles += 5 + ec
        elif op == 0xFD:
            ea = self._ea_extended()
            self._write16(ea, self._get_reg_d())
            self._inst_tst16(self._get_reg_d())
            cycles += 6

        # --- NOP ---
        elif op == 0x12:
            cycles += 2

        # --- MUL ---
        elif op == 0x3D:
            r = (self.reg_a & 0xFF) * (self.reg_b & 0xFF)
            self._set_reg_d(r)
            self._set_cc(self.FLAG_Z, self._test_z16(r))
            self._set_cc(self.FLAG_C, (r >> 7) & 1)
            cycles += 11

        # --- BRA / BRN ---
        elif op == 0x20 or op == 0x21:
            cycles += self._inst_bra8(0, op)
        # --- BHI / BLS ---
        elif op == 0x22 or op == 0x23:
            cycles += self._inst_bra8(self._get_cc(self.FLAG_C) | self._get_cc(self.FLAG_Z), op)
        # --- BHS(BCC) / BLO(BCS) ---
        elif op == 0x24 or op == 0x25:
            cycles += self._inst_bra8(self._get_cc(self.FLAG_C), op)
        # --- BNE / BEQ ---
        elif op == 0x26 or op == 0x27:
            cycles += self._inst_bra8(self._get_cc(self.FLAG_Z), op)
        # --- BVC / BVS ---
        elif op == 0x28 or op == 0x29:
            cycles += self._inst_bra8(self._get_cc(self.FLAG_V), op)
        # --- BPL / BMI ---
        elif op == 0x2A or op == 0x2B:
            cycles += self._inst_bra8(self._get_cc(self.FLAG_N), op)
        # --- BGE / BLT ---
        elif op == 0x2C or op == 0x2D:
            cycles += self._inst_bra8(self._get_cc(self.FLAG_N) ^ self._get_cc(self.FLAG_V), op)
        # --- BGT / BLE ---
        elif op == 0x2E or op == 0x2F:
            cycles += self._inst_bra8(
                self._get_cc(self.FLAG_Z) | (self._get_cc(self.FLAG_N) ^ self._get_cc(self.FLAG_V)), op)

        # --- LBRA ---
        elif op == 0x16:
            r = self._pc_read16()
            self.reg_pc = (self.reg_pc + r) & 0xFFFF
            cycles += 5

        # --- LBSR ---
        elif op == 0x17:
            r = self._pc_read16()
            self._push16_s(self.reg_pc)
            self.reg_pc = (self.reg_pc + r) & 0xFFFF
            cycles += 9

        # --- BSR ---
        elif op == 0x8D:
            r = self._pc_read8()
            self._push16_s(self.reg_pc)
            self.reg_pc = (self.reg_pc + self._sign_extend(r)) & 0xFFFF
            cycles += 7

        # --- JSR ---
        elif op == 0x9D:
            ea = self._ea_direct()
            self._push16_s(self.reg_pc)
            self.reg_pc = ea
            cycles += 7
        elif op == 0xAD:
            ea, ec = self._ea_indexed()
            self._push16_s(self.reg_pc)
            self.reg_pc = ea
            cycles += 7 + ec
        elif op == 0xBD:
            ea = self._ea_extended()
            self._push16_s(self.reg_pc)
            self.reg_pc = ea
            cycles += 8

        # --- LEAX ---
        elif op == 0x30:
            ea, ec = self._ea_indexed()
            self.reg_x = ea & 0xFFFF
            self._set_cc(self.FLAG_Z, self._test_z16(self.reg_x))
            cycles += 4 + ec

        # --- LEAY ---
        elif op == 0x31:
            ea, ec = self._ea_indexed()
            self.reg_y = ea & 0xFFFF
            self._set_cc(self.FLAG_Z, self._test_z16(self.reg_y))
            cycles += 4 + ec

        # --- LEAS ---
        elif op == 0x32:
            ea, ec = self._ea_indexed()
            self.reg_s = ea & 0xFFFF
            cycles += 4 + ec

        # --- LEAU ---
        elif op == 0x33:
            ea, ec = self._ea_indexed()
            self.reg_u = ea & 0xFFFF
            cycles += 4 + ec

        # --- PSHS ---
        elif op == 0x34:
            pb = self._pc_read8()
            cycles += self._inst_psh_s(pb, self.reg_u)
            cycles += 5

        # --- PULS ---
        elif op == 0x35:
            pb = self._pc_read8()
            cycles += self._inst_pul_s(pb)
            cycles += 5

        # --- PSHU ---
        elif op == 0x36:
            pb = self._pc_read8()
            cycles += self._inst_psh_u(pb, self.reg_s)
            cycles += 5

        # --- PULU ---
        elif op == 0x37:
            pb = self._pc_read8()
            cycles += self._inst_pul_u(pb)
            cycles += 5

        # --- RTS ---
        elif op == 0x39:
            self.reg_pc = self._pull16_s()
            cycles += 5

        # --- ABX ---
        elif op == 0x3A:
            self.reg_x = (self.reg_x + (self.reg_b & 0xFF)) & 0xFFFF
            cycles += 3

        # --- ORCC ---
        elif op == 0x1A:
            self.reg_cc |= self._pc_read8()
            self.reg_cc &= 0xFF
            cycles += 3

        # --- ANDCC ---
        elif op == 0x1C:
            self.reg_cc &= self._pc_read8()
            cycles += 3

        # --- SEX ---
        elif op == 0x1D:
            self._set_reg_d(self._sign_extend(self.reg_b))
            self._set_cc(self.FLAG_N, self._test_n(self.reg_a))
            self._set_cc(self.FLAG_Z, self._test_z16(self._get_reg_d()))
            cycles += 2

        # --- EXG ---
        elif op == 0x1E:
            self._inst_exg()
            cycles += 8

        # --- TFR ---
        elif op == 0x1F:
            self._inst_tfr()
            cycles += 6

        # --- RTI ---
        elif op == 0x3B:
            if self._get_cc(self.FLAG_E):
                cycles += self._inst_pul_s(0xFF)
            else:
                cycles += self._inst_pul_s(0x81)
            cycles += 3

        # --- SWI ---
        elif op == 0x3F:
            self._set_cc(self.FLAG_E, 1)
            cycles += self._inst_psh_s(0xFF, self.reg_u)
            self._set_cc(self.FLAG_I, 1)
            self._set_cc(self.FLAG_F, 1)
            self.reg_pc = self._read16(0xFFFA)
            cycles += 7

        # --- SYNC ---
        elif op == 0x13:
            self.irq_status = self.IRQ_SYNC
            cycles += 2

        # --- DAA ---
        elif op == 0x19:
            i0 = self.reg_a & 0xFF
            i1 = 0
            if (self.reg_a & 0x0F) > 0x09 or self._get_cc(self.FLAG_H) == 1:
                i1 |= 0x06
            if (self.reg_a & 0xF0) > 0x80 and (self.reg_a & 0x0F) > 0x09:
                i1 |= 0x60
            if (self.reg_a & 0xF0) > 0x90 or self._get_cc(self.FLAG_C) == 1:
                i1 |= 0x60
            self.reg_a = (i0 + i1) & 0xFF
            self._set_cc(self.FLAG_N, self._test_n(self.reg_a))
            self._set_cc(self.FLAG_Z, self._test_z8(self.reg_a))
            self._set_cc(self.FLAG_V, 0)
            self._set_cc(self.FLAG_C, self._test_c(i0, i1, i0 + i1, 0))
            cycles += 2

        # --- CWAI ---
        elif op == 0x3C:
            self.reg_cc &= self._pc_read8()
            self.reg_cc &= 0xFF
            self._set_cc(self.FLAG_E, 1)
            cycles += self._inst_psh_s(0xFF, self.reg_u)
            self.irq_status = self.IRQ_CWAI
            cycles += 4

        # ==============================================================
        # Page 1 instructions  (0x10 prefix)
        # ==============================================================
        elif op == 0x10:
            op = self._pc_read8()

            # --- LBRA / LBRN ---
            if op == 0x20 or op == 0x21:
                cycles += self._inst_bra16(0, op)
            # --- LBHI / LBLS ---
            elif op == 0x22 or op == 0x23:
                cycles += self._inst_bra16(self._get_cc(self.FLAG_C) | self._get_cc(self.FLAG_Z), op)
            # --- LBHS / LBLO ---
            elif op == 0x24 or op == 0x25:
                cycles += self._inst_bra16(self._get_cc(self.FLAG_C), op)
            # --- LBNE / LBEQ ---
            elif op == 0x26 or op == 0x27:
                cycles += self._inst_bra16(self._get_cc(self.FLAG_Z), op)
            # --- LBVC / LBVS ---
            elif op == 0x28 or op == 0x29:
                cycles += self._inst_bra16(self._get_cc(self.FLAG_V), op)
            # --- LBPL / LBMI ---
            elif op == 0x2A or op == 0x2B:
                cycles += self._inst_bra16(self._get_cc(self.FLAG_N), op)
            # --- LBGE / LBLT ---
            elif op == 0x2C or op == 0x2D:
                cycles += self._inst_bra16(self._get_cc(self.FLAG_N) ^ self._get_cc(self.FLAG_V), op)
            # --- LBGT / LBLE ---
            elif op == 0x2E or op == 0x2F:
                cycles += self._inst_bra16(
                    self._get_cc(self.FLAG_Z) | (self._get_cc(self.FLAG_N) ^ self._get_cc(self.FLAG_V)), op)

            # --- CMPD ---
            elif op == 0x83:
                self._inst_sub16(self._get_reg_d(), self._pc_read16())
                cycles += 5
            elif op == 0x93:
                ea = self._ea_direct()
                self._inst_sub16(self._get_reg_d(), self._read16(ea))
                cycles += 7
            elif op == 0xA3:
                ea, ec = self._ea_indexed()
                self._inst_sub16(self._get_reg_d(), self._read16(ea))
                cycles += 7 + ec
            elif op == 0xB3:
                ea = self._ea_extended()
                self._inst_sub16(self._get_reg_d(), self._read16(ea))
                cycles += 8

            # --- CMPY ---
            elif op == 0x8C:
                self._inst_sub16(self.reg_y, self._pc_read16())
                cycles += 5
            elif op == 0x9C:
                ea = self._ea_direct()
                self._inst_sub16(self.reg_y, self._read16(ea))
                cycles += 7
            elif op == 0xAC:
                ea, ec = self._ea_indexed()
                self._inst_sub16(self.reg_y, self._read16(ea))
                cycles += 7 + ec
            elif op == 0xBC:
                ea = self._ea_extended()
                self._inst_sub16(self.reg_y, self._read16(ea))
                cycles += 8

            # --- LDY ---
            elif op == 0x8E:
                self.reg_y = self._pc_read16()
                self._inst_tst16(self.reg_y)
                cycles += 4
            elif op == 0x9E:
                ea = self._ea_direct()
                self.reg_y = self._read16(ea)
                self._inst_tst16(self.reg_y)
                cycles += 6
            elif op == 0xAE:
                ea, ec = self._ea_indexed()
                self.reg_y = self._read16(ea)
                self._inst_tst16(self.reg_y)
                cycles += 6 + ec
            elif op == 0xBE:
                ea = self._ea_extended()
                self.reg_y = self._read16(ea)
                self._inst_tst16(self.reg_y)
                cycles += 7

            # --- STY ---
            elif op == 0x9F:
                ea = self._ea_direct()
                self._write16(ea, self.reg_y)
                self._inst_tst16(self.reg_y)
                cycles += 6
            elif op == 0xAF:
                ea, ec = self._ea_indexed()
                self._write16(ea, self.reg_y)
                self._inst_tst16(self.reg_y)
                cycles += 6 + ec
            elif op == 0xBF:
                ea = self._ea_extended()
                self._write16(ea, self.reg_y)
                self._inst_tst16(self.reg_y)
                cycles += 7

            # --- LDS ---
            elif op == 0xCE:
                self.reg_s = self._pc_read16()
                self._inst_tst16(self.reg_s)
                cycles += 4
            elif op == 0xDE:
                ea = self._ea_direct()
                self.reg_s = self._read16(ea)
                self._inst_tst16(self.reg_s)
                cycles += 6
            elif op == 0xEE:
                ea, ec = self._ea_indexed()
                self.reg_s = self._read16(ea)
                self._inst_tst16(self.reg_s)
                cycles += 6 + ec
            elif op == 0xFE:
                ea = self._ea_extended()
                self.reg_s = self._read16(ea)
                self._inst_tst16(self.reg_s)
                cycles += 7

            # --- STS ---
            elif op == 0xDF:
                ea = self._ea_direct()
                self._write16(ea, self.reg_s)
                self._inst_tst16(self.reg_s)
                cycles += 6
            elif op == 0xEF:
                ea, ec = self._ea_indexed()
                self._write16(ea, self.reg_s)
                self._inst_tst16(self.reg_s)
                cycles += 6 + ec
            elif op == 0xFF:
                ea = self._ea_extended()
                self._write16(ea, self.reg_s)
                self._inst_tst16(self.reg_s)
                cycles += 7

            # --- SWI2 ---
            elif op == 0x3F:
                self._set_cc(self.FLAG_E, 1)
                cycles += self._inst_psh_s(0xFF, self.reg_u)
                self.reg_pc = self._read16(0xFFF4)
                cycles += 8

        # ==============================================================
        # Page 2 instructions  (0x11 prefix)
        # ==============================================================
        elif op == 0x11:
            op = self._pc_read8()

            # --- CMPU ---
            if op == 0x83:
                self._inst_sub16(self.reg_u, self._pc_read16())
                cycles += 5
            elif op == 0x93:
                ea = self._ea_direct()
                self._inst_sub16(self.reg_u, self._read16(ea))
                cycles += 7
            elif op == 0xA3:
                ea, ec = self._ea_indexed()
                self._inst_sub16(self.reg_u, self._read16(ea))
                cycles += 7 + ec
            elif op == 0xB3:
                ea = self._ea_extended()
                self._inst_sub16(self.reg_u, self._read16(ea))
                cycles += 8

            # --- CMPS ---
            elif op == 0x8C:
                self._inst_sub16(self.reg_s, self._pc_read16())
                cycles += 5
            elif op == 0x9C:
                ea = self._ea_direct()
                self._inst_sub16(self.reg_s, self._read16(ea))
                cycles += 7
            elif op == 0xAC:
                ea, ec = self._ea_indexed()
                self._inst_sub16(self.reg_s, self._read16(ea))
                cycles += 7 + ec
            elif op == 0xBC:
                ea = self._ea_extended()
                self._inst_sub16(self.reg_s, self._read16(ea))
                cycles += 8

            # --- SWI3 ---
            elif op == 0x3F:
                self._set_cc(self.FLAG_E, 1)
                cycles += self._inst_psh_s(0xFF, self.reg_u)
                self.reg_pc = self._read16(0xFFF2)
                cycles += 8

        return cycles
