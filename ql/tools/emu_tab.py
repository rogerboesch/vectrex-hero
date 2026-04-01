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

        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(
            side=tk.LEFT, fill=tk.Y, padx=5, pady=2)

        self.soft_bp_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(toolbar, text="Breakpoints", variable=self.soft_bp_var,
                        command=self._toggle_soft_bp).pack(side=tk.LEFT, padx=2)

        self.trap_log_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(toolbar, text="Trap Log", variable=self.trap_log_var,
                        command=self._toggle_trap_log).pack(side=tk.LEFT, padx=2)

        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = ttk.Label(toolbar, textvariable=self.status_var)
        status_bar.pack(side=tk.RIGHT, padx=5)

        # Vertical paned window: top = screen+debug, bottom = console
        paned = ttk.PanedWindow(self.parent, orient=tk.VERTICAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Top pane: screen canvas + debug panel
        top_frame = ttk.Frame(paned)
        paned.add(top_frame, weight=1)

        # Debug panel (right side, fixed width, packed first)
        debug_frame = ttk.Frame(top_frame, width=DEBUG_PANEL_WIDTH)
        debug_frame.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))
        debug_frame.pack_propagate(False)

        # Debug notebook: CPU | Memory | Breakpoints
        self.debug_nb = ttk.Notebook(debug_frame)
        self.debug_nb.pack(fill=tk.BOTH, expand=True)

        # --- Tab: CPU State ---
        cpu_frame = ttk.Frame(self.debug_nb)
        self.debug_nb.add(cpu_frame, text="CPU")

        self.debug_text = tk.Text(cpu_frame, width=48, height=20,
                                  font=("Courier", 11), state=tk.DISABLED,
                                  bg="#1e1e1e", fg="#d4d4d4",
                                  relief=tk.FLAT, padx=6, pady=6,
                                  wrap=tk.NONE)
        self.debug_text.pack(fill=tk.BOTH, expand=True)

        # Step controls under CPU tab
        step_frame = ttk.Frame(cpu_frame)
        step_frame.pack(fill=tk.X, padx=4, pady=4)
        self.btn_step1 = ttk.Button(step_frame, text="Step 1", width=7,
                                     command=lambda: self._step(1))
        self.btn_step1.pack(side=tk.LEFT, padx=1)
        self.btn_step10 = ttk.Button(step_frame, text="Step 10", width=7,
                                      command=lambda: self._step(10))
        self.btn_step10.pack(side=tk.LEFT, padx=1)
        self.btn_step100 = ttk.Button(step_frame, text="Step 100", width=7,
                                       command=lambda: self._step(100))
        self.btn_step100.pack(side=tk.LEFT, padx=1)

        # --- Tab: Memory Viewer ---
        mem_frame = ttk.Frame(self.debug_nb)
        self.debug_nb.add(mem_frame, text="Memory")

        mem_top = ttk.Frame(mem_frame)
        mem_top.pack(fill=tk.X, padx=4, pady=4)
        ttk.Label(mem_top, text="Addr:").pack(side=tk.LEFT)
        self.mem_addr_var = tk.StringVar(value="20000")
        mem_entry = ttk.Entry(mem_top, textvariable=self.mem_addr_var, width=8)
        mem_entry.pack(side=tk.LEFT, padx=2)
        mem_entry.bind("<Return>", lambda e: self._update_mem_view())
        ttk.Button(mem_top, text="Go", command=self._update_mem_view).pack(
            side=tk.LEFT, padx=2)
        ttk.Label(mem_top, text="Rows:").pack(side=tk.LEFT, padx=(8, 0))
        self.mem_rows_var = tk.IntVar(value=16)
        ttk.Spinbox(mem_top, from_=4, to=64, textvariable=self.mem_rows_var,
                     width=4).pack(side=tk.LEFT, padx=2)

        self.mem_text = tk.Text(mem_frame, width=48, height=20,
                                font=("Courier", 10), state=tk.DISABLED,
                                bg="#1e1e1e", fg="#d4d4d4",
                                relief=tk.FLAT, padx=6, pady=4,
                                wrap=tk.NONE)
        self.mem_text.pack(fill=tk.BOTH, expand=True)
        self.mem_text.tag_configure("addr", foreground="#569cd6")
        self.mem_text.tag_configure("nonzero", foreground="#ce9178")

        # --- Tab: Breakpoints ---
        bp_frame = ttk.Frame(self.debug_nb)
        self.debug_nb.add(bp_frame, text="Break")

        bp_top = ttk.Frame(bp_frame)
        bp_top.pack(fill=tk.X, padx=4, pady=4)
        ttk.Label(bp_top, text="Addr:").pack(side=tk.LEFT)
        self.bp_addr_var = tk.StringVar()
        bp_entry = ttk.Entry(bp_top, textvariable=self.bp_addr_var, width=8)
        bp_entry.pack(side=tk.LEFT, padx=2)
        bp_entry.bind("<Return>", lambda e: self._add_breakpoint())
        ttk.Button(bp_top, text="Add", command=self._add_breakpoint).pack(
            side=tk.LEFT, padx=2)
        ttk.Button(bp_top, text="Clear All", command=self._clear_breakpoints).pack(
            side=tk.RIGHT, padx=2)
        ttk.Button(bp_top, text="Remove", command=self._remove_breakpoint).pack(
            side=tk.RIGHT, padx=2)

        self.bp_listbox = tk.Listbox(bp_frame, height=8, font=("Courier", 11),
                                      bg="#1e1e1e", fg="#d4d4d4",
                                      selectbackground="#264f78")
        self.bp_listbox.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        # Breakpoint hit status
        self.bp_status_var = tk.StringVar(value="")
        ttk.Label(bp_frame, textvariable=self.bp_status_var,
                  foreground="red").pack(padx=4, pady=2)

        # --- Tab: Watch ---
        watch_frame = ttk.Frame(self.debug_nb)
        self.debug_nb.add(watch_frame, text="Watch")

        watch_top = ttk.Frame(watch_frame)
        watch_top.pack(fill=tk.X, padx=4, pady=4)
        ttk.Label(watch_top, text="Addr:").pack(side=tk.LEFT)
        self.watch_addr_var = tk.StringVar()
        w_entry = ttk.Entry(watch_top, textvariable=self.watch_addr_var, width=8)
        w_entry.pack(side=tk.LEFT, padx=2)
        ttk.Label(watch_top, text="Name:").pack(side=tk.LEFT, padx=(4, 0))
        self.watch_name_var = tk.StringVar()
        ttk.Entry(watch_top, textvariable=self.watch_name_var, width=10).pack(
            side=tk.LEFT, padx=2)

        watch_type_frame = ttk.Frame(watch_frame)
        watch_type_frame.pack(fill=tk.X, padx=4)
        self.watch_type_var = tk.StringVar(value="byte")
        for t in ("byte", "word", "long"):
            ttk.Radiobutton(watch_type_frame, text=t, value=t,
                           variable=self.watch_type_var).pack(side=tk.LEFT)
        ttk.Button(watch_type_frame, text="Add", command=self._add_watch).pack(
            side=tk.LEFT, padx=8)
        ttk.Button(watch_type_frame, text="Remove", command=self._remove_watch).pack(
            side=tk.LEFT, padx=2)

        self.watch_list = []  # list of (addr, name, type)
        self.watch_text = tk.Text(watch_frame, width=48, height=16,
                                   font=("Courier", 11), state=tk.DISABLED,
                                   bg="#1e1e1e", fg="#d4d4d4",
                                   relief=tk.FLAT, padx=6, pady=4,
                                   wrap=tk.NONE)
        self.watch_text.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        self.watch_text.tag_configure("name", foreground="#4ec9b0")
        self.watch_text.tag_configure("val", foreground="#ce9178")

        # Emulator screen canvas (fills remaining space)
        screen_frame = ttk.Frame(top_frame)
        screen_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.emu_canvas = tk.Canvas(screen_frame, bg="#000000",
                                    highlightthickness=0)
        self.emu_canvas.pack(fill=tk.BOTH, expand=True)

        self.emu_canvas.config(takefocus=True)
        self.emu_canvas.bind("<Button-1>", lambda e: self.emu_canvas.focus_set())
        self.emu_canvas.bind("<Configure>", self._on_canvas_resize)
        self.emu_canvas.bind("<Motion>", self._on_canvas_motion)
        self.emu_canvas.bind("<Leave>", self._on_canvas_leave)

        # Pixel inspector bar under screen
        self.pixel_info_var = tk.StringVar(value="")
        pixel_bar = ttk.Label(screen_frame, textvariable=self.pixel_info_var,
                              font=("Courier", 10), anchor=tk.W)
        pixel_bar.pack(fill=tk.X, padx=2)

        # Track display offset/scale for pixel mapping
        self._disp_ox = 0
        self._disp_oy = 0
        self._disp_scale = 1.0

        # Bottom pane: console log
        console_frame = ttk.LabelFrame(paned, text="Console")
        paned.add(console_frame, weight=0)

        console_scroll = ttk.Scrollbar(console_frame)
        console_scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.console_text = tk.Text(console_frame, height=CONSOLE_HEIGHT,
                                    font=("Courier", 10), state=tk.DISABLED,
                                    bg="#1a1a1a", fg="#cccccc",
                                    relief=tk.FLAT, padx=6, pady=4,
                                    wrap=tk.NONE,
                                    yscrollcommand=console_scroll.set)
        self.console_text.pack(fill=tk.BOTH, expand=True)
        console_scroll.config(command=self.console_text.yview)

        self.console_text.tag_configure("err", foreground="#f44747")
        self.console_text.tag_configure("dbg", foreground="#808080")

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
        self._disp_ox = x_off
        self._disp_oy = y_off
        self._disp_scale = scale
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
            self._console_append(f"--- Build FAILED ---\n", "err")
            self._console_append(result.stderr[-1000:], "err")
            return

        # Show build output in console
        self._console_append(f"--- Build OK ---\n")
        if result.stdout.strip():
            self._console_append(result.stdout[-1000:])

        self.status_var.set("Starting emulator...")
        self.root.update_idletasks()

        # Stop any running emulator
        self._emu_stop()

        # Start emulator
        _iql.init(IQL_SYSTEM_PATH)

        # Enable software breakpoints after QDOS finishes booting
        # (delayed to avoid false triggers from uninitialized RAM)
        self.root.after(3000, self._enable_soft_bp)

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

        # Check for breakpoint hits
        self._check_breakpoint_hit()

        # Update debug panel and console periodically
        self._tick_count += 1
        if self._emu_paused or self._tick_count % DEBUG_UPDATE_INTERVAL == 0:
            self._update_debug_panel()
            self._update_watch()
            self._drain_console()
            self._drain_trap_log()

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

    def _console_append(self, text, tag=None):
        """Append text to the console widget."""
        self.console_text.config(state=tk.NORMAL)
        self.console_text.insert(tk.END, text, tag)
        self.console_text.see(tk.END)
        self.console_text.config(state=tk.DISABLED)

    def _drain_console(self):
        """Drain log buffer from emulator and append to console."""
        log = _iql.get_log()
        if log is None:
            return

        self.console_text.config(state=tk.NORMAL)

        for line in log.splitlines(keepends=True):
            tag = None
            if "] ERR " in line:
                tag = "err"
            elif "] DBG " in line:
                tag = "dbg"
            self.console_text.insert(tk.END, line, tag)

        # Trim if too many lines
        line_count = int(self.console_text.index("end-1c").split(".")[0])
        if line_count > CONSOLE_MAX_LINES:
            self.console_text.delete("1.0", f"{line_count - CONSOLE_MAX_LINES}.0")

        self.console_text.see(tk.END)
        self.console_text.config(state=tk.DISABLED)

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

    # --- Step controls ---

    def _step(self, count):
        """Single-step N instructions (pause first if running)."""
        if not self._emu_active:
            return
        if not self._emu_paused:
            self._toggle_pause()
        _iql.step(count)
        self._update_display()
        self._update_debug_panel()
        self._update_mem_view()
        self._update_watch()

    # --- Screenshot ---

    def _screenshot(self):
        """Save screenshot as PNG."""
        if not self._emu_active:
            return
        from tkinter import filedialog
        path = filedialog.asksaveasfilename(
            defaultextension=".png",
            filetypes=[("PNG", "*.png")],
            initialfile="screenshot.png"
        )
        if not path:
            return
        fb = _iql.get_framebuffer()
        if not fb:
            return
        w, h = _iql.get_screen_size()
        img = Image.frombytes('RGBA', (w, h), fb)
        img.save(path)
        self._console_append(f"Screenshot saved: {path}\n")

    # --- Memory Viewer ---

    def _update_mem_view(self):
        """Refresh the memory hex dump."""
        if not self._emu_active:
            return
        try:
            addr = int(self.mem_addr_var.get(), 16)
        except ValueError:
            return
        rows = self.mem_rows_var.get()

        self.mem_text.config(state=tk.NORMAL)
        self.mem_text.delete("1.0", tk.END)

        for r in range(rows):
            row_addr = addr + r * 16
            self.mem_text.insert(tk.END, f"{row_addr:06X}: ", "addr")
            data = _iql.read_mem(row_addr, 16)
            hex_parts = []
            ascii_parts = []
            for b in data:
                tag = "nonzero" if b != 0 else None
                hex_str = f"{b:02X} "
                self.mem_text.insert(tk.END, hex_str, tag)
                ascii_parts.append(chr(b) if 32 <= b < 127 else '.')
            self.mem_text.insert(tk.END, " " + "".join(ascii_parts) + "\n")

        self.mem_text.config(state=tk.DISABLED)

    # --- Breakpoints ---

    def _add_breakpoint(self):
        """Add a breakpoint at the entered address."""
        if not self._emu_active:
            return
        try:
            addr = int(self.bp_addr_var.get(), 16)
        except ValueError:
            return
        _iql.add_breakpoint(addr)
        self.bp_addr_var.set("")
        self._refresh_bp_list()

    def _remove_breakpoint(self):
        """Remove the selected breakpoint."""
        sel = self.bp_listbox.curselection()
        if not sel:
            return
        text = self.bp_listbox.get(sel[0])
        try:
            addr = int(text.strip().split(":")[0].strip("$"), 16)
            _iql.remove_breakpoint(addr)
        except (ValueError, IndexError):
            pass
        self._refresh_bp_list()

    def _clear_breakpoints(self):
        """Clear all breakpoints."""
        _iql.clear_breakpoints()
        self.bp_status_var.set("")
        self._refresh_bp_list()

    def _refresh_bp_list(self):
        """Refresh the breakpoint listbox."""
        self.bp_listbox.delete(0, tk.END)
        if not self._emu_active:
            return
        for addr in _iql.list_breakpoints():
            self.bp_listbox.insert(tk.END, f"  ${addr:06X}")

    def _check_breakpoint_hit(self):
        """Check if a breakpoint was hit during the last tick."""
        if not self._emu_active:
            return
        hit = _iql.check_breakpoint()
        if hit is not None:
            addr = hit.get("addr", 0) if isinstance(hit, dict) else hit
            soft_id = hit.get("soft_id", 0) if isinstance(hit, dict) else 0
            self._emu_paused = True
            self.btn_pause.config(text="Resume")
            if soft_id > 0:
                msg = f"SOFTWARE BP #{soft_id} at ${addr:06X}"
            else:
                msg = f"BREAKPOINT at ${addr:06X}"
            self.status_var.set(msg)
            self.bp_status_var.set(msg)
            self._console_append(f"*** {msg} ***\n", "err")
            self._update_debug_panel()
            self._update_mem_view()
            self._update_watch()

    # --- Watch ---

    def _add_watch(self):
        """Add a memory watch entry."""
        try:
            addr = int(self.watch_addr_var.get(), 16)
        except ValueError:
            return
        name = self.watch_name_var.get().strip() or f"${addr:06X}"
        wtype = self.watch_type_var.get()
        self.watch_list.append((addr, name, wtype))
        self.watch_addr_var.set("")
        self.watch_name_var.set("")
        self._update_watch()

    def _remove_watch(self):
        """Remove last watch entry."""
        if self.watch_list:
            self.watch_list.pop()
            self._update_watch()

    def _update_watch(self):
        """Refresh watch values from emulator memory."""
        self.watch_text.config(state=tk.NORMAL)
        self.watch_text.delete("1.0", tk.END)
        if not self._emu_active:
            self.watch_text.config(state=tk.DISABLED)
            return
        for addr, name, wtype in self.watch_list:
            if wtype == "byte":
                val = _iql.read_byte(addr)
                val_str = f"${val:02X} ({val})"
            elif wtype == "word":
                val = _iql.read_word(addr)
                val_str = f"${val:04X} ({val})"
            else:
                val = _iql.read_long(addr)
                val_str = f"${val:08X}"
            self.watch_text.insert(tk.END, f"{name}: ", "name")
            self.watch_text.insert(tk.END, f"{val_str}\n", "val")
        self.watch_text.config(state=tk.DISABLED)

    # --- Frame step ---


    def _toggle_soft_bp(self):
        """Toggle software breakpoint checking."""
        if not self._emu_active:
            return
        enabled = self.soft_bp_var.get()
        _iql.set_soft_bp(1 if enabled else 0)
        state = "ON" if enabled else "OFF"
        self._console_append(f"--- Software breakpoints {state} ---\n")

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

    # --- Pixel inspector ---

    _COLOR_NAMES = ["black", "red", "blue", "magenta", "green", "cyan", "yellow", "white"]

    def _on_canvas_motion(self, event):
        """Show pixel info on hover over emulator display."""
        if not self._emu_active:
            return
        # Map canvas coords to QL screen coords
        qx = int((event.x - self._disp_ox) / self._disp_scale)
        qy = int((event.y - self._disp_oy) / self._disp_scale)

        # iQL screen is 512x256 (Mode 4 width), QL Mode 8 is 256x256
        # The pixel buffer is 512 wide but Mode 8 pixels are 2px wide
        ql_x = qx // 2  # Mode 8 pixel x (0-255)
        ql_y = qy        # pixel y (0-255)

        if ql_x < 0 or ql_x >= 256 or ql_y < 0 or ql_y >= 256:
            self.pixel_info_var.set("")
            return

        # Read the Mode 8 byte pair
        pair_addr = 0x20000 + ql_y * 128 + (ql_x // 4) * 2
        pos = ql_x & 3
        even = _iql.read_byte(pair_addr)
        odd = _iql.read_byte(pair_addr + 1)

        # Extract color for this pixel position
        shift = (3 - pos) * 2
        g = (even >> (shift + 1)) & 1
        r = (odd >> shift) & 1
        b = (odd >> (shift + 1)) & 1
        color = (g << 2) | (b << 1) | r
        cname = self._COLOR_NAMES[color] if color < 8 else "?"

        self.pixel_info_var.set(
            f"X:{ql_x} Y:{ql_y}  Color:{color} ({cname})  "
            f"Addr:${pair_addr:05X}  Even:${even:02X} Odd:${odd:02X}"
        )

    def _on_canvas_leave(self, event):
        self.pixel_info_var.set("")

    def _enable_soft_bp(self):
        """Enable software breakpoints after QDOS boot delay."""
        if self._emu_active and self.soft_bp_var.get():
            _iql.set_soft_bp(1)
            self._console_append("Software breakpoints enabled\n")

    def cleanup(self):
        """Clean up on application exit."""
        self._emu_stop()
