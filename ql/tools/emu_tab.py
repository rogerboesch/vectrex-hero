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
DEBUG_PANEL_WIDTH = 290

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

        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = ttk.Label(toolbar, textvariable=self.status_var)
        status_bar.pack(side=tk.RIGHT, padx=5)

        # Main area: screen canvas + debug panel
        main_frame = ttk.Frame(self.parent)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Debug panel (right side, fixed width, packed first so it claims space)
        debug_frame = ttk.LabelFrame(main_frame, text="CPU State",
                                     width=DEBUG_PANEL_WIDTH)
        debug_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
        debug_frame.pack_propagate(False)

        self.debug_text = tk.Text(debug_frame, width=34, height=28,
                                  font=("Courier", 11), state=tk.DISABLED,
                                  bg="#1e1e1e", fg="#d4d4d4",
                                  relief=tk.FLAT, padx=6, pady=6,
                                  wrap=tk.NONE)
        self.debug_text.pack(fill=tk.BOTH, expand=True)

        # Emulator screen canvas (fills remaining space)
        screen_frame = ttk.Frame(main_frame)
        screen_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.emu_canvas = tk.Canvas(screen_frame, bg="#000000",
                                    highlightthickness=0)
        self.emu_canvas.pack(fill=tk.BOTH, expand=True)

        # Make canvas focusable so it captures keys instead of buttons
        self.emu_canvas.config(takefocus=True)
        self.emu_canvas.bind("<Button-1>", lambda e: self.emu_canvas.focus_set())
        self.emu_canvas.bind("<Configure>", self._on_canvas_resize)

        # Tag for highlighting PC line
        self.debug_text.tag_configure("highlight", foreground="#4ec9b0")
        self.debug_text.tag_configure("dim", foreground="#808080")

    def _on_canvas_resize(self, event):
        """Redraw framebuffer when canvas resizes."""
        if self._emu_active and self._photo:
            self._update_display()

    def _update_display(self):
        """Fetch framebuffer and scale to fit canvas."""
        fb = _iql.get_framebuffer()
        if not fb:
            return

        w, h = _iql.get_screen_size()
        img = Image.frombytes('RGBA', (w, h), fb)

        # Scale to fit canvas while maintaining 2:1 aspect ratio
        cw = self.emu_canvas.winfo_width()
        ch = self.emu_canvas.winfo_height()
        if cw < 2 or ch < 2:
            return

        scale_x = cw / w
        scale_y = ch / h
        scale = min(scale_x, scale_y)
        disp_w = max(1, int(w * scale))
        disp_h = max(1, int(h * scale))

        img = img.resize((disp_w, disp_h), Image.NEAREST)
        self._photo = ImageTk.PhotoImage(img)
        self.emu_canvas.delete("all")
        # Center the image in the canvas
        x_off = (cw - disp_w) // 2
        y_off = (ch - disp_h) // 2
        self.emu_canvas.create_image(x_off, y_off, anchor=tk.NW,
                                     image=self._photo)

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
        self._emu_paused = False
        self._tick_count = 0
        self.btn_build_run.config(text="Rebuild & Run")
        self.btn_pause.config(state=tk.NORMAL, text="Pause")
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
            self._emu_paused = False
            self.btn_pause.config(state=tk.DISABLED, text="Pause")
            self.btn_restart.config(state=tk.DISABLED)
            self.status_var.set("Stopped")

    def _toggle_pause(self):
        """Toggle pause/resume."""
        if not self._emu_active:
            return

        if self._emu_paused:
            _iql.resume()
            self._emu_paused = False
            self.btn_pause.config(text="Pause")
            self.status_var.set("Running")
            self.emu_canvas.focus_set()
        else:
            _iql.pause()
            self._emu_paused = True
            self.btn_pause.config(text="Resume")
            self.status_var.set("Paused")
            self._update_debug_panel()

    def _emu_restart(self):
        """Restart the emulator with confirmation."""
        if not self._emu_active:
            return

        if not messagebox.askyesno("Restart", "Restart the emulator?"):
            self.emu_canvas.focus_set()
            return

        if self._emu_paused:
            _iql.resume()
            self._emu_paused = False
            self.btn_pause.config(text="Pause")

        _iql.restart()
        self.status_var.set("Restarting...")
        self.emu_canvas.focus_set()

    def _on_speed_change(self, event=None):
        """Handle speed combobox change."""
        speed = 0 if self.speed_var.get() == "Normal" else 4
        if self._emu_active:
            _iql.set_speed(speed)

    def _emu_tick(self):
        """Advance emulation and update display."""
        if not self._emu_active:
            return

        # Advance emulation (skipped internally if paused)
        _iql.tick()

        # Update display
        self._update_display()

        # Update debug panel periodically
        self._tick_count += 1
        if self._emu_paused or self._tick_count % DEBUG_UPDATE_INTERVAL == 0:
            self._update_debug_panel()

        # Schedule next tick
        self._after_id = self.root.after(EMU_TICK_MS, self._emu_tick)

    def _update_debug_panel(self):
        """Fetch CPU state and update the debug text widget."""
        if not self._emu_active:
            return

        state = _iql.get_cpu_state()
        if state is None:
            return

        sr = state.get("SR", 0)
        pc = state.get("PC", 0)
        flags = _format_sr_flags(sr)

        lines = []
        lines.append(f"PC: ${pc:06X}   SR: ${sr:04X}")
        lines.append(f"Flags: {flags}")
        lines.append("")

        # Data registers in two columns
        for i in range(4):
            d_lo = state.get(f"D{i}", 0)
            d_hi = state.get(f"D{i+4}", 0)
            lines.append(f"D{i}: {d_lo:08X}  D{i+4}: {d_hi:08X}")
        lines.append("")

        # Address registers in two columns
        for i in range(4):
            a_lo = state.get(f"A{i}", 0)
            a_hi = state.get(f"A{i+4}", 0)
            lines.append(f"A{i}: {a_lo:08X}  A{i+4}: {a_hi:08X}")
        lines.append("")

        usp = state.get("USP", 0)
        ssp = state.get("SSP", 0)
        lines.append(f"USP: {usp:08X}")
        lines.append(f"SSP: {ssp:08X}")
        lines.append("")

        # Keyboard state
        key = state.get("key_down", 0)
        shift = state.get("shift", 0)
        ctrl = state.get("control", 0)
        alt = state.get("alt", 0)
        lines.append("--- Keyboard ---")
        lines.append(f"Key: {key}  Sh: {shift}  Ct: {ctrl}  Al: {alt}")

        text = "\n".join(lines)

        self.debug_text.config(state=tk.NORMAL)
        self.debug_text.delete("1.0", tk.END)
        self.debug_text.insert("1.0", text)

        # Highlight PC and flags lines
        self.debug_text.tag_add("highlight", "1.0", "1.end")
        self.debug_text.tag_add("dim", "2.0", "2.end")

        self.debug_text.config(state=tk.DISABLED)

    def _resolve_key(self, event):
        """Resolve Tkinter event to (vk, shift) matching iQL's processEvent."""
        shift = 1 if event.state & 0x0001 else 0

        # 1. Special keys (no printable char) — lookup by keysym
        entry = _KEYSYM_TO_VK.get(event.keysym)
        if entry is not None:
            vk, force_shift = entry
            if force_shift >= 0:
                shift = force_shift
            return vk, shift

        # 2. Printable chars
        ch = event.char
        if not ch:
            return None

        # Check shift fixes first
        fix = _SHIFT_FIXES.get(ch)
        if fix is not None:
            return fix, 1

        # Uppercase → base letter + shift
        if ch.isupper():
            vk = _CHAR_TO_VK.get(ch)
            if vk is not None:
                return vk, 1

        # Regular character lookup
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
        self.root.bind("<KeyPress>", None)
        self.root.bind("<KeyRelease>", None)

    def cleanup(self):
        """Clean up on application exit."""
        self._emu_stop()
