"""
Image Editor tab — Edit 256x256 pixel images for Sinclair QL Mode 8.

Uses PIL/Pillow for efficient rendering instead of individual Tkinter
rectangles. Provides the same 8-color palette and drawing tools as the
sprite editor, with 1x/2x/4x zoom levels.
"""

import tkinter as tk
from tkinter import ttk
import os

try:
    from PIL import Image, ImageTk
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

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

# Color index to RGB tuple for PIL
_COLOR_RGB = {
    0: (0, 0, 0),
    1: (255, 0, 0),
    2: (0, 0, 255),
    3: (255, 0, 255),
    4: (0, 255, 0),
    5: (0, 255, 255),
    6: (255, 255, 0),
    7: (255, 255, 255),
}

IMAGE_SIZE = 256
DEFAULT_ZOOM = 2
MODE8_BYTES_PER_IMAGE = 32768  # 256 * 256 / 2 = 32KB


def _make_empty_pixels():
    """Create a 256x256 pixel array filled with color 0."""
    return [[0] * IMAGE_SIZE for _ in range(IMAGE_SIZE)]


def _make_image_dict(name="image"):
    """Create a new image data dict."""
    return {
        "name": name,
        "width": IMAGE_SIZE,
        "height": IMAGE_SIZE,
        "pixels": _make_empty_pixels(),
    }


