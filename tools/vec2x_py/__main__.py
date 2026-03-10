"""Entry point: python -m tools.vec2x_py --rom <rom> [--cart <cart>]"""

import argparse
import sys

from .emulator import Emulator


def main():
    parser = argparse.ArgumentParser(description='Vec2X-Py: Python Vectrex Emulator')
    parser.add_argument('--rom', required=True, help='Path to Vectrex system ROM')
    parser.add_argument('--cart', default=None, help='Path to cartridge ROM')
    args = parser.parse_args()

    print('Vec2X-Py starting...')
    print(f'ROM: {args.rom}')
    if args.cart:
        print(f'Cart: {args.cart}')

    emu = Emulator(args.rom, args.cart)
    emu.run()

    print('Vec2X-Py ended.')


if __name__ == '__main__':
    main()
