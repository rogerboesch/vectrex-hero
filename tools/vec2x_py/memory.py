"""Memory bus: ROM, RAM, cartridge, and I/O dispatch."""


class MemoryBus:
    __slots__ = ('rom', 'cart', 'ram', 'via')

    def __init__(self, via):
        self.rom = bytearray(8192)      # 0xE000-0xFFFF system ROM
        self.cart = bytearray(32768)    # 0x0000-0x7FFF cartridge
        self.ram = bytearray(1024)      # 0xC800-0xCFFF
        self.via = via

    def load_rom(self, path):
        with open(path, 'rb') as f:
            data = f.read(8192)
        self.rom[:len(data)] = data

    def load_cart(self, path):
        with open(path, 'rb') as f:
            data = f.read(32768)
        self.cart[:len(data)] = data

    def read8(self, address):
        address &= 0xFFFF

        if (address & 0xE000) == 0xE000:
            return self.rom[address & 0x1FFF]

        if (address & 0xE000) == 0xC000:
            if address & 0x800:
                return self.ram[address & 0x3FF]
            if address & 0x1000:
                return self.via.read(address & 0xF)
            return 0xFF

        if address < 0x8000:
            return self.cart[address]

        return 0xFF

    def write8(self, address, data):
        address &= 0xFFFF
        data &= 0xFF

        if (address & 0xE000) == 0xE000:
            return  # ROM is read-only

        if (address & 0xE000) == 0xC000:
            # RAM and I/O can both be written simultaneously
            if address & 0x800:
                self.ram[address & 0x3FF] = data
            if address & 0x1000:
                self.via.write(address & 0xF, data)
            return

        # address < 0x8000: cartridge ROM, ignore writes

    def reset(self):
        for r in range(1024):
            self.ram[r] = r & 0xFF