class ImageEditorTab:
    """Image editor tab for the QL Studio notebook."""

    def __init__(self, parent, editor):
        """
        Args:
            parent: ttk.Frame to build into (the notebook tab)
            editor: SpriteEditor instance (for accessing project data)
        """
        self.parent = parent
        self.editor = editor
        self.root = parent.winfo_toplevel()

        self.images = []
        self.current_idx = 0
        self.current_color = 7  # white
        self.zoom = DEFAULT_ZOOM
        self._pil_image = None
        self._photo = None
        self._redraw_pending = False

        self._build_ui()

    def _build_ui(self):
        if not HAS_PIL:
            lbl = ttk.Label(self.parent,
                            text="PIL/Pillow required for image editor.\n\n"
                                 "Install: pip3 install Pillow",
                            anchor=tk.CENTER, justify=tk.CENTER)
            lbl.pack(expand=True)
            return

        main = ttk.Frame(self.parent)
        main.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self._build_left_panel(main)
        self._build_center(main)
        self._build_right_panel(main)

    def _build_left_panel(self, parent):
        """Build the image list panel."""
        left = ttk.LabelFrame(parent, text="Images")
        left.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 5))

        self.image_listbox = tk.Listbox(left, width=18, height=20)
        self.image_listbox.pack(fill=tk.Y, expand=True, padx=3, pady=3)
        self.image_listbox.bind("<<ListboxSelect>>", self._on_list_select)

        btn_frame = ttk.Frame(left)
        btn_frame.pack(fill=tk.X, padx=3, pady=3)
        ttk.Button(btn_frame, text="+", width=3,
                   command=self._add_image).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="-", width=3,
                   command=self._del_image).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="Dup", width=4,
                   command=self._dup_image).pack(side=tk.LEFT)

        # Name entry
        prop_frame = ttk.LabelFrame(left, text="Properties")
        prop_frame.pack(fill=tk.X, padx=3, pady=3)

        ttk.Label(prop_frame, text="Name:").grid(row=0, column=0, sticky=tk.W)
        self.name_var = tk.StringVar()
        self.name_entry = ttk.Entry(prop_frame, textvariable=self.name_var,
                                    width=14)
        self.name_entry.grid(row=0, column=1)
        self.name_entry.bind("<Return>", self._rename_image)

        # Edit operations
        op_frame = ttk.LabelFrame(left, text="Operations")
        op_frame.pack(fill=tk.X, padx=3, pady=3)
        ttk.Button(op_frame, text="Clear",
                   command=self._clear_image).pack(fill=tk.X, padx=2, pady=1)
        ttk.Button(op_frame, text="Flip H",
                   command=self._flip_h).pack(fill=tk.X, padx=2, pady=1)

    def _build_center(self, parent):
        """Build the canvas area with scrollbars."""
        center = ttk.Frame(parent)
        center.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Canvas with scrollbars
        self.canvas = tk.Canvas(center, bg="#222222", cursor="crosshair")
        self.h_scroll = ttk.Scrollbar(center, orient=tk.HORIZONTAL,
                                      command=self.canvas.xview)
        self.v_scroll = ttk.Scrollbar(center, orient=tk.VERTICAL,
                                      command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=self.h_scroll.set,
                              yscrollcommand=self.v_scroll.set)

        self.v_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.h_scroll.pack(side=tk.BOTTOM, fill=tk.X)
        self.canvas.pack(fill=tk.BOTH, expand=True)

        self.canvas.bind("<Button-1>", self._on_click)
        self.canvas.bind("<B1-Motion>", self._on_drag)
        self.canvas.bind("<Button-3>", self._on_right_click)
        self.canvas.bind("<B3-Motion>", self._on_right_click)

    def _build_right_panel(self, parent):
        """Build the palette and zoom controls."""
        right = ttk.Frame(parent)
        right.pack(side=tk.RIGHT, fill=tk.Y, padx=(5, 0))

        # Palette
        pal_frame = ttk.LabelFrame(right, text="Palette")
        pal_frame.pack(fill=tk.X, pady=(0, 5))

        self.pal_buttons = []
        for i in range(8):
            color, name = QL_COLORS[i]
            f = tk.Frame(pal_frame, bd=2, relief=tk.RAISED)
            f.pack(fill=tk.X, padx=2, pady=1)
            swatch = tk.Canvas(f, width=24, height=18, highlightthickness=0)
            swatch.pack(side=tk.LEFT, padx=2)
            swatch.create_rectangle(0, 0, 25, 19, fill=color,
                                    outline="#888888")
            swatch.bind("<Button-1>", lambda e, c=i: self._set_color(c))
            lbl = tk.Label(f, text=f"{i}: {name}", anchor=tk.W)
            lbl.pack(side=tk.LEFT, fill=tk.X)
            lbl.bind("<Button-1>", lambda e, c=i: self._set_color(c))
            self.pal_buttons.append(f)

        # Current color indicator
        self.color_label = ttk.Label(right, text="Color: 7 (white)")
        self.color_label.pack(pady=5)

        # Zoom controls
        zoom_frame = ttk.LabelFrame(right, text="Zoom")
        zoom_frame.pack(fill=tk.X, pady=5)

        self.zoom_var = tk.IntVar(value=DEFAULT_ZOOM)
        for z in (1, 2, 4):
            ttk.Radiobutton(zoom_frame, text=f"{z}x", value=z,
                            variable=self.zoom_var,
                            command=lambda: self._set_zoom(
                                self.zoom_var.get())
                            ).pack(anchor=tk.W, padx=5, pady=1)

        # Size info
        self.size_label = ttk.Label(right, text="256x256 (32KB)")
        self.size_label.pack(pady=5)

        # Export button
        ttk.Button(right, text="Export C Files",
                   command=self._export_c).pack(pady=5)

    # --- Color ---

    def _set_color(self, c):
        self.current_color = c
        color, name = QL_COLORS[c]
        self.color_label.config(text=f"Color: {c} ({name})")
        for i, f in enumerate(self.pal_buttons):
            f.config(relief=tk.SUNKEN if i == c else tk.RAISED)

    def on_key(self, event):
        """Handle keyboard shortcuts (called from editor's central handler)."""
        if event.char in "01234567":
            self._set_color(int(event.char))

    # --- Image list management ---

    def _cur(self):
        """Get current image dict or None."""
        if 0 <= self.current_idx < len(self.images):
            return self.images[self.current_idx]
        return None

    def _update_image_list(self):
        self.image_listbox.delete(0, tk.END)
        for img in self.images:
            self.image_listbox.insert(tk.END, img["name"])

    def _select_image(self, idx):
        if idx < 0 or idx >= len(self.images):
            return
        self.current_idx = idx
        self.image_listbox.selection_clear(0, tk.END)
        self.image_listbox.selection_set(idx)
        img = self._cur()
        self.name_var.set(img["name"])
        self._rebuild_pil_image()
        self._update_display()

    def _on_list_select(self, event):
        sel = self.image_listbox.curselection()
        if sel:
            self._select_image(sel[0])

    def _add_image(self):
        name = f"image_{len(self.images)}"
        self.images.append(_make_image_dict(name))
        self._update_image_list()
        self._select_image(len(self.images) - 1)

    def _del_image(self):
        if len(self.images) <= 1:
            return
        del self.images[self.current_idx]
        self._update_image_list()
        self._select_image(min(self.current_idx, len(self.images) - 1))

    def _dup_image(self):
        img = self._cur()
        if not img:
            return
        dup = {
            "name": img["name"] + "_copy",
            "width": IMAGE_SIZE,
            "height": IMAGE_SIZE,
            "pixels": [row[:] for row in img["pixels"]],
        }
        self.images.append(dup)
        self._update_image_list()
        self._select_image(len(self.images) - 1)

    def _rename_image(self, event=None):
        img = self._cur()
        if img:
            img["name"] = self.name_var.get().strip().replace(" ", "_")
            self._update_image_list()

    # --- Edit operations ---

    def _clear_image(self):
        img = self._cur()
        if img:
            img["pixels"] = _make_empty_pixels()
            self._rebuild_pil_image()
            self._update_display()

    def _flip_h(self):
        img = self._cur()
        if img:
            for y in range(IMAGE_SIZE):
                img["pixels"][y] = img["pixels"][y][::-1]
            self._rebuild_pil_image()
            self._update_display()

    # --- Rendering ---

    def _rebuild_pil_image(self):
        """Full rebuild of PIL Image from pixel data."""
        img = self._cur()
        if not img:
            return
        self._pil_image = Image.new("RGB", (IMAGE_SIZE, IMAGE_SIZE))
        pixels = img["pixels"]
        for y in range(IMAGE_SIZE):
            for x in range(IMAGE_SIZE):
                self._pil_image.putpixel((x, y),
                                         _COLOR_RGB[pixels[y][x]])

    def _update_display(self):
        """Zoom PIL image and display on canvas."""
        if self._pil_image is None:
            return
        display_size = IMAGE_SIZE * self.zoom
        zoomed = self._pil_image.resize((display_size, display_size),
                                        Image.NEAREST)
        self._photo = ImageTk.PhotoImage(zoomed)
        self.canvas.delete("all")
        self.canvas.create_image(0, 0, anchor=tk.NW, image=self._photo)
        self.canvas.configure(scrollregion=(0, 0, display_size, display_size))

    def _schedule_redraw(self):
        """Coalesce rapid redraws via after_idle."""
        if not self._redraw_pending:
            self._redraw_pending = True
            self.root.after_idle(self._do_redraw)

    def _do_redraw(self):
        self._redraw_pending = False
        self._update_display()

    # --- Drawing ---

    def _pixel_at(self, event):
        """Convert canvas event coordinates to image pixel coords."""
        # Account for scroll position
        cx = self.canvas.canvasx(event.x)
        cy = self.canvas.canvasy(event.y)
        px = int(cx) // self.zoom
        py = int(cy) // self.zoom
        if 0 <= px < IMAGE_SIZE and 0 <= py < IMAGE_SIZE:
            return px, py
        return None, None

    def _paint_pixel(self, px, py):
        """Update a single pixel in both the data and PIL image."""
        img = self._cur()
        if not img:
            return
        img["pixels"][py][px] = self.current_color
        if self._pil_image:
            self._pil_image.putpixel((px, py),
                                     _COLOR_RGB[self.current_color])
        self._schedule_redraw()

    def _on_click(self, event):
        px, py = self._pixel_at(event)
        if px is not None:
            self._paint_pixel(px, py)

    def _on_drag(self, event):
        self._on_click(event)

    def _on_right_click(self, event):
        """Right-click: pick color from pixel."""
        px, py = self._pixel_at(event)
        if px is not None:
            img = self._cur()
            if img:
                c = img["pixels"][py][px]
                self._set_color(c)

    # --- Zoom ---

    def _set_zoom(self, level):
        self.zoom = level
        self._update_display()

    # --- Serialization ---

    def get_images_data(self):
        """Return images data for JSON serialization."""
        return [
            {
                "name": img["name"],
                "width": img["width"],
                "height": img["height"],
                "pixels": img["pixels"],
            }
            for img in self.images
        ]

    def set_images_data(self, data):
        """Load images from JSON data."""
        self.images = []
        for d in data:
            self.images.append({
                "name": d["name"],
                "width": d.get("width", IMAGE_SIZE),
                "height": d.get("height", IMAGE_SIZE),
                "pixels": d["pixels"],
            })
        self._update_image_list()
        if self.images:
            self._select_image(0)

    # --- C Export ---

    def _export_c(self):
        from tkinter import filedialog, messagebox
        path = filedialog.askdirectory(title="Export images to directory")
        if not path:
            return
        self.export_c(path)
        messagebox.showinfo("Export",
                            f"Exported {len(self.images)} images to:\n"
                            f"{path}/images.c\n{path}/images.h")

    def export_c(self, directory):
        """Generate images.c and images.h with raw Mode 8 data."""
        c_lines = [
            "/* images.c — Image data for QL Mode 8 */",
            "/* Generated by QL Studio (image_editor.py) */", "",
            '#include "images.h"', "",
        ]
        h_lines = [
            "/* images.h — Image declarations for QL Mode 8 */",
            "/* Generated by QL Studio (image_editor.py) */", "",
            "#ifndef IMAGES_H", "#define IMAGES_H", "",
            "#include <stdint.h>", "",
        ]

        for img in self.images:
            cname = img["name"].lower().replace(" ", "_")
            mode8 = self._image_to_mode8_bytes(img)
            sz = MODE8_BYTES_PER_IMAGE
            c_lines.append(f"const uint8_t img_{cname}[{sz}] = {{")
            for i in range(0, len(mode8), 16):
                vals = ", ".join(f"0x{b:02X}" for b in mode8[i:i + 16])
                c_lines.append(f"    {vals},")
            c_lines.extend(["};", ""])
            h_lines.append(f"extern const uint8_t img_{cname}[{sz}];")

        h_lines.extend(["", "#endif", ""])

        with open(os.path.join(directory, "images.c"), "w") as f:
            f.write("\n".join(c_lines))
        with open(os.path.join(directory, "images.h"), "w") as f:
            f.write("\n".join(h_lines))

    def _image_to_mode8_bytes(self, img):
        """Encode 256x256 pixels to Mode 8 byte pairs.

        4 pixels per 2 bytes: even=green+flash plane, odd=red+blue plane.
        Color remap: swap indices 5/6 (cyan/yellow fix).
        """
        col_remap = [0, 1, 2, 3, 4, 6, 5, 7]
        data = []
        for y in range(IMAGE_SIZE):
            row = img["pixels"][y]
            for x in range(0, IMAGE_SIZE, 4):
                even_byte = 0
                odd_byte = 0
                for bit in range(4):
                    c = col_remap[row[x + bit] & 7]
                    shift = 7 - (bit * 2)
                    even_byte |= (((c >> 2) & 1) << shift)
                    odd_byte |= ((c & 1) << shift)
                    odd_byte |= (((c >> 1) & 1) << (shift - 1))
                data.append(even_byte)
                data.append(odd_byte)
        return data
