"""
Emulator tab — Embeds the iQL Sinclair QL emulator in a Tkinter notebook tab.

Provides Build & Run workflow: export sprites, compile, and test in the
embedded emulator without leaving the sprite editor.
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

# Tkinter keysym → iQL virtual key code mapping
_TK_TO_VK = {}
if HAS_IQL:
    _TK_TO_VK = {
        'a': _iql.VK_A, 'b': _iql.VK_B, 'c': _iql.VK_C, 'd': _iql.VK_D,
        'e': _iql.VK_E, 'f': _iql.VK_F, 'g': _iql.VK_G, 'h': _iql.VK_H,
        'i': _iql.VK_I, 'j': _iql.VK_J, 'k': _iql.VK_K, 'l': _iql.VK_L,
        'm': _iql.VK_M, 'n': _iql.VK_N, 'o': _iql.VK_O, 'p': _iql.VK_P,
        'q': _iql.VK_Q, 'r': _iql.VK_R, 's': _iql.VK_S, 't': _iql.VK_T,
        'u': _iql.VK_U, 'v': _iql.VK_V, 'w': _iql.VK_W, 'x': _iql.VK_X,
        'y': _iql.VK_Y, 'z': _iql.VK_Z,
        '0': _iql.VK_0, '1': _iql.VK_1, '2': _iql.VK_2, '3': _iql.VK_3,
        '4': _iql.VK_4, '5': _iql.VK_5, '6': _iql.VK_6, '7': _iql.VK_7,
        '8': _iql.VK_8, '9': _iql.VK_9,
        'space': _iql.VK_SPACE, 'Return': _iql.VK_RETURN,
        'Escape': _iql.VK_ESCAPE, 'Tab': _iql.VK_TAB,
        'BackSpace': _iql.VK_BACKSPACE,
        'Left': _iql.VK_LEFT, 'Right': _iql.VK_RIGHT,
        'Up': _iql.VK_UP, 'Down': _iql.VK_DOWN,
        'F1': _iql.VK_F1, 'F2': _iql.VK_F2, 'F3': _iql.VK_F3,
        'F4': _iql.VK_F4, 'F5': _iql.VK_F5,
        'comma': _iql.VK_COMMA, 'period': _iql.VK_PERIOD,
        'slash': _iql.VK_SLASH, 'semicolon': _iql.VK_SEMICOLON,
        'minus': _iql.VK_DASH, 'equal': _iql.VK_EQUAL,
        'Shift_L': _iql.VK_LSHIFT, 'Shift_R': _iql.VK_RSHIFT,
        'Control_L': _iql.VK_LCONTROL, 'Control_R': _iql.VK_RCONTROL,
        'Alt_L': _iql.VK_LALT, 'Alt_R': _iql.VK_RALT,
    }


class EmulatorTab:
    """Emulator tab for the QL sprite editor notebook."""

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
        self._ql_dir = os.path.dirname(os.path.abspath(__file__))

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

    def _on_emu_key_press(self, event):
        """Forward key press to emulator."""
        if not self._emu_active:
            return
        vk = _TK_TO_VK.get(event.keysym)
        if vk is not None:
            shift = 1 if event.state & 0x0001 else 0
            ctrl = 1 if event.state & 0x0004 else 0
            alt = 1 if event.state & 0x0008 else 0
            _iql.send_key(vk, 1, shift, ctrl, alt)

    def _on_emu_key_release(self, event):
        """Forward key release to emulator."""
        if not self._emu_active:
            return
        vk = _TK_TO_VK.get(event.keysym)
        if vk is not None:
            _iql.send_key(vk, 0)

    def on_tab_selected(self):
        """Called when this tab becomes active."""
        if self._emu_active:
            self.root.bind("<KeyPress>", self._on_emu_key_press)
            self.root.bind("<KeyRelease>", self._on_emu_key_release)

    def on_tab_deselected(self):
        """Called when switching away from this tab."""
        # Restore sprite editor key bindings
        self.root.bind("<KeyPress>", None)
        self.root.bind("<KeyRelease>", None)
        self.root.bind("<Key>", self.editor._on_key)

    def cleanup(self):
        """Clean up on application exit."""
        self._emu_stop()
