#!/usr/bin/env python3
"""
QL Sprite Editor — Design sprites for Sinclair QL Mode 8

Features:
  - 8 colors (QL Mode 8 palette)
  - Nibble-packed format (2 pixels per byte)
  - Multiple sprites with names and sizes
  - Export to sprites.c / sprites.h
  - Save/load project as JSON
  - Horizontal flip preview

Usage: python3 sprite_editor.py [project.json]
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
        self.root.title("QL Sprite Editor")
        self.sprites = []
        self.current_idx = 0
        self.current_color = 7  # white
        self.drawing = False
        self.project_file = project_file

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
        filemenu.add_command(label="New Project", command=self._new_project)
        filemenu.add_command(label="Open Project...", command=self._open_project)
        filemenu.add_command(label="Save Project", command=self._save_project)
        filemenu.add_command(label="Save Project As...", command=self._save_project_as)
        filemenu.add_separator()
        filemenu.add_command(label="Export C files...", command=self._export_c)
        filemenu.add_separator()
        filemenu.add_command(label="Quit", command=self.root.quit)
        menubar.add_cascade(label="File", menu=filemenu)

        editmenu = tk.Menu(menubar, tearoff=0)
        editmenu.add_command(label="Flip Horizontal", command=self._flip_h)
        editmenu.add_command(label="Clear Sprite", command=self._clear_sprite)
        menubar.add_cascade(label="Edit", menu=editmenu)

        self.root.config(menu=menubar)

        # Main layout: left panel (sprite list), center (canvas), right (palette + info)
        main = ttk.Frame(self.root)
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

        # Right: palette + preview
        right = ttk.Frame(main)
        right.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))

        # Palette
        pal_frame = ttk.LabelFrame(right, text="Palette")
        pal_frame.pack(fill=tk.X, pady=(0, 5))

        self.pal_buttons = []
        for i in range(8):
            color, name = QL_COLORS[i]
            f = tk.Frame(pal_frame, bd=2, relief=tk.RAISED)
            f.pack(fill=tk.X, padx=2, pady=1)
            btn = tk.Button(f, bg=color, width=3, height=1,
                           activebackground=color,
                           command=lambda c=i: self._set_color(c))
            btn.pack(side=tk.LEFT, padx=2)
            lbl = tk.Label(f, text=f"{i}: {name}", anchor=tk.W)
            lbl.pack(side=tk.LEFT, fill=tk.X)
            self.pal_buttons.append(f)

        # Current color indicator
        self.color_label = ttk.Label(right, text="Color: 7 (white)")
        self.color_label.pack(pady=5)

        # Preview (actual size)
        preview_frame = ttk.LabelFrame(right, text="Preview (2x)")
        preview_frame.pack(fill=tk.X, pady=5)
        self.preview = tk.Canvas(preview_frame, width=80, height=80, bg="#000000")
        self.preview.pack(padx=5, pady=5)

        # Byte size info
        self.size_label = ttk.Label(right, text="Bytes: 0")
        self.size_label.pack(pady=5)

        # Export button
        ttk.Button(right, text="Export C Files", command=self._export_c).pack(pady=5)

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
        if event.char in "01234567":
            self._set_color(int(event.char))

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

    def _flip_h(self):
        s = self._cur()
        if s:
            s.flip_h()
            self._redraw()

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
        self.preview.config(width=max(pw + 10, 80), height=max(ph + 10, 80))
        ox = 5
        oy = 5
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

    # --- File I/O ---

    def _new_project(self):
        self.sprites = [Sprite("sprite_0", 10, 20)]
        self.project_file = None
        self._update_sprite_list()
        self._select_sprite(0)

    def _save_project(self):
        if not self.project_file:
            self._save_project_as()
            return
        data = {"sprites": [s.to_dict() for s in self.sprites]}
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
        self.project_file = path
        self._update_sprite_list()
        self._select_sprite(0)

    # --- C Export ---

    def _export_c(self):
        path = filedialog.askdirectory(title="Export to directory")
        if not path:
            return
        self._write_c_files(path)
        messagebox.showinfo("Export", f"Exported {len(self.sprites)} sprites to:\n"
                           f"{path}/sprites.c\n{path}/sprites.h")

    def _write_c_files(self, directory):
        c_lines = []
        h_lines = []

        c_lines.append("/*")
        c_lines.append(" * sprites.c — Sprite bitmap data for QL Mode 8")
        c_lines.append(" * Generated by sprite_editor.py")
        c_lines.append(" *")
        c_lines.append(" * Mode 8: 2 pixels per byte, high nibble = left pixel, low nibble = right.")
        c_lines.append(" * Color key: 0=transparent 1=red 2=blue 3=magenta 4=green 5=cyan 6=yellow 7=white")
        c_lines.append(" */")
        c_lines.append("")
        c_lines.append('#include "sprites.h"')
        c_lines.append("")

        h_lines.append("/*")
        h_lines.append(" * sprites.h — Sprite declarations for QL Mode 8")
        h_lines.append(" * Generated by sprite_editor.py")
        h_lines.append(" */")
        h_lines.append("")
        h_lines.append("#ifndef SPRITES_H")
        h_lines.append("#define SPRITES_H")
        h_lines.append("")
        h_lines.append('#include "game.h"')
        h_lines.append("")
        h_lines.append("typedef struct {")
        h_lines.append("    uint8_t w;")
        h_lines.append("    uint8_t h;")
        h_lines.append("    const uint8_t *data;")
        h_lines.append("} Sprite;")
        h_lines.append("")

        for s in self.sprites:
            cname = s.name.lower().replace(" ", "_")
            data = s.to_c_bytes()
            wb = s.width // 2

            # C data array
            c_lines.append(f"/* {s.name} — {s.width}x{s.height} ({wb} bytes/row x {s.height} rows) */")
            c_lines.append(f"static const uint8_t spr_{cname}_data[] = {{")
            for y, row in enumerate(data):
                hex_vals = ", ".join(f"0x{b:02X}" for b in row)
                c_lines.append(f"    {hex_vals},")
            c_lines.append("};")
            c_lines.append(f"const Sprite spr_{cname} = {{ {s.width}, {s.height}, spr_{cname}_data }};")
            c_lines.append("")

            # H declaration
            h_lines.append(f"extern const Sprite spr_{cname};")

        h_lines.append("")
        h_lines.append("#endif")
        h_lines.append("")

        with open(os.path.join(directory, "sprites.c"), "w") as f:
            f.write("\n".join(c_lines))

        with open(os.path.join(directory, "sprites.h"), "w") as f:
            f.write("\n".join(h_lines))


def main():
    import sys
    root = tk.Tk()
    root.geometry("900x650")
    project = sys.argv[1] if len(sys.argv) > 1 else None
    app = SpriteEditor(root, project)
    root.mainloop()


if __name__ == "__main__":
    main()
