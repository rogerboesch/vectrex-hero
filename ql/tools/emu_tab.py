"""
Emulator tab — Embeds the iQL Sinclair QL emulator in a Tkinter notebook tab.

Provides Build & Run workflow: export sprites, compile, and test in the
embedded emulator without leaving QL Studio.
"""

import tkinter as tk
from tkinter import ttk
import subprocess
import os
import sys
import shutil

# iQL system path (where ROMs and ql.ini live)
IQL_SYSTEM_PATH = os.path.expanduser("~/Documents/iQLmac/")

# Emulator tick interval in ms (matches iQL's 20ms timer)
EMU_TICK_MS = 20

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


class EmulatorTab:
    """Emulator tab for the QL Studio notebook."""

    def __init__(self, parent, editor):
        """
        Args:
            parent: ttk.Frame to build into (the notebook tab)
            editor: SpriteEditor instance (for accessing sprites/project)
        """
        self.parent = parent
        self.editor = editor
        self.root = parent.winfo_toplevel()
        self._after_id = None
        self._emu_active = False
        self._photo = None
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

        self.btn_stop = ttk.Button(toolbar, text="Stop",
                                   command=self._emu_stop, state=tk.DISABLED)
        self.btn_stop.pack(side=tk.LEFT, padx=2)

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

        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = ttk.Label(toolbar, textvariable=self.status_var)
        status_bar.pack(side=tk.RIGHT, padx=5)

        # Emulator screen canvas (512x256 native, displayed at 2x)
        screen_frame = ttk.Frame(self.parent)
        screen_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.emu_canvas = tk.Canvas(screen_frame, bg="#000000",
                                    width=1024, height=512,
                                    highlightthickness=0)
        self.emu_canvas.pack(expand=True)

        # Make canvas focusable so it captures keys instead of buttons
        self.emu_canvas.config(takefocus=True)
        self.emu_canvas.bind("<Button-1>", lambda e: self.emu_canvas.focus_set())

    def _build_and_run(self):
        """Export sprites, compile the game, and start the emulator."""
        self.status_var.set("Exporting sprites...")
        self.root.update_idletasks()

        # Export sprites.c / sprites.h to the ql/ directory
        self.editor._write_c_files(self._ql_dir)

        # Run make
        self.status_var.set("Compiling...")
        self.root.update_idletasks()

        result = subprocess.run(
            ["make"], cwd=self._ql_dir,
            capture_output=True, text=True, timeout=30
        )

        if result.returncode != 0:
            self.status_var.set("Build FAILED")
            from tkinter import messagebox
            messagebox.showerror("Build Error",
                                 f"make failed:\n{result.stderr[-500:]}")
            return

        self.status_var.set("Starting emulator...")
        self.root.update_idletasks()

        # Stop any running emulator
        self._emu_stop()

        # Start emulator
        _iql.init(IQL_SYSTEM_PATH)

        self._emu_active = True
        self.btn_build_run.config(text="Rebuild & Run")
        self.btn_stop.config(state=tk.NORMAL)
        self.btn_restart.config(state=tk.NORMAL)
        self.status_var.set("Running")

        # Bind keyboard events for this tab
        self.root.bind("<KeyPress>", self._on_emu_key_press)
        self.root.bind("<KeyRelease>", self._on_emu_key_release)

        # Focus the canvas so Space/keys go to emulator, not buttons
        self.emu_canvas.focus_set()

        # Start tick loop
        self._emu_tick()

    def _emu_stop(self):
        """Stop the emulator."""
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
            self._after_id = None

        if self._emu_active:
            _iql.stop()
            self._emu_active = False
            self.btn_stop.config(state=tk.DISABLED)
            self.btn_restart.config(state=tk.DISABLED)
            self.status_var.set("Stopped")

    def _emu_restart(self):
        """Restart the emulator."""
        if self._emu_active:
            _iql.restart()
            self.status_var.set("Restarting...")

    def _on_speed_change(self, event=None):
        """Handle speed combobox change."""
        speed = 0 if self.speed_var.get() == "Normal" else 4
        if self._emu_active:
            _iql.set_speed(speed)

    def _emu_tick(self):
        """Advance emulation and update display."""
        if not self._emu_active:
            return

        # Advance emulation
        _iql.tick()

        # Get framebuffer and display
        fb = _iql.get_framebuffer()
        if fb:
            w, h = _iql.get_screen_size()
            img = Image.frombytes('RGBA', (w, h), fb)
            # Scale 2x for display
            img = img.resize((w * 2, h * 2), Image.NEAREST)
            self._photo = ImageTk.PhotoImage(img)
            self.emu_canvas.delete("all")
            self.emu_canvas.create_image(0, 0, anchor=tk.NW, image=self._photo)

        # Schedule next tick
        self._after_id = self.root.after(EMU_TICK_MS, self._emu_tick)

    def _resolve_key(self, event):
        """Resolve Tkinter event to (vk, shift) matching iQL's processEvent.

        Replicates rb_renderview.m: character_to_vk() + ql_shift_key_fixes().
        Returns (vk_code, shift) or None.
        """
        shift = 1 if event.state & 0x0001 else 0

        # 1. Special keys (no printable char) — lookup by keysym
        entry = _KEYSYM_TO_VK.get(event.keysym)
        if entry is not None:
            vk, force_shift = entry
            if force_shift >= 0:
                shift = force_shift
            return vk, shift

        # 2. Printable chars — replicate iQL's character_to_vk + ql_shift_key_fixes
        ch = event.char
        if not ch:
            return None

        # Check shift fixes first (like iQL's ql_shift_key_fixes)
        fix = _SHIFT_FIXES.get(ch)
        if fix is not None:
            return fix, 1

        # Uppercase → base letter + shift (like iQL's charactersIgnoringModifiers)
        if ch.isupper():
            vk = _CHAR_TO_VK.get(ch)
            if vk is not None:
                return vk, 1

        # Regular character lookup (like iQL's character_to_vk on uppercase)
        vk = _CHAR_TO_VK.get(ch.upper())
        if vk is not None:
            return vk, shift

        return None

    def _on_emu_key_press(self, event):
        """Forward key press to emulator."""
        if not self._emu_active:
            return
        result = self._resolve_key(event)
        if result is not None:
            vk, shift = result
            ctrl = 1 if event.state & 0x0004 else 0
            alt = 1 if event.state & 0x0008 else 0
            _iql.send_key(vk, 1, shift, ctrl, alt)

    def _on_emu_key_release(self, event):
        """Forward key release to emulator."""
        if not self._emu_active:
            return
        result = self._resolve_key(event)
        if result is not None:
            vk, shift = result
            _iql.send_key(vk, 0, shift)

    def on_tab_selected(self):
        """Called when this tab becomes active."""
        if self._emu_active:
            self.root.bind("<KeyPress>", self._on_emu_key_press)
            self.root.bind("<KeyRelease>", self._on_emu_key_release)
            self.emu_canvas.focus_set()

    def on_tab_deselected(self):
        """Called when switching away from this tab."""
        # Restore Sprites tab key bindings
        self.root.bind("<KeyPress>", None)
        self.root.bind("<KeyRelease>", None)
        self.root.bind("<Key>", self.editor._on_key)

    def cleanup(self):
        """Clean up on application exit."""
        self._emu_stop()
