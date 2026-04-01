"""
Emulator tab — Embeds the iQL Sinclair QL emulator in a Tkinter notebook tab.

Provides Build & Run workflow: export sprites, compile, and test in the
embedded emulator without leaving QL Studio. Includes CPU state debug panel.
"""

import tkinter as tk
from tkinter import ttk, messagebox
import subprocess
import os
import sys

# iQL system path (where ROMs and ql.ini live)
IQL_SYSTEM_PATH = os.path.expanduser("~/Documents/iQLmac/")

# Emulator tick interval in ms (matches iQL's 20ms timer)
EMU_TICK_MS = 20

# How often to update the debug panel (every Nth tick)
DEBUG_UPDATE_INTERVAL = 5

# Debug panel fixed width in pixels
DEBUG_PANEL_WIDTH = 420

# Console log height in lines
CONSOLE_HEIGHT = 8

# Max lines kept in console before trimming
CONSOLE_MAX_LINES = 500

# Try importing the _iql extension and PIL
try:
    ext_dir = os.path.join(os.path.dirname(__file__), "iql_ext")
    if ext_dir not in sys.path:
        sys.path.insert(0, ext_dir)
    import _iql
    HAS_IQL = True
except ImportError:
    HAS_IQL = False

try:
    from PIL import Image, ImageTk
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# Keysym-based lookup for special keys (no printable char)
_KEYSYM_TO_VK = {}

# Character-based lookup: replicates iQL's character_to_vk() + ql_shift_key_fixes()
_CHAR_TO_VK = {}

if HAS_IQL:
    # Special keys (keysym lookup, no shift override)
    _KEYSYM_TO_VK = {
        'space': (_iql.VK_SPACE, -1),
        'Return': (_iql.VK_RETURN, -1),
        'Escape': (_iql.VK_ESCAPE, -1),
        'Tab': (_iql.VK_TAB, -1),
        'BackSpace': (_iql.VK_BACKSPACE, -1),
        'Left': (_iql.VK_LEFT, -1),
        'Right': (_iql.VK_RIGHT, -1),
        'Up': (_iql.VK_UP, -1),
        'Down': (_iql.VK_DOWN, -1),
        'F1': (_iql.VK_F1, -1), 'F2': (_iql.VK_F2, -1),
        'F3': (_iql.VK_F3, -1), 'F4': (_iql.VK_F4, -1),
        'F5': (_iql.VK_F5, -1),
        'Shift_L': (_iql.VK_LSHIFT, -1),
        'Shift_R': (_iql.VK_RSHIFT, -1),
        'Control_L': (_iql.VK_LCONTROL, -1),
        'Control_R': (_iql.VK_RCONTROL, -1),
        'Alt_L': (_iql.VK_LALT, -1),
        'Alt_R': (_iql.VK_RALT, -1),
    }

    # Replicate iQL's KeyCharacterSet: character → VK by position
    _KEY_CHAR_SET = "ABCDEFGHIJKLMNOPQRSTUVWXZY0123456789()[]{}:;,.'\"/\\~=-+-*/<>!?@#$%^&|_` \t\r"
    for i, ch in enumerate(_KEY_CHAR_SET):
        _CHAR_TO_VK[ch] = i

    # ql_shift_key_fixes(): shifted char → (base VK, force_shift=1)
    _SHIFT_FIXES = {
        '!': _iql.VK_1, '@': _iql.VK_2, '#': _iql.VK_3,
        '$': _iql.VK_4, '%': _iql.VK_5, '^': _iql.VK_6,
        '&': _iql.VK_7, '*': _iql.VK_8, '(': _iql.VK_9,
        ')': _iql.VK_0, '_': _iql.VK_DASH, '+': _iql.VK_EQUAL,
        '|': _iql.VK_BACKSLASH, '~': _iql.VK_GRAVE,
        '{': _iql.VK_LBRACKET, '}': _iql.VK_RBRACKET,
        '[': _iql.VK_LBRACKET, ']': _iql.VK_RBRACKET,
        ':': _iql.VK_SEMICOLON, '"': _iql.VK_QUOTE,
        '<': _iql.VK_COMMA, '>': _iql.VK_PERIOD,
        '?': _iql.VK_SLASH,
    }


