#!/usr/bin/env python3
"""
QL Studio — Design sprites for Sinclair QL Mode 8

Features:
  - 8 colors (QL Mode 8 palette)
  - Nibble-packed format (2 pixels per byte)
  - Multiple sprites with names and sizes
  - Export to sprites.c / sprites.h
  - Save/load project as JSON
  - Horizontal flip preview
  - Embedded iQL emulator for build & test

Usage: python3 qlstudio.py [project.json]
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import json
import os

# QL Mode 8 colors (RGB approximations for display)
QL_COLORS = {
    0: ("#000000", "transparent"),
    1: ("#FF0000", "red"),
    2: ("#0000FF", "blue"),
    3: ("#FF00FF", "magenta"),
    4: ("#00FF00", "green"),
    5: ("#00FFFF", "cyan"),
    6: ("#FFFF00", "yellow"),
    7: ("#FFFFFF", "white"),
}

CELL_SIZE = 20
MAX_W = 32
MAX_H = 32


class Sprite:
    def __init__(self, name="sprite", width=10, height=20):
        self.name = name
        self.width = width  # must be even
        self.height = height
        # Pixel data: list of rows, each row is list of color indices (0-7)
        self.pixels = [[0] * width for _ in range(height)]

    def to_dict(self):
        return {
            "name": self.name,
            "width": self.width,
            "height": self.height,
            "pixels": self.pixels,
        }

    @staticmethod
    def from_dict(d):
        s = Sprite(d["name"], d["width"], d["height"])
        s.pixels = d["pixels"]
        return s

    def resize(self, new_w, new_h):
        if new_w & 1:
            new_w += 1
        old = self.pixels
        self.pixels = [[0] * new_w for _ in range(new_h)]
        for y in range(min(self.height, new_h)):
            for x in range(min(self.width, new_w)):
                self.pixels[y][x] = old[y][x]
        self.width = new_w
        self.height = new_h

    def flip_h(self):
        for y in range(self.height):
            self.pixels[y] = self.pixels[y][::-1]

    def flip_v(self):
        self.pixels = self.pixels[::-1]

    def move_up(self):
        self.pixels = self.pixels[1:] + [self.pixels[0]]

    def move_down(self):
        self.pixels = [self.pixels[-1]] + self.pixels[:-1]

    def move_left(self):
        for y in range(self.height):
            self.pixels[y] = self.pixels[y][1:] + [self.pixels[y][0]]

    def move_right(self):
        for y in range(self.height):
            self.pixels[y] = [self.pixels[y][-1]] + self.pixels[y][:-1]

    def to_c_bytes(self):
        """Convert to nibble-packed byte array (high nibble = left pixel)."""
        data = []
        for y in range(self.height):
            row_bytes = []
            for x in range(0, self.width, 2):
                hi = self.pixels[y][x] & 0x0F
                lo = self.pixels[y][x + 1] & 0x0F if x + 1 < self.width else 0
                row_bytes.append((hi << 4) | lo)
            data.append(row_bytes)
        return data


class SpriteEditor:
    def __init__(self, root, project_file=None):
        self.root = root
        self.root.title("QL Studio")
        self.sprites = []
        self.current_idx = 0
        self.current_color = 7  # white
        self.drawing = False
        self.project_file = project_file
        self._ql_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

        self._build_ui()

        if project_file and os.path.exists(project_file):
            self._load_project(project_file)
        else:
            self.sprites.append(Sprite("player_r", 10, 20))
            self._update_sprite_list()

        self._select_sprite(0)

    def _build_ui(self):
        # Menu bar
        menubar = tk.Menu(self.root)
        filemenu = tk.Menu(menubar, tearoff=0)
        filemenu.add_command(label="New Project", command=self._new_project, accelerator="Cmd+N")
        filemenu.add_command(label="Open Project...", command=self._open_project, accelerator="Cmd+O")
        filemenu.add_command(label="Save Project", command=self._save_project, accelerator="Cmd+S")
        filemenu.add_command(label="Save Project As...", command=self._save_project_as, accelerator="Cmd+Shift+S")
        filemenu.add_separator()
        filemenu.add_command(label="Export C files...", command=self._export_c, accelerator="Cmd+E")
        filemenu.add_separator()
        filemenu.add_command(label="Screenshot...", command=self._take_screenshot, accelerator="Cmd+P")
        filemenu.add_separator()
        filemenu.add_command(label="Quit", command=self.root.quit, accelerator="Cmd+Q")
        menubar.add_cascade(label="File", menu=filemenu)

        editmenu = tk.Menu(menubar, tearoff=0)
        editmenu.add_command(label="Copy Sprite", command=self._copy_sprite, accelerator="Cmd+C")
        editmenu.add_command(label="Paste Sprite", command=self._paste_sprite, accelerator="Cmd+V")
        editmenu.add_separator()
        editmenu.add_command(label="Flip Horizontal", command=self._flip_h, accelerator="Cmd+H")
        editmenu.add_command(label="Flip Vertical", command=self._flip_v, accelerator="Cmd+J")
        editmenu.add_separator()
        editmenu.add_command(label="Move Up", command=self._move_up, accelerator="Cmd+Up")
        editmenu.add_command(label="Move Down", command=self._move_down, accelerator="Cmd+Down")
        editmenu.add_command(label="Move Left", command=self._move_left, accelerator="Cmd+Left")
        editmenu.add_command(label="Move Right", command=self._move_right, accelerator="Cmd+Right")
        editmenu.add_separator()
        editmenu.add_command(label="Clear Sprite", command=self._clear_sprite, accelerator="Cmd+Delete")
        menubar.add_cascade(label="Edit", menu=editmenu)

        # Keyboard shortcuts — File
        self.root.bind("<Command-n>", lambda e: self._new_project())
        self.root.bind("<Command-o>", lambda e: self._open_project())
        self.root.bind("<Command-s>", lambda e: self._save_project())
        self.root.bind("<Command-Shift-S>", lambda e: self._save_project_as())
        self.root.bind("<Command-e>", lambda e: self._export_c())
        self.root.bind("<Command-p>", lambda e: self._take_screenshot())
        self.root.bind("<Command-q>", lambda e: self.root.quit())

        # Keyboard shortcuts — Edit
        self.root.bind("<Command-c>", lambda e: self._copy_sprite())
        self.root.bind("<Command-v>", lambda e: self._paste_sprite())
        self.root.bind("<Command-h>", lambda e: self._flip_h())
        self.root.bind("<Command-j>", lambda e: self._flip_v())
        self.root.bind("<Command-Up>", lambda e: self._move_up())
        self.root.bind("<Command-Down>", lambda e: self._move_down())
        self.root.bind("<Command-Left>", lambda e: self._move_left())
        self.root.bind("<Command-Right>", lambda e: self._move_right())
        self.root.bind("<Command-Delete>", lambda e: self._clear_sprite())

        self.root.config(menu=menubar)

        # Notebook for Sprites + Emulator tabs
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)

        # Tab 0: Sprites
        sprite_tab = ttk.Frame(self.notebook)
        self.notebook.add(sprite_tab, text="Sprites")

        # Tab 1: Images
        img_frame = ttk.Frame(self.notebook)
        self.notebook.add(img_frame, text="Images")

        from image_editor import ImageEditorTab
        self.img_tab = ImageEditorTab(img_frame, self)

        # Tab 2: Emulator
        emu_frame = ttk.Frame(self.notebook)
        self.notebook.add(emu_frame, text="Emulator")

        from emu_tab import EmulatorTab
        self.emu_tab = EmulatorTab(emu_frame, self)

        # Main layout: left panel (sprite list), center (canvas), right (palette + info)
        main = ttk.Frame(sprite_tab)
        main.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Left: sprite list
        left = ttk.LabelFrame(main, text="Sprites")
        left.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 5))

        self.sprite_listbox = tk.Listbox(left, width=18, height=20)
        self.sprite_listbox.pack(fill=tk.Y, expand=True, padx=3, pady=3)
        self.sprite_listbox.bind("<<ListboxSelect>>", self._on_list_select)

        btn_frame = ttk.Frame(left)
        btn_frame.pack(fill=tk.X, padx=3, pady=3)
        ttk.Button(btn_frame, text="+", width=3, command=self._add_sprite).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="-", width=3, command=self._del_sprite).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="Dup", width=4, command=self._dup_sprite).pack(side=tk.LEFT)

        # Sprite properties
        prop_frame = ttk.LabelFrame(left, text="Properties")
        prop_frame.pack(fill=tk.X, padx=3, pady=3)

        ttk.Label(prop_frame, text="Name:").grid(row=0, column=0, sticky=tk.W)
        self.name_var = tk.StringVar()
        self.name_entry = ttk.Entry(prop_frame, textvariable=self.name_var, width=14)
        self.name_entry.grid(row=0, column=1)
        self.name_entry.bind("<Return>", self._rename_sprite)

        ttk.Label(prop_frame, text="Width:").grid(row=1, column=0, sticky=tk.W)
        self.width_var = tk.IntVar(value=10)
        ttk.Spinbox(prop_frame, from_=2, to=MAX_W, increment=2,
                     textvariable=self.width_var, width=5,
                     command=self._resize_sprite).grid(row=1, column=1, sticky=tk.W)

        ttk.Label(prop_frame, text="Height:").grid(row=2, column=0, sticky=tk.W)
        self.height_var = tk.IntVar(value=20)
        ttk.Spinbox(prop_frame, from_=1, to=MAX_H, increment=1,
                     textvariable=self.height_var, width=5,
                     command=self._resize_sprite).grid(row=2, column=1, sticky=tk.W)

        # Center: drawing canvas
        center = ttk.Frame(main)
        center.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(center, bg="#222222", cursor="crosshair")
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Button-1>", self._on_click)
        self.canvas.bind("<B1-Motion>", self._on_drag)
        self.canvas.bind("<Button-3>", self._on_right_click)
        self.canvas.bind("<B3-Motion>", self._on_right_click)
        self.canvas.bind("<Configure>", lambda e: self._redraw())

        # Right: palette + preview
        right = ttk.Frame(main)
        right.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))

        # Palette — use Canvas swatches (tk.Button ignores bg on macOS)
        pal_frame = ttk.LabelFrame(right, text="Palette")
        pal_frame.pack(fill=tk.X, pady=(0, 5))

        self.pal_buttons = []
        for i in range(8):
            color, name = QL_COLORS[i]
            f = tk.Frame(pal_frame, bd=2, relief=tk.RAISED)
            f.pack(fill=tk.X, padx=2, pady=1)
            swatch = tk.Canvas(f, width=24, height=18, highlightthickness=0)
            swatch.pack(side=tk.LEFT, padx=2)
            swatch.create_rectangle(0, 0, 25, 19, fill=color, outline="#888888")
            swatch.bind("<Button-1>", lambda e, c=i: self._set_color(c))
            lbl = tk.Label(f, text=f"{i}: {name}", anchor=tk.W)
            lbl.pack(side=tk.LEFT, fill=tk.X)
            lbl.bind("<Button-1>", lambda e, c=i: self._set_color(c))
            self.pal_buttons.append(f)

        # Current color indicator
        self.color_label = ttk.Label(right, text="Color: 7 (white)")
        self.color_label.pack(pady=5)

        # Preview (actual size)
        preview_frame = ttk.LabelFrame(right, text="Preview (2x)")
        preview_frame.pack(fill=tk.X, pady=5)
        self.preview = tk.Canvas(preview_frame, width=80, height=80, bg="#000000")
        self.preview.pack(padx=5, pady=5)

        preview_ctrl = ttk.Frame(preview_frame)
        preview_ctrl.pack(fill=tk.X, padx=5, pady=(0, 5))
        self.anim_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(preview_ctrl, text="Animate", variable=self.anim_var,
                        command=self._toggle_anim).pack(side=tk.LEFT)
        self._anim_frame = 0
        self._anim_after_id = None

        # Byte size info
        self.size_label = ttk.Label(right, text="Bytes: 0")
        self.size_label.pack(pady=5)

        # Keyboard shortcuts
        self.root.bind("<Key>", self._on_key)

    def _cur(self):
        if 0 <= self.current_idx < len(self.sprites):
            return self.sprites[self.current_idx]
        return None

    def _set_color(self, c):
        self.current_color = c
        color, name = QL_COLORS[c]
        self.color_label.config(text=f"Color: {c} ({name})")
        # Highlight selected palette
        for i, f in enumerate(self.pal_buttons):
            f.config(relief=tk.SUNKEN if i == c else tk.RAISED)

    def _on_key(self, event):
        if isinstance(event.widget, (tk.Entry, ttk.Entry)):
            return
        if event.char in "01234567":
            self._set_color(int(event.char))

    def _on_tab_changed(self, event=None):
        idx = self.notebook.index(self.notebook.select())
        # Deselect emulator when leaving its tab
        if hasattr(self, '_prev_tab') and self._prev_tab == 2:
            self.emu_tab.on_tab_deselected()
        self._prev_tab = idx

        if idx == 0:
            # Sprites tab — color keys
            self.root.bind("<Key>", self._on_key)
        elif idx == 1:
            # Images tab — color keys via image editor
            self.root.bind("<Key>", self.img_tab.on_key)
        elif idx == 2:
            # Emulator tab
            self.emu_tab.on_tab_selected()

    def _update_sprite_list(self):
        self.sprite_listbox.delete(0, tk.END)
        for s in self.sprites:
            self.sprite_listbox.insert(tk.END, f"{s.name} ({s.width}x{s.height})")

    def _select_sprite(self, idx):
        if idx < 0 or idx >= len(self.sprites):
            return
        self.current_idx = idx
        self.sprite_listbox.selection_clear(0, tk.END)
        self.sprite_listbox.selection_set(idx)
        s = self._cur()
        self.name_var.set(s.name)
        self.width_var.set(s.width)
        self.height_var.set(s.height)
        self._redraw()

    def _on_list_select(self, event):
        sel = self.sprite_listbox.curselection()
        if sel:
            self._select_sprite(sel[0])

    def _add_sprite(self):
        name = f"sprite_{len(self.sprites)}"
        self.sprites.append(Sprite(name, 10, 20))
        self._update_sprite_list()
        self._select_sprite(len(self.sprites) - 1)

    def _del_sprite(self):
        if len(self.sprites) <= 1:
            return
        del self.sprites[self.current_idx]
        self._update_sprite_list()
        self._select_sprite(min(self.current_idx, len(self.sprites) - 1))

    def _dup_sprite(self):
        s = self._cur()
        if not s:
            return
        d = Sprite(s.name + "_copy", s.width, s.height)
        d.pixels = [row[:] for row in s.pixels]
        self.sprites.append(d)
        self._update_sprite_list()
        self._select_sprite(len(self.sprites) - 1)

    def _rename_sprite(self, event=None):
        s = self._cur()
        if s:
            s.name = self.name_var.get().strip().replace(" ", "_")
            self._update_sprite_list()

    def _resize_sprite(self):
        s = self._cur()
        if not s:
            return
        w = self.width_var.get()
        h = self.height_var.get()
        if w & 1:
            w += 1
            self.width_var.set(w)
        s.resize(w, h)
        self._redraw()

    def _copy_sprite(self):
        s = self._cur()
        if s:
            self._clipboard = s.to_dict()

    def _paste_sprite(self):
        if not hasattr(self, '_clipboard') or not self._clipboard:
            return
        s = self._cur()
        if not s:
            return
        src = self._clipboard
        # Paste pixels into current sprite (resize to match if needed)
        s.resize(src['width'], src['height'])
        s.pixels = [row[:] for row in src['pixels']]
        self.width_var.set(s.width)
        self.height_var.set(s.height)
        self._redraw()

    def _sprite_op(self, method_name):
        s = self._cur()
        if s:
            getattr(s, method_name)()
            self._redraw()

    def _flip_h(self):     self._sprite_op('flip_h')
    def _flip_v(self):     self._sprite_op('flip_v')
    def _move_up(self):    self._sprite_op('move_up')
    def _move_down(self):  self._sprite_op('move_down')
    def _move_left(self):  self._sprite_op('move_left')
    def _move_right(self): self._sprite_op('move_right')

    def _clear_sprite(self):
        s = self._cur()
        if s:
            s.pixels = [[0] * s.width for _ in range(s.height)]
            self._redraw()

    def _pixel_at(self, event):
        s = self._cur()
        if not s:
            return None, None
        cw = self.canvas.winfo_width()
        ch = self.canvas.winfo_height()
        cell = min(cw // s.width, ch // s.height, CELL_SIZE)
        ox = (cw - s.width * cell) // 2
        oy = (ch - s.height * cell) // 2
        px = (event.x - ox) // cell
        py = (event.y - oy) // cell
        if 0 <= px < s.width and 0 <= py < s.height:
            return px, py
        return None, None

    def _on_click(self, event):
        px, py = self._pixel_at(event)
        if px is not None:
            self._cur().pixels[py][px] = self.current_color
            self._redraw()

    def _on_drag(self, event):
        self._on_click(event)

    def _on_right_click(self, event):
        """Right-click: pick color from pixel"""
        px, py = self._pixel_at(event)
        if px is not None:
            c = self._cur().pixels[py][px]
            self._set_color(c)

    def _redraw(self):
        s = self._cur()
        if not s:
            return

        self.canvas.delete("all")
        cw = self.canvas.winfo_width()
        ch = self.canvas.winfo_height()
        cell = min(cw // s.width, ch // s.height, CELL_SIZE)
        if cell < 2:
            cell = 2
        ox = (cw - s.width * cell) // 2
        oy = (ch - s.height * cell) // 2

        for y in range(s.height):
            for x in range(s.width):
                c = s.pixels[y][x]
                color = QL_COLORS[c][0]
                x1 = ox + x * cell
                y1 = oy + y * cell
                self.canvas.create_rectangle(
                    x1, y1, x1 + cell, y1 + cell,
                    fill=color, outline="#333333", width=1
                )

        # Grid lines for every 2 pixels (byte boundaries)
        for x in range(0, s.width + 1, 2):
            x1 = ox + x * cell
            self.canvas.create_line(x1, oy, x1, oy + s.height * cell,
                                    fill="#555555", width=1)

        # Update preview
        self._update_preview(s)

        # Update size
        byte_count = (s.width // 2) * s.height
        self.size_label.config(text=f"Bytes: {byte_count}")

    def _update_preview(self, s):
        self.preview.delete("all")
        scale = 2
        pw = s.width * scale
        ph = s.height * scale
        canvas_w = max(pw + 10, 80)
        canvas_h = max(ph + 10, 80)
        self.preview.config(width=canvas_w, height=canvas_h)
        # Center sprite in preview
        ox = (canvas_w - pw) // 2
        oy = (canvas_h - ph) // 2
        for y in range(s.height):
            for x in range(s.width):
                c = s.pixels[y][x]
                if c == 0:
                    continue
                color = QL_COLORS[c][0]
                self.preview.create_rectangle(
                    ox + x * scale, oy + y * scale,
                    ox + (x + 1) * scale, oy + (y + 1) * scale,
                    fill=color, outline=""
                )

    def _find_anim_group(self):
        """Find sprites that share the same base name (e.g., player_walk_0, player_walk_1)."""
        s = self._cur()
        if not s:
            return []
        # Strip trailing _N to get base name
        name = s.name
        parts = name.rsplit('_', 1)
        if len(parts) == 2 and parts[1].isdigit():
            base = parts[0] + '_'
        else:
            return [self.current_idx]
        # Find all sprites with same base
        indices = []
        for i, sp in enumerate(self.sprites):
            if sp.name.startswith(base):
                suffix = sp.name[len(base):]
                if suffix.isdigit():
                    indices.append(i)
        return sorted(indices) if len(indices) > 1 else [self.current_idx]

    def _toggle_anim(self):
        """Toggle animation preview."""
        if self.anim_var.get():
            self._anim_group = self._find_anim_group()
            self._anim_frame = 0
            self._anim_tick()
        else:
            if self._anim_after_id:
                self.root.after_cancel(self._anim_after_id)
                self._anim_after_id = None

    def _anim_tick(self):
        """Advance animation frame and update preview."""
        if not self.anim_var.get():
            return
        group = self._anim_group
        if not group:
            return
        idx = group[self._anim_frame % len(group)]
        s = self.sprites[idx]
        self._update_preview(s)
        self._anim_frame += 1
        self._anim_after_id = self.root.after(200, self._anim_tick)

    # --- File I/O ---

    def _new_project(self):
        self.sprites = [Sprite("sprite_0", 10, 20)]
        self.img_tab.set_images_data([])
        self.project_file = None
        self._update_sprite_list()
        self._select_sprite(0)

    def _save_project(self):
        if not self.project_file:
            self._save_project_as()
            return
        data = {
            "sprites": [s.to_dict() for s in self.sprites],
            "images": self.img_tab.get_images_data(),
        }
        with open(self.project_file, "w") as f:
            json.dump(data, f, indent=2)

    def _save_project_as(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("JSON", "*.json"), ("All", "*.*")]
        )
        if path:
            self.project_file = path
            self._save_project()

    def _open_project(self):
        path = filedialog.askopenfilename(
            filetypes=[("JSON", "*.json"), ("All", "*.*")]
        )
        if path:
            self._load_project(path)

    def _load_project(self, path):
        with open(path) as f:
            data = json.load(f)
        self.sprites = [Sprite.from_dict(d) for d in data["sprites"]]
        self.img_tab.set_images_data(data.get("images", []))
        self.project_file = path
        self._update_sprite_list()
        self._select_sprite(0)

    # --- C Export ---

    def _take_screenshot(self):
        if hasattr(self, 'emu_tab'):
            self.emu_tab._screenshot()

    def _export_c(self):
        initial = getattr(self, '_last_export_dir', self._ql_dir)
        path = filedialog.askdirectory(title="Export to directory",
                                       initialdir=initial)
        if not path:
            return
        self._last_export_dir = path
        self._write_c_files(path)
        messagebox.showinfo("Export", f"Exported {len(self.sprites)} sprites to:\n"
                           f"{path}/sprites.c\n{path}/sprites.h")

    def _write_c_files(self, directory):
        c_lines = [
            "/* sprites.c — Sprite bitmap data for QL Mode 8 */",
            "/* Generated by QL Studio (qlstudio.py) */", "",
            '#include "sprites.h"', "",
        ]
        h_lines = [
            "/* sprites.h — Sprite declarations for QL Mode 8 */",
            "/* Generated by QL Studio (qlstudio.py) */", "",
            "#ifndef SPRITES_H", "#define SPRITES_H", "",
            '#include "game.h"', "",
            "typedef struct {", "    uint8_t w;", "    uint8_t h;",
            "    const uint8_t *data;", "} Sprite;", "",
        ]

        for s in self.sprites:
            cname = s.name.lower().replace(" ", "_")
            data = s.to_c_bytes()
            wb = s.width // 2
            c_lines.append(f"/* {s.name} — {s.width}x{s.height} ({wb} bytes/row x {s.height} rows) */")
            c_lines.append(f"static const uint8_t spr_{cname}_data[] = {{")
            for row in data:
                c_lines.append("    " + ", ".join(f"0x{b:02X}" for b in row) + ",")
            c_lines.append("};")
            c_lines.append(f"const Sprite spr_{cname} = {{ {s.width}, {s.height}, spr_{cname}_data }};")
            c_lines.append("")
            h_lines.append(f"extern const Sprite spr_{cname};")

        h_lines.extend(["", "#endif", ""])

        with open(os.path.join(directory, "sprites.c"), "w") as f:
            f.write("\n".join(c_lines))
        with open(os.path.join(directory, "sprites.h"), "w") as f:
            f.write("\n".join(h_lines))


def main():
    import sys
    root = tk.Tk()
    root.geometry("1100x700")
    project = sys.argv[1] if len(sys.argv) > 1 else None
    app = SpriteEditor(root, project)

    def on_close():
        app.emu_tab.cleanup()
        root.destroy()
    root.protocol("WM_DELETE_WINDOW", on_close)

    root.mainloop()


if __name__ == "__main__":
    main()