def _format_sr_flags(sr):
    """Decode 68000 status register into flag string."""
    flags = []
    flags.append('T' if sr & 0x8000 else '-')
    flags.append('S' if sr & 0x2000 else '-')
    ipl = (sr >> 8) & 7
    flags.append(str(ipl))
    flags.append('X' if sr & 0x0010 else '-')
    flags.append('N' if sr & 0x0008 else '-')
    flags.append('Z' if sr & 0x0004 else '-')
    flags.append('V' if sr & 0x0002 else '-')
    flags.append('C' if sr & 0x0001 else '-')
    return ''.join(flags)


class EmulatorTab:
    """Emulator tab for the QL Studio notebook."""

    def __init__(self, parent, editor):
        self.parent = parent
        self.editor = editor
        self.root = parent.winfo_toplevel()
        self._after_id = None
        self._emu_active = False
        self._emu_paused = False
        self._photo = None
        self._tick_count = 0
        self._ql_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

        self._build_ui()

    def _build_ui(self):
        if not HAS_IQL:
            lbl = ttk.Label(self.parent,
                            text="Emulator not available.\n\n"
                                 "Build the extension: cd iql_ext && make",
                            anchor=tk.CENTER, justify=tk.CENTER)
            lbl.pack(expand=True)
            return

        if not HAS_PIL:
            lbl = ttk.Label(self.parent,
                            text="PIL/Pillow required for emulator display.\n\n"
                                 "Install: pip3 install Pillow",
                            anchor=tk.CENTER, justify=tk.CENTER)
            lbl.pack(expand=True)
            return

        # Toolbar
        toolbar = ttk.Frame(self.parent)
        toolbar.pack(fill=tk.X, padx=5, pady=(5, 0))

        self.btn_build_run = ttk.Button(toolbar, text="Build & Run",
                                        command=self._build_and_run)
        self.btn_build_run.pack(side=tk.LEFT, padx=2)

        self.btn_pause = ttk.Button(toolbar, text="Pause",
                                    command=self._toggle_pause, state=tk.DISABLED)
        self.btn_pause.pack(side=tk.LEFT, padx=2)

        self.btn_restart = ttk.Button(toolbar, text="Restart",
                                      command=self._emu_restart, state=tk.DISABLED)
        self.btn_restart.pack(side=tk.LEFT, padx=2)

        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(
            side=tk.LEFT, fill=tk.Y, padx=5, pady=2)

        self.speed_var = tk.StringVar(value="Normal")
        ttk.Label(toolbar, text="Speed:").pack(side=tk.LEFT, padx=(0, 2))
        speed_cb = ttk.Combobox(toolbar, textvariable=self.speed_var,
                                values=["Normal", "Slow"], width=8,
                                state="readonly")
        speed_cb.pack(side=tk.LEFT)
        speed_cb.bind("<<ComboboxSelected>>", self._on_speed_change)

        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(
            side=tk.LEFT, fill=tk.Y, padx=5, pady=2)

        ttk.Button(toolbar, text="Screenshot",
                   command=self._screenshot).pack(side=tk.LEFT, padx=2)


    # --- Trap logging ---

    def _toggle_trap_log(self):
        """Toggle QDOS trap logging to console."""
        if not self._emu_active:
            return
        enabled = self.trap_log_var.get()
        _iql.set_trap_logging(1 if enabled else 0)
        state = "ON" if enabled else "OFF"
        self._console_append(f"--- Trap logging {state} ---\n")

    def _drain_trap_log(self):
        """Drain trap log buffer and append to console."""
        if not self._emu_active or not self.trap_log_var.get():
            return
        log = _iql.get_trap_log()
        if log:
            self._console_append(log, "dbg")

    def cleanup(self):
        """Clean up on application exit."""
        self._emu_stop()
