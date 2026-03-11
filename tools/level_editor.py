#!/usr/bin/env python3
"""
Vectrex H.E.R.O. Level & Sprite Editor

Visual editor for designing levels and VLC sprites for the Vectrex H.E.R.O. game.
Exports data as .h header files ready to #include in main.c.

Usage: python3 tools/level_editor.py
"""

import subprocess
import sys
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import json
import math
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vec2x_py.emulator import Emulator, EMU_TIMER
from vec2x_py.renderer import SCREEN_WIDTH, SCREEN_HEIGHT

try:
    from PIL import Image, ImageTk
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

# ---------------------------------------------------------------------------
# Vectrex coordinate system: -128..127 both axes, Y-up
# Game uses roughly (-90,-95) to (90,105)
# ---------------------------------------------------------------------------

VX_MIN = -128
VX_MAX = 127
VX_RANGE = VX_MAX - VX_MIN  # 255

CANVAS_SIZE = 600
SPRITE_CANVAS_SIZE = 400
SPRITE_COORD_RANGE = 40  # -20..20

GRID_SNAP = 5
CLICK_TOLERANCE = 8  # pixels


def vx_to_canvas(vx, vy, canvas_size=CANVAS_SIZE, vmin=VX_MIN, vmax=VX_MAX):
    """Convert Vectrex coords to canvas pixel coords (Y flipped)."""
    rng = vmax - vmin
    cx = (vx - vmin) / rng * canvas_size
    cy = (1.0 - (vy - vmin) / rng) * canvas_size
    return cx, cy


def canvas_to_vx(cx, cy, canvas_size=CANVAS_SIZE, vmin=VX_MIN, vmax=VX_MAX):
    """Convert canvas pixel coords to Vectrex coords (Y flipped)."""
    rng = vmax - vmin
    vx = cx / canvas_size * rng + vmin
    vy = (1.0 - cy / canvas_size) * rng + vmin
    return vx, vy


def snap(val, grid=GRID_SNAP):
    """Snap value to nearest grid increment."""
    return round(val / grid) * grid


def clamp(val, lo=-128, hi=127):
    return max(lo, min(hi, int(val)))


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

def new_level():
    return {
        "name": "Level 1",
        "cave_lines": [],       # list of polylines, each is list of [x, y]
        "walls": [],            # list of {"y": int, "x": int, "h": int, "w": int, "destroyable": bool}
        "enemies": [],          # list of {"x": int, "y": int, "vx": int}
        "miner": None,          # {"x": int, "y": int} or None
        "player_start": None,   # {"x": int, "y": int} or None
        "cave_constants": {
            "CAVE_LEFT": -90, "CAVE_RIGHT": 90,
            "CAVE_TOP": 105, "CAVE_FLOOR": -95,
        },
        "bg_image": None,       # {"path": str, "x": int, "y": int, "scale": float} or None
    }


def new_sprite():
    return {
        "name": "sprite",
        "points": [],  # list of [x, y] -- connected line segments
        "bg_image": None,       # {"path": str, "x": int, "y": int, "scale": float} or None
    }


def new_project():
    return {
        "levels": [new_level()],
        "sprites": [new_sprite()],
    }


# ---------------------------------------------------------------------------
# Background image helper
# ---------------------------------------------------------------------------

def load_bg_image(path, canvas_size, scale=1.0, opacity=0.5):
    """Load an image, apply scale, apply opacity. Returns PhotoImage or None."""
    if not HAS_PIL or not path or not os.path.isfile(path):
        return None
    try:
        img = Image.open(path).convert("RGBA")
        # Apply user scale then clamp to canvas size
        w = max(1, int(img.width * scale))
        h = max(1, int(img.height * scale))
        img = img.resize((w, h), Image.LANCZOS)
        # Apply opacity by blending with dark background
        bg = Image.new("RGBA", img.size, (10, 10, 26, 255))  # match canvas bg
        blended = Image.blend(bg, img, opacity)
        return ImageTk.PhotoImage(blended)
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Level Editor Canvas
# ---------------------------------------------------------------------------

class LevelCanvas(tk.Canvas):
    TOOLS = ("select", "cave_line", "wall", "enemy", "miner", "player_start")

    def __init__(self, parent, app):
        super().__init__(parent, width=CANVAS_SIZE, height=CANVAS_SIZE,
                         bg="#0a0a1a", highlightthickness=0)
        self.app = app
        self.tool = "select"
        self.snap_enabled = True

        # Interaction state
        self._drag_item = None
        self._drag_type = None
        self._drag_idx = None
        self._drag_offset = (0, 0)
        self._wall_start = None      # (vx, vy) for wall drag start
        self._wall_rect_id = None
        self._polyline_pts = []      # points being drawn for current polyline
        self._polyline_ids = []      # canvas line ids for preview
        self._bg_photo = None        # cached PhotoImage for background
        self._bg_path_loaded = None  # path of currently loaded bg
        self._bg_scale_loaded = None # scale when last loaded

        self.bind("<Motion>", self._on_motion)
        self.bind("<Button-1>", self._on_click)
        self.bind("<B1-Motion>", self._on_drag)
        self.bind("<ButtonRelease-1>", self._on_release)
        self.bind("<Button-2>", self._on_right_click)
        self.bind("<Button-3>", self._on_right_click)
        self.bind("<Double-Button-1>", self._on_double_click)
        self.bind("<Delete>", self._on_delete)
        self.bind("<BackSpace>", self._on_delete)
        self.focus_set()

    @property
    def level(self):
        return self.app.current_level_data()

    def _vx(self, cx, cy):
        vx, vy = canvas_to_vx(cx, cy)
        if self.snap_enabled:
            vx, vy = snap(vx), snap(vy)
        return clamp(vx), clamp(vy)

    def _cx(self, vx, vy):
        return vx_to_canvas(vx, vy)

    # ---- Drawing ----

    def redraw(self):
        self.delete("all")
        self._draw_grid()
        self._draw_axes()
        lvl = self.level
        if lvl is None:
            return
        self._draw_bg_image(lvl)
        self._draw_cave_from_constants(lvl)
        self._draw_cave_lines(lvl)
        self._draw_walls(lvl)
        self._draw_enemies(lvl)
        self._draw_miner(lvl)
        self._draw_player_start(lvl)

    def _draw_grid(self):
        step = GRID_SNAP
        for v in range(VX_MIN, VX_MAX + 1, step):
            cx, _ = self._cx(v, 0)
            _, cy = self._cx(0, v)
            color = "#1a1a2e" if v % 50 != 0 else "#2a2a3e"
            self.create_line(cx, 0, cx, CANVAS_SIZE, fill=color, tags="grid")
            self.create_line(0, cy, CANVAS_SIZE, cy, fill=color, tags="grid")

    def _draw_axes(self):
        cx0, cy0 = self._cx(0, 0)
        self.create_line(cx0, 0, cx0, CANVAS_SIZE, fill="#333355", dash=(2, 4), tags="axis")
        self.create_line(0, cy0, CANVAS_SIZE, cy0, fill="#333355", dash=(2, 4), tags="axis")
        # Game bounds
        gl, _ = self._cx(-90, 0)
        gr, _ = self._cx(90, 0)
        _, gt = self._cx(0, 105)
        _, gb = self._cx(0, -95)
        self.create_rectangle(gl, gt, gr, gb, outline="#333344", dash=(4, 4), tags="bounds")

    def _draw_bg_image(self, lvl):
        bg = lvl.get("bg_image")
        if not bg or not bg.get("path"):
            return
        path = bg["path"]
        scale = bg.get("scale", 1.0)
        # Reload if path or scale changed
        if path != self._bg_path_loaded or scale != self._bg_scale_loaded:
            self._bg_photo = load_bg_image(path, CANVAS_SIZE, scale)
            self._bg_path_loaded = path
            self._bg_scale_loaded = scale
        if self._bg_photo:
            cx, cy = self._cx(bg.get("x", 0), bg.get("y", 0))
            self.create_image(cx, cy, image=self._bg_photo, anchor="center",
                              tags="bg_image")

    def _draw_cave_from_constants(self, lvl):
        """Draw cave bounding box from cave_constants."""
        c = lvl.get("cave_constants", {})
        cl = c.get("CAVE_LEFT")
        cr = c.get("CAVE_RIGHT")
        ct = c.get("CAVE_TOP")
        cf = c.get("CAVE_FLOOR")
        if None in (cl, cr, ct, cf):
            return
        x1, y1 = self._cx(cl, ct)
        x2, y2 = self._cx(cr, cf)
        self.create_rectangle(x1, y1, x2, y2, outline="#556688", width=1,
                              dash=(6, 3), tags="cave_const")

    def _draw_cave_lines(self, lvl):
        for pi, polyline in enumerate(lvl["cave_lines"]):
            if len(polyline) < 2:
                continue
            coords = []
            for pt in polyline:
                cx, cy = self._cx(pt[0], pt[1])
                coords.extend([cx, cy])
            self.create_line(*coords, fill="#e0e0ff", width=2, tags="cave_line")
            # Draw draggable point handles in select mode
            if self.tool == "select":
                for vi, pt in enumerate(polyline):
                    cx, cy = self._cx(pt[0], pt[1])
                    r = 4
                    selected = (self._drag_type == "cave_point"
                                and self._drag_idx == (pi, vi))
                    color = "#ffff00" if selected else "#e0e0ff"
                    self.create_oval(cx - r, cy - r, cx + r, cy + r,
                                     fill=color, outline=color,
                                     tags=("cave_point", f"cp_{pi}_{vi}"))

    def _draw_walls(self, lvl):
        for i, w in enumerate(lvl["walls"]):
            y, x, h, width = w["y"], w["x"], w["h"], w["w"]
            # Wall rect: left-x = x, top-y = y+h, right-x = x+w, bottom-y = y-h
            x1, y1 = self._cx(x, y + h)
            x2, y2 = self._cx(x + width, y - h)
            color = "#ff6644" if w.get("destroyable", False) else "#88aaff"
            dash = (4, 3) if w.get("destroyable", False) else ()
            self.create_rectangle(x1, y1, x2, y2, outline=color, width=2,
                                  dash=dash, tags=("wall", f"wall_{i}"))
            label = f"W{i}"
            mx, my = (x1 + x2) / 2, (y1 + y2) / 2
            self.create_text(mx, my, text=label, fill=color, font=("Courier", 9),
                             tags=("wall_label", f"wall_{i}"))

    def _draw_enemies(self, lvl):
        for i, e in enumerate(lvl["enemies"]):
            cx, cy = self._cx(e["x"], e["y"])
            r = 8
            self.create_oval(cx - r, cy - r, cx + r, cy + r,
                             outline="#ff3333", width=2, tags=("enemy", f"enemy_{i}"))
            # Bat wings
            self.create_line(cx - r, cy, cx - r - 5, cy - 4, fill="#ff3333",
                             tags=("enemy", f"enemy_{i}"))
            self.create_line(cx + r, cy, cx + r + 5, cy - 4, fill="#ff3333",
                             tags=("enemy", f"enemy_{i}"))
            # Direction arrow
            arrow_dx = 12 if e["vx"] > 0 else -12 if e["vx"] < 0 else 0
            if arrow_dx:
                self.create_line(cx, cy + r + 4, cx + arrow_dx, cy + r + 4,
                                 fill="#ff6666", arrow=tk.LAST, width=2,
                                 tags=("enemy", f"enemy_{i}"))
            self.create_text(cx, cy, text="B", fill="#ff3333",
                             font=("Courier", 8, "bold"), tags=("enemy", f"enemy_{i}"))

    def _draw_miner(self, lvl):
        if lvl["miner"] is None:
            return
        cx, cy = self._cx(lvl["miner"]["x"], lvl["miner"]["y"])
        r = 8
        self.create_rectangle(cx - r, cy - r, cx + r, cy + r,
                              outline="#00ff88", width=2, tags="miner")
        self.create_text(cx, cy, text="M", fill="#00ff88",
                         font=("Courier", 10, "bold"), tags="miner")

    def _draw_player_start(self, lvl):
        if lvl["player_start"] is None:
            return
        cx, cy = self._cx(lvl["player_start"]["x"], lvl["player_start"]["y"])
        r = 8
        self.create_polygon(cx, cy - r - 2, cx - r, cy + r, cx + r, cy + r,
                            outline="#ffff00", fill="", width=2, tags="player_start")
        self.create_text(cx, cy + 2, text="P", fill="#ffff00",
                         font=("Courier", 9, "bold"), tags="player_start")

    # ---- Tool: Select/Move ----

    def _hit_test(self, cx, cy):
        """Find what object is near canvas coords. Returns (type, index) or None."""
        lvl = self.level
        tol = CLICK_TOLERANCE
        # Cave line points (check first — small targets get priority)
        for pi, polyline in enumerate(lvl["cave_lines"]):
            for vi, pt in enumerate(polyline):
                px, py = self._cx(pt[0], pt[1])
                if abs(cx - px) < tol + 4 and abs(cy - py) < tol + 4:
                    return ("cave_point", (pi, vi))
        # Enemies
        for i, e in enumerate(lvl["enemies"]):
            ex, ey = self._cx(e["x"], e["y"])
            if abs(cx - ex) < tol + 8 and abs(cy - ey) < tol + 8:
                return ("enemy", i)
        # Walls
        for i, w in enumerate(lvl["walls"]):
            x1, y1 = self._cx(w["x"], w["y"] + w["h"])
            x2, y2 = self._cx(w["x"] + w["w"], w["y"] - w["h"])
            lx, rx = min(x1, x2), max(x1, x2)
            ty, by_ = min(y1, y2), max(y1, y2)
            if lx - tol <= cx <= rx + tol and ty - tol <= cy <= by_ + tol:
                return ("wall", i)
        # Miner
        if lvl["miner"]:
            mx, my = self._cx(lvl["miner"]["x"], lvl["miner"]["y"])
            if abs(cx - mx) < tol + 8 and abs(cy - my) < tol + 8:
                return ("miner", 0)
        # Player start
        if lvl["player_start"]:
            px, py = self._cx(lvl["player_start"]["x"], lvl["player_start"]["y"])
            if abs(cx - px) < tol + 8 and abs(cy - py) < tol + 8:
                return ("player_start", 0)
        # Background image (lowest priority — drag to reposition)
        if lvl.get("bg_image") and self._bg_photo:
            return ("bg_image", 0)
        return None

    def _selection_info(self):
        """Return info string for selected item."""
        lvl = self.level
        if self._drag_type == "cave_point":
            pi, vi = self._drag_idx
            pt = lvl["cave_lines"][pi][vi]
            return f"Cave point [line {pi}, pt {vi}]: ({pt[0]}, {pt[1]})"
        elif self._drag_type == "enemy":
            e = lvl["enemies"][self._drag_idx]
            return f"Enemy {self._drag_idx}: x={e['x']}, y={e['y']}, vx={e['vx']}"
        elif self._drag_type == "wall":
            w = lvl["walls"][self._drag_idx]
            return f"Wall {self._drag_idx}: y={w['y']}, x={w['x']}, h={w['h']}, w={w['w']}"
        elif self._drag_type == "miner":
            m = lvl["miner"]
            return f"Miner: x={m['x']}, y={m['y']}"
        elif self._drag_type == "player_start":
            p = lvl["player_start"]
            return f"Player Start: x={p['x']}, y={p['y']}"
        elif self._drag_type == "bg_image":
            bg = lvl.get("bg_image", {})
            return f"Background: x={bg.get('x', 0)}, y={bg.get('y', 0)}"
        return ""

    # ---- Event handlers ----

    def _on_motion(self, event):
        vx, vy = canvas_to_vx(event.x, event.y)
        self.app.update_status(f"Vectrex: ({int(vx)}, {int(vy)})  |  Tool: {self.tool}")

    def _on_click(self, event):
        self.focus_set()
        vx, vy = self._vx(event.x, event.y)

        if self.tool == "select":
            hit = self._hit_test(event.x, event.y)
            if hit:
                self._drag_type, self._drag_idx = hit
                self._drag_offset = (event.x, event.y)
                self.app.update_status(self._selection_info())
            else:
                self._drag_type = None
                self._drag_idx = None

        elif self.tool == "cave_line":
            self._polyline_pts.append([vx, vy])
            self._redraw_polyline_preview()

        elif self.tool == "enemy":
            self.level["enemies"].append({"x": vx, "y": vy, "vx": 1})
            self.redraw()

        elif self.tool == "miner":
            self.level["miner"] = {"x": vx, "y": vy}
            self.redraw()

        elif self.tool == "player_start":
            self.level["player_start"] = {"x": vx, "y": vy}
            self.redraw()

        elif self.tool == "wall":
            self._wall_start = (vx, vy)

    def _on_drag(self, event):
        if self.tool == "select" and self._drag_type:
            vx, vy = self._vx(event.x, event.y)
            lvl = self.level
            if self._drag_type == "cave_point":
                pi, vi = self._drag_idx
                lvl["cave_lines"][pi][vi] = [vx, vy]
            elif self._drag_type == "enemy":
                lvl["enemies"][self._drag_idx]["x"] = vx
                lvl["enemies"][self._drag_idx]["y"] = vy
            elif self._drag_type == "wall":
                w = lvl["walls"][self._drag_idx]
                w["x"] = vx
                w["y"] = vy
            elif self._drag_type == "miner":
                lvl["miner"]["x"] = vx
                lvl["miner"]["y"] = vy
            elif self._drag_type == "player_start":
                lvl["player_start"]["x"] = vx
                lvl["player_start"]["y"] = vy
            elif self._drag_type == "bg_image" and lvl.get("bg_image"):
                lvl["bg_image"]["x"] = vx
                lvl["bg_image"]["y"] = vy
            self.redraw()
            self.app.update_status(self._selection_info())

        elif self.tool == "wall" and self._wall_start:
            # Preview rectangle
            vx, vy = self._vx(event.x, event.y)
            sx, sy = self._wall_start
            x1, y1 = self._cx(min(sx, vx), max(sy, vy))
            x2, y2 = self._cx(max(sx, vx), min(sy, vy))
            if self._wall_rect_id:
                self.delete(self._wall_rect_id)
            self._wall_rect_id = self.create_rectangle(
                x1, y1, x2, y2, outline="#ff6644", width=2, dash=(4, 3))

    def _on_release(self, event):
        if self.tool == "wall" and self._wall_start:
            vx, vy = self._vx(event.x, event.y)
            sx, sy = self._wall_start
            left_x = min(sx, vx)
            right_x = max(sx, vx)
            top_y = max(sy, vy)
            bot_y = min(sy, vy)
            w = right_x - left_x
            h2 = (top_y - bot_y) // 2
            center_y = (top_y + bot_y) // 2
            if w > 0 and h2 > 0:
                self.level["walls"].append({
                    "y": clamp(center_y), "x": clamp(left_x),
                    "h": clamp(h2, 1, 127), "w": clamp(w, 1, 127),
                    "destroyable": True,
                })
            self._wall_start = None
            if self._wall_rect_id:
                self.delete(self._wall_rect_id)
                self._wall_rect_id = None
            self.redraw()

        if self.tool == "select":
            self._drag_type = None
            self._drag_idx = None

    def _on_right_click(self, event):
        if self.tool == "cave_line" and self._polyline_pts:
            self._finish_polyline()

    def _on_double_click(self, event):
        if self.tool == "cave_line" and len(self._polyline_pts) >= 2:
            self._finish_polyline()
        elif self.tool == "select":
            # Double-click to edit enemy vx or wall destroyable
            hit = self._hit_test(event.x, event.y)
            if hit:
                self._edit_properties(hit)

    def _on_delete(self, event):
        if self._drag_type and self._drag_idx is not None:
            lvl = self.level
            if self._drag_type == "cave_point":
                pi, vi = self._drag_idx
                polyline = lvl["cave_lines"][pi]
                del polyline[vi]
                # Remove polyline entirely if fewer than 2 points remain
                if len(polyline) < 2:
                    del lvl["cave_lines"][pi]
            elif self._drag_type == "enemy":
                del lvl["enemies"][self._drag_idx]
            elif self._drag_type == "wall":
                del lvl["walls"][self._drag_idx]
            elif self._drag_type == "miner":
                lvl["miner"] = None
            elif self._drag_type == "player_start":
                lvl["player_start"] = None
            self._drag_type = None
            self._drag_idx = None
            self.redraw()

    # ---- Polyline drawing ----

    def _redraw_polyline_preview(self):
        for lid in self._polyline_ids:
            self.delete(lid)
        self._polyline_ids.clear()
        pts = self._polyline_pts
        for i in range(len(pts)):
            cx, cy = self._cx(pts[i][0], pts[i][1])
            dot = self.create_oval(cx - 3, cy - 3, cx + 3, cy + 3,
                                   fill="#e0e0ff", outline="#e0e0ff")
            self._polyline_ids.append(dot)
            if i > 0:
                px, py = self._cx(pts[i - 1][0], pts[i - 1][1])
                ln = self.create_line(px, py, cx, cy, fill="#e0e0ff", width=2)
                self._polyline_ids.append(ln)

    def _finish_polyline(self):
        if len(self._polyline_pts) >= 2:
            self.level["cave_lines"].append(list(self._polyline_pts))
        self._polyline_pts.clear()
        for lid in self._polyline_ids:
            self.delete(lid)
        self._polyline_ids.clear()
        self.redraw()

    # ---- Property editing ----

    def _edit_properties(self, hit):
        obj_type, idx = hit
        lvl = self.level
        if obj_type == "enemy":
            e = lvl["enemies"][idx]
            self._prop_dialog("Enemy Properties", [
                ("x", e["x"]), ("y", e["y"]), ("vx", e["vx"]),
            ], lambda vals: e.update({"x": int(vals[0]), "y": int(vals[1]), "vx": int(vals[2])}))
        elif obj_type == "wall":
            w = lvl["walls"][idx]
            self._prop_dialog("Wall Properties", [
                ("y (center)", w["y"]), ("x (left)", w["x"]),
                ("h (half-height)", w["h"]), ("w (width)", w["w"]),
                ("destroyable (0/1)", int(w.get("destroyable", False))),
            ], lambda vals: w.update({
                "y": int(vals[0]), "x": int(vals[1]),
                "h": int(vals[2]), "w": int(vals[3]),
                "destroyable": bool(int(vals[4])),
            }))

    def _prop_dialog(self, title, fields, on_ok):
        dlg = tk.Toplevel(self)
        dlg.title(title)
        dlg.transient(self.winfo_toplevel())
        dlg.grab_set()
        entries = []
        for i, (label, val) in enumerate(fields):
            tk.Label(dlg, text=label + ":").grid(row=i, column=0, padx=5, pady=2, sticky="e")
            ent = tk.Entry(dlg, width=10)
            ent.insert(0, str(val))
            ent.grid(row=i, column=1, padx=5, pady=2)
            entries.append(ent)

        def apply():
            try:
                vals = [e.get() for e in entries]
                on_ok(vals)
                dlg.destroy()
                self.redraw()
            except ValueError:
                messagebox.showerror("Invalid input", "Please enter valid integers.")

        tk.Button(dlg, text="OK", command=apply).grid(
            row=len(fields), column=0, columnspan=2, pady=8)
        entries[0].focus_set()
        dlg.bind("<Return>", lambda e: apply())


# ---------------------------------------------------------------------------
# Sprite Editor Canvas
# ---------------------------------------------------------------------------

class SpriteCanvas(tk.Canvas):
    TOOLS = ("draw", "select")

    def __init__(self, parent, app):
        super().__init__(parent, width=SPRITE_CANVAS_SIZE, height=SPRITE_CANVAS_SIZE,
                         bg="#0a0a1a", highlightthickness=0)
        self.app = app
        self.snap_enabled = True
        self.tool = "draw"
        self._drag_idx = None      # index of point being dragged
        self._drag_bg = False      # dragging background image
        self._bg_photo = None      # cached PhotoImage
        self._bg_path_loaded = None
        self._bg_scale_loaded = None

        self.bind("<Motion>", self._on_motion)
        self.bind("<Button-1>", self._on_click)
        self.bind("<B1-Motion>", self._on_drag)
        self.bind("<ButtonRelease-1>", self._on_release)
        self.bind("<Button-2>", self._on_right_click)
        self.bind("<Button-3>", self._on_right_click)
        self.bind("<Delete>", self._on_delete)
        self.bind("<BackSpace>", self._on_delete)
        self.focus_set()

    @property
    def sprite(self):
        return self.app.current_sprite_data()

    def _vx(self, cx, cy):
        half = SPRITE_COORD_RANGE // 2
        vx, vy = canvas_to_vx(cx, cy, SPRITE_CANVAS_SIZE, -half, half)
        if self.snap_enabled:
            vx, vy = round(vx), round(vy)
        return clamp(vx, -half, half), clamp(vy, -half, half)

    def _cx(self, vx, vy):
        half = SPRITE_COORD_RANGE // 2
        return vx_to_canvas(vx, vy, SPRITE_CANVAS_SIZE, -half, half)

    def _hit_point(self, cx, cy):
        """Return index of point near canvas coords, or None."""
        sprite = self.sprite
        if not sprite:
            return None
        tol = CLICK_TOLERANCE + 4
        for i, pt in enumerate(sprite["points"]):
            px, py = self._cx(pt[0], pt[1])
            if abs(cx - px) < tol and abs(cy - py) < tol:
                return i
        return None

    def redraw(self):
        self.delete("all")
        self._draw_grid()
        sprite = self.sprite
        if sprite is None:
            return
        self._draw_bg_image(sprite)
        pts = sprite["points"]
        # Draw lines
        for i in range(len(pts) - 1):
            x1, y1 = self._cx(pts[i][0], pts[i][1])
            x2, y2 = self._cx(pts[i + 1][0], pts[i + 1][1])
            self.create_line(x1, y1, x2, y2, fill="#e0e0ff", width=2)
        # Draw points
        for i, pt in enumerate(pts):
            cx, cy = self._cx(pt[0], pt[1])
            r = 4
            selected = (self._drag_idx == i)
            if selected:
                color = "#ff4444"
            elif i == 0:
                color = "#ffff00"
            else:
                color = "#88aaff"
            self.create_oval(cx - r, cy - r, cx + r, cy + r, fill=color, outline=color)
            self.create_text(cx + 8, cy - 8, text=str(i), fill="#666688",
                             font=("Courier", 8))
        # Show VLC data
        self._show_vlc_text(sprite)

    def _draw_bg_image(self, sprite):
        bg = sprite.get("bg_image")
        if not bg or not bg.get("path"):
            return
        path = bg["path"]
        scale = bg.get("scale", 1.0)
        if path != self._bg_path_loaded or scale != self._bg_scale_loaded:
            self._bg_photo = load_bg_image(path, SPRITE_CANVAS_SIZE, scale)
            self._bg_path_loaded = path
            self._bg_scale_loaded = scale
        if self._bg_photo:
            cx, cy = self._cx(bg.get("x", 0), bg.get("y", 0))
            self.create_image(cx, cy, image=self._bg_photo, anchor="center",
                              tags="bg_image")

    def _draw_grid(self):
        half = SPRITE_COORD_RANGE // 2
        for v in range(-half, half + 1):
            cx, _ = self._cx(v, 0)
            _, cy = self._cx(0, v)
            color = "#1a1a2e" if v % 5 != 0 else "#2a2a3e"
            self.create_line(cx, 0, cx, SPRITE_CANVAS_SIZE, fill=color)
            self.create_line(0, cy, SPRITE_CANVAS_SIZE, cy, fill=color)
        # Axes
        cx0, cy0 = self._cx(0, 0)
        self.create_line(cx0, 0, cx0, SPRITE_CANVAS_SIZE, fill="#333355", dash=(2, 4))
        self.create_line(0, cy0, SPRITE_CANVAS_SIZE, cy0, fill="#333355", dash=(2, 4))

    def _show_vlc_text(self, sprite):
        vlc = self._compute_vlc(sprite)
        if vlc:
            txt = "VLC: " + ", ".join(str(v) for v in vlc)
            self.create_text(10, SPRITE_CANVAS_SIZE - 15, text=txt,
                             fill="#666688", font=("Courier", 9), anchor="w")

    def _compute_vlc(self, sprite):
        pts = sprite["points"]
        if len(pts) < 2:
            return None
        count = len(pts) - 1
        data = [count]
        for i in range(1, len(pts)):
            dy = pts[i][1] - pts[i - 1][1]
            dx = pts[i][0] - pts[i - 1][0]
            data.extend([clamp(dy), clamp(dx)])
        return data

    def _on_motion(self, event):
        vx, vy = self._vx(event.x, event.y)
        tool_str = f"  |  Tool: {self.tool}" if self.tool == "select" else ""
        sel_str = ""
        if self._drag_idx is not None and self.sprite:
            pt = self.sprite["points"][self._drag_idx]
            sel_str = f"  |  Point {self._drag_idx}: ({pt[0]}, {pt[1]})"
        self.app.update_status(f"Sprite: ({vx}, {vy}){tool_str}{sel_str}")

    def _on_click(self, event):
        self.focus_set()
        sprite = self.sprite
        if sprite is None:
            return

        if self.tool == "select":
            hit = self._hit_point(event.x, event.y)
            self._drag_idx = hit
            self._drag_bg = False
            if hit is not None:
                self.redraw()
                pt = sprite["points"][hit]
                self.app.update_status(
                    f"Sprite point {hit}: ({pt[0]}, {pt[1]})")
            elif sprite.get("bg_image") and self._bg_photo:
                # No point hit — grab background
                self._drag_bg = True
                self.redraw()
                bg = sprite["bg_image"]
                self.app.update_status(
                    f"Background: x={bg.get('x', 0)}, y={bg.get('y', 0)}")
            else:
                self.redraw()
        else:
            # Draw mode: add point
            vx, vy = self._vx(event.x, event.y)
            sprite["points"].append([vx, vy])
            self.redraw()

    def _on_drag(self, event):
        if self.tool == "select":
            sprite = self.sprite
            if not sprite:
                return
            vx, vy = self._vx(event.x, event.y)
            if self._drag_idx is not None and 0 <= self._drag_idx < len(sprite["points"]):
                sprite["points"][self._drag_idx] = [vx, vy]
                self.redraw()
                self.app.update_status(
                    f"Sprite point {self._drag_idx}: ({vx}, {vy})")
            elif self._drag_bg and sprite.get("bg_image"):
                sprite["bg_image"]["x"] = vx
                sprite["bg_image"]["y"] = vy
                self.redraw()
                self.app.update_status(f"Background: x={vx}, y={vy}")

    def _on_release(self, event):
        pass  # keep selection active after drag

    def _on_delete(self, event):
        if self._drag_idx is not None:
            sprite = self.sprite
            if sprite and 0 <= self._drag_idx < len(sprite["points"]):
                del sprite["points"][self._drag_idx]
                self._drag_idx = None
                self.redraw()

    def _on_right_click(self, event):
        """Remove last point."""
        sprite = self.sprite
        if sprite and sprite["points"]:
            sprite["points"].pop()
            self.redraw()


# ---------------------------------------------------------------------------
# Main Application
# ---------------------------------------------------------------------------

class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Vectrex H.E.R.O. Level & Sprite Editor")
        self.root.configure(bg="#1a1a2e")
        self.project = new_project()
        self._current_level_idx = 0
        self._current_sprite_idx = 0

        self._build_menu()
        self._build_ui()
        self.level_canvas.redraw()
        self.sprite_canvas.redraw()

    def current_level_data(self):
        idx = self._current_level_idx
        if 0 <= idx < len(self.project["levels"]):
            return self.project["levels"][idx]
        return None

    def current_sprite_data(self):
        idx = self._current_sprite_idx
        if 0 <= idx < len(self.project["sprites"]):
            return self.project["sprites"][idx]
        return None

    # ---- Menu ----

    def _build_menu(self):
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)

        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="New Project", command=self._new_project)
        file_menu.add_command(label="Open Project...", command=self._load_project)
        file_menu.add_command(label="Save Project", command=self._save_project)
        file_menu.add_command(label="Save Project As...", command=self._save_project_as)
        file_menu.add_separator()
        file_menu.add_command(label="Export .h Files...", command=self._export_headers)
        file_menu.add_separator()
        file_menu.add_command(label="Quit", command=self.root.quit)
        menubar.add_cascade(label="File", menu=file_menu)

        edit_menu = tk.Menu(menubar, tearoff=0)
        edit_menu.add_command(label="Delete Last Cave Polyline",
                              command=self._delete_last_polyline)
        edit_menu.add_command(label="Clear All Cave Lines",
                              command=self._clear_cave_lines)
        menubar.add_cascade(label="Edit", menu=edit_menu)


    # ---- UI ----

    def _build_ui(self):
        # Main notebook for tabs
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, padx=4, pady=4)

        # Level Editor tab
        level_frame = ttk.Frame(self.notebook)
        self.notebook.add(level_frame, text="Level Editor")
        self._build_level_tab(level_frame)

        # Sprite Editor tab
        sprite_frame = ttk.Frame(self.notebook)
        self.notebook.add(sprite_frame, text="Sprite Editor")
        self._build_sprite_tab(sprite_frame)

        # Emulator tab
        emu_frame = ttk.Frame(self.notebook)
        self.notebook.add(emu_frame, text="Emulator")
        self._build_emulator_tab(emu_frame)

        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = tk.Label(self.root, textvariable=self.status_var,
                              bg="#1a1a2e", fg="#888899", anchor="w",
                              font=("Courier", 10))
        status_bar.pack(fill="x", side="bottom", padx=4, pady=2)

    def _build_level_tab(self, parent):
        outer = ttk.Frame(parent)
        outer.pack(fill="both", expand=True)

        # -- Left panel: Level selector + Tools --
        left = ttk.Frame(outer, width=160)
        left.pack(side="left", fill="y", padx=(4, 0), pady=4)

        ttk.Label(left, text="Level", font=("Helvetica", 11, "bold")).pack(pady=(4, 4))
        self._level_combo = ttk.Combobox(left, state="readonly", width=14)
        self._level_combo.pack(padx=4, pady=2)
        self._level_combo.bind("<<ComboboxSelected>>", self._level_selected)
        self._refresh_level_combo()

        btn_frame = ttk.Frame(left)
        btn_frame.pack(fill="x", padx=4, pady=4)
        ttk.Button(btn_frame, text="+", width=3,
                   command=self._add_level).pack(side="left", padx=2)
        ttk.Button(btn_frame, text="-", width=3,
                   command=self._remove_level).pack(side="left", padx=2)
        ttk.Button(btn_frame, text="Rename", width=6,
                   command=self._rename_level).pack(side="left", padx=2)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)

        ttk.Label(left, text="Tools", font=("Helvetica", 11, "bold")).pack(pady=(0, 4))
        self._tool_var = tk.StringVar(value="select")
        tools = [
            ("Select/Move (S)", "select"),
            ("Cave Line (C)", "cave_line"),
            ("Wall (W)", "wall"),
            ("Enemy/Bat (E)", "enemy"),
            ("Miner (M)", "miner"),
            ("Player Start (P)", "player_start"),
        ]
        for label, val in tools:
            rb = ttk.Radiobutton(left, text=label, variable=self._tool_var,
                                 value=val, command=self._tool_changed)
            rb.pack(anchor="w", padx=4, pady=2)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)

        self._snap_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(left, text="Snap to Grid",
                         variable=self._snap_var,
                         command=self._snap_changed).pack(anchor="w", padx=4)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)
        ttk.Button(left, text="Run in Emulator",
                   command=self._run_level_test).pack(padx=4, pady=4, fill="x")

        # -- Middle: Canvas --
        canvas_frame = ttk.Frame(outer)
        canvas_frame.pack(side="left", fill="both", expand=True, padx=4, pady=4)
        self.level_canvas = LevelCanvas(canvas_frame, self)
        self.level_canvas.pack()

        # -- Right panel: Properties --
        right = ttk.Frame(outer, width=250)
        right.pack(side="right", fill="y", padx=(0, 4), pady=4)
        right.pack_propagate(False)

        # Cave constants
        ttk.Label(right, text="Cave Constants", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(4, 4))
        self._const_entries = {}
        const_frame = ttk.Frame(right)
        const_frame.pack(fill="x", padx=4)
        for i, key in enumerate(["CAVE_LEFT", "CAVE_RIGHT", "CAVE_TOP",
                                   "CAVE_FLOOR"]):
            ttk.Label(const_frame, text=key + ":", font=("Courier", 8)).grid(
                row=i, column=0, sticky="e", padx=2)
            ent = ttk.Entry(const_frame, width=7)
            ent.grid(row=i, column=1, padx=2, pady=1)
            self._const_entries[key] = ent
        ttk.Button(right, text="Apply Constants",
                   command=self._apply_constants).pack(padx=4, pady=4)
        self._load_constants_to_entries()

        ttk.Separator(right, orient="horizontal").pack(fill="x", pady=8)

        # Background image
        ttk.Label(right, text="Background", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(0, 4))
        ttk.Button(right, text="Load BG",
                   command=self._load_level_bg).pack(fill="x", padx=4, pady=2)
        ttk.Button(right, text="Clear BG",
                   command=self._clear_level_bg).pack(fill="x", padx=4, pady=2)
        ttk.Label(right, text="BG Scale:", font=("Courier", 8)).pack(
            anchor="w", padx=4, pady=(4, 0))
        self._level_bg_scale = tk.DoubleVar(value=1.0)
        ttk.Scale(right, from_=0.1, to=5.0,
                  variable=self._level_bg_scale, orient="horizontal",
                  command=self._on_level_bg_scale).pack(fill="x", padx=4)
        self._level_bg_scale_label = ttk.Label(right, text="1.0x", font=("Courier", 8))
        self._level_bg_scale_label.pack(anchor="w", padx=4)

        # Keyboard shortcuts
        self.root.bind("<Key>", self._on_key)

    def _build_sprite_tab(self, parent):
        outer = ttk.Frame(parent)
        outer.pack(fill="both", expand=True)

        # -- Left panel: Sprite selector + Tools --
        left = ttk.Frame(outer, width=160)
        left.pack(side="left", fill="y", padx=(4, 0), pady=4)

        ttk.Label(left, text="Sprite", font=("Helvetica", 11, "bold")).pack(pady=(4, 4))
        self._sprite_combo = ttk.Combobox(left, state="readonly", width=14)
        self._sprite_combo.pack(padx=4, pady=2)
        self._sprite_combo.bind("<<ComboboxSelected>>", self._sprite_selected)
        self._refresh_sprite_combo()

        btn_frame = ttk.Frame(left)
        btn_frame.pack(fill="x", padx=4, pady=4)
        ttk.Button(btn_frame, text="+", width=3,
                   command=self._add_sprite).pack(side="left", padx=2)
        ttk.Button(btn_frame, text="-", width=3,
                   command=self._remove_sprite).pack(side="left", padx=2)
        ttk.Button(btn_frame, text="Rename", width=6,
                   command=self._rename_sprite).pack(side="left", padx=2)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)

        ttk.Label(left, text="Tools", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(0, 4))
        self._sprite_tool_var = tk.StringVar(value="draw")
        for label, val in [("Draw Points (D)", "draw"), ("Select/Move (S)", "select")]:
            ttk.Radiobutton(left, text=label, variable=self._sprite_tool_var,
                            value=val, command=self._sprite_tool_changed).pack(
                anchor="w", padx=4, pady=2)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)

        ttk.Label(left, text="Draw: click to add.\nSelect: drag to move,\n  Del to remove.\nRight-click: undo.",
                  font=("Courier", 9), justify="left").pack(padx=4, anchor="w")

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)

        ttk.Button(left, text="Clear Points",
                   command=self._clear_sprite_points).pack(padx=4, pady=4)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)
        ttk.Button(left, text="Run in Emulator",
                   command=self._run_sprite_test).pack(padx=4, pady=4, fill="x")

        # -- Middle: Canvas --
        canvas_frame = ttk.Frame(outer)
        canvas_frame.pack(side="left", fill="both", expand=True, padx=4, pady=4)
        self.sprite_canvas = SpriteCanvas(canvas_frame, self)
        self.sprite_canvas.pack()

        # -- Right panel: Background + VLC --
        right = ttk.Frame(outer, width=250)
        right.pack(side="right", fill="y", padx=(0, 4), pady=4)
        right.pack_propagate(False)

        # Background image
        ttk.Label(right, text="Background", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(4, 4))
        ttk.Button(right, text="Load BG",
                   command=self._load_sprite_bg).pack(fill="x", padx=4, pady=2)
        ttk.Button(right, text="Clear BG",
                   command=self._clear_sprite_bg).pack(fill="x", padx=4, pady=2)
        ttk.Label(right, text="BG Scale:", font=("Courier", 8)).pack(
            anchor="w", padx=4, pady=(4, 0))
        self._sprite_bg_scale = tk.DoubleVar(value=1.0)
        ttk.Scale(right, from_=0.1, to=5.0,
                  variable=self._sprite_bg_scale, orient="horizontal",
                  command=self._on_sprite_bg_scale).pack(fill="x", padx=4)
        self._sprite_bg_scale_label = ttk.Label(right, text="1.0x", font=("Courier", 8))
        self._sprite_bg_scale_label.pack(anchor="w", padx=4)

        ttk.Separator(right, orient="horizontal").pack(fill="x", pady=8)

        # VLC data display
        ttk.Label(right, text="VLC Array", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(0, 4))
        self._vlc_text = tk.Text(right, width=22, height=14, bg="#0a0a1a", fg="#88aaff",
                                 font=("Courier", 9), state="disabled")
        self._vlc_text.pack(padx=4, fill="both", expand=True)

        # Update VLC text when switching to sprite tab
        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)

    def _build_emulator_tab(self, parent):
        self._emu = None

        outer = ttk.Frame(parent)
        outer.pack(fill="both", expand=True)

        # Left panel: controls
        left = ttk.Frame(outer, width=180)
        left.pack(side="left", fill="y", padx=(4, 0), pady=4)

        ttk.Label(left, text="ROM Path", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(8, 2))
        self._emu_rom_var = tk.StringVar(
            value=os.path.expanduser("~/retro-tools/vectrec/roms/romfast.bin"))
        rom_entry = ttk.Entry(left, textvariable=self._emu_rom_var, width=22)
        rom_entry.pack(padx=4, fill="x")
        ttk.Button(left, text="Browse...", command=self._emu_browse_rom).pack(
            padx=4, pady=(2, 8), anchor="w")

        ttk.Label(left, text="Cart Path", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(0, 2))
        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self._emu_cart_var = tk.StringVar(
            value=os.path.join(project_root, "bin", "main.bin"))
        cart_entry = ttk.Entry(left, textvariable=self._emu_cart_var, width=22)
        cart_entry.pack(padx=4, fill="x")
        ttk.Button(left, text="Browse...", command=self._emu_browse_cart).pack(
            padx=4, pady=(2, 8), anchor="w")

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=4)

        btn_frame = ttk.Frame(left)
        btn_frame.pack(fill="x", padx=4, pady=4)
        ttk.Button(btn_frame, text="Start", command=self._emu_start).pack(
            fill="x", pady=2)
        ttk.Button(btn_frame, text="Stop", command=self._emu_stop).pack(
            fill="x", pady=2)
        self._emu_pause_btn = ttk.Button(btn_frame, text="Pause", command=self._emu_pause)
        self._emu_pause_btn.pack(fill="x", pady=2)
        ttk.Button(btn_frame, text="Reset", command=self._emu_reset).pack(
            fill="x", pady=2)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=8)

        ttk.Label(left, text="Controls", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(0, 4))
        ttk.Label(left, text="Arrows: Move").pack(anchor="w", padx=8)
        ttk.Label(left, text="A/S/D/F: Btn 1-4").pack(anchor="w", padx=8)

        # Center: emulator canvas container
        self._emu_container = tk.Frame(outer, bg="black")
        self._emu_container.pack(side="left", fill="both", expand=True, padx=4, pady=4)

        # Right panel: CPU state
        right = ttk.Frame(outer, width=160)
        right.pack(side="left", fill="y", padx=(0, 4), pady=4)

        ttk.Label(right, text="CPU State", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(8, 4))

        self._emu_state_text = tk.Text(
            right, width=18, height=20, bg="#0a0a1a", fg="#88aaff",
            font=("Courier", 10), state="disabled", borderwidth=1, relief="sunken")
        self._emu_state_text.pack(padx=4, fill="both", expand=True)
        self._emu_state_timer = None

    def _emu_browse_rom(self):
        path = filedialog.askopenfilename(
            title="Select ROM", filetypes=[("BIN files", "*.bin"), ("All", "*.*")])
        if path:
            self._emu_rom_var.set(path)

    def _emu_browse_cart(self):
        path = filedialog.askopenfilename(
            title="Select Cart", filetypes=[("BIN files", "*.bin"), ("All", "*.*")])
        if path:
            self._emu_cart_var.set(path)

    def _emu_start(self):
        self._emu_stop()
        rom = self._emu_rom_var.get()
        cart = self._emu_cart_var.get()
        if not os.path.isfile(rom):
            messagebox.showerror("Error", f"ROM not found: {rom}")
            return
        if not cart or not os.path.isfile(cart):
            cart = None
        try:
            self._emu = Emulator(rom, cart, parent=self._emu_container)
            self._emu.start()
            self._emu_pause_btn.config(text="Pause")
            self._emu_state_update()
            self.update_status("Emulator running")
        except Exception as e:
            messagebox.showerror("Emulator Error", str(e))
            self._emu = None

    def _emu_stop(self):
        if self._emu_state_timer is not None:
            self.root.after_cancel(self._emu_state_timer)
            self._emu_state_timer = None
        if self._emu is not None:
            self._emu.stop()
            self._emu.canvas.destroy()
            self._emu = None
            self.update_status("Emulator stopped")

    def _emu_pause(self):
        if self._emu is None:
            return
        if self._emu._after_id is not None:
            self._emu.stop()
            self._emu_pause_btn.config(text="Resume")
            self.update_status("Emulator paused")
        else:
            self._emu._after_id = self._emu.root.after(EMU_TIMER, self._emu._tick)
            self._emu_pause_btn.config(text="Pause")
            self.update_status("Emulator running")

    def _emu_reset(self):
        if self._emu is not None:
            self._emu.reset()
            self.update_status("Emulator reset")

    def _emu_state_update(self):
        if self._emu is None:
            return
        state = self._emu.get_state()
        if state:
            cc = state["CC"]
            flags = "".join(c if cc & b else "." for c, b in
                            [("E",0x80),("F",0x40),("H",0x20),("I",0x10),
                             ("N",0x08),("Z",0x04),("V",0x02),("C",0x01)])
            lines = [
                f"PC  ${state['PC']:04X}",
                f"A   ${state['A']:02X}",
                f"B   ${state['B']:02X}",
                f"D   ${(state['A']<<8|state['B']):04X}",
                f"X   ${state['X']:04X}",
                f"Y   ${state['Y']:04X}",
                f"U   ${state['U']:04X}",
                f"S   ${state['S']:04X}",
                f"DP  ${state['DP']:02X}",
                f"CC  ${cc:02X} {flags}",
                "",
                f"Vectors {state['vectors']}",
            ]
            self._emu_state_text.config(state="normal")
            self._emu_state_text.delete("1.0", "end")
            self._emu_state_text.insert("1.0", "\n".join(lines))
            self._emu_state_text.config(state="disabled")
        self._emu_state_timer = self.root.after(200, self._emu_state_update)

    # ---- Status ----

    def update_status(self, text):
        self.status_var.set(text)

    # ---- Keyboard shortcuts ----

    def _on_key(self, event):
        key = event.char.lower()
        tab = self.notebook.index("current")
        if tab == 2:
            # Emulator tab — let emulator handle keys
            return
        if tab == 1:
            # Sprite tab
            sprite_mapping = {"d": "draw", "s": "select"}
            if key in sprite_mapping:
                self._sprite_tool_var.set(sprite_mapping[key])
                self._sprite_tool_changed()
            return
        # Level tab
        mapping = {"s": "select", "c": "cave_line", "w": "wall",
                   "e": "enemy", "m": "miner", "p": "player_start"}
        if key in mapping:
            self._tool_var.set(mapping[key])
            self._tool_changed()

    # ---- Tool changes ----

    def _tool_changed(self):
        tool = self._tool_var.get()
        self.level_canvas.tool = tool
        # Finish any in-progress polyline
        if tool != "cave_line" and self.level_canvas._polyline_pts:
            self.level_canvas._finish_polyline()
        self.update_status(f"Tool: {tool}")

    def _snap_changed(self):
        self.level_canvas.snap_enabled = self._snap_var.get()
        self.sprite_canvas.snap_enabled = self._snap_var.get()

    # ---- Level management ----

    def _refresh_level_combo(self):
        names = [lvl["name"] for lvl in self.project["levels"]]
        self._level_combo["values"] = names
        if names:
            self._level_combo.current(self._current_level_idx)

    def _level_selected(self, event=None):
        self._current_level_idx = self._level_combo.current()
        self._load_constants_to_entries()
        self._sync_level_bg_scale()
        self.level_canvas._bg_path_loaded = None  # force reload for new level
        self.level_canvas.redraw()

    def _sync_level_bg_scale(self):
        lvl = self.current_level_data()
        if lvl and lvl.get("bg_image"):
            s = lvl["bg_image"].get("scale", 1.0)
            self._level_bg_scale.set(s)
            self._level_bg_scale_label.config(text=f"{s:.1f}x")
        else:
            self._level_bg_scale.set(1.0)
            self._level_bg_scale_label.config(text="1.0x")

    def _add_level(self):
        n = len(self.project["levels"]) + 1
        lvl = new_level()
        lvl["name"] = f"Level {n}"
        self.project["levels"].append(lvl)
        self._current_level_idx = len(self.project["levels"]) - 1
        self._refresh_level_combo()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    def _remove_level(self):
        if len(self.project["levels"]) <= 1:
            messagebox.showinfo("Info", "Cannot remove the last level.")
            return
        del self.project["levels"][self._current_level_idx]
        self._current_level_idx = max(0, self._current_level_idx - 1)
        self._refresh_level_combo()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    def _rename_level(self):
        self._simple_rename_dialog(
            "Rename Level",
            self.project["levels"][self._current_level_idx]["name"],
            lambda name: self._do_rename_level(name))

    def _do_rename_level(self, name):
        self.project["levels"][self._current_level_idx]["name"] = name
        self._refresh_level_combo()

    # ---- Cave constants ----

    def _load_constants_to_entries(self):
        lvl = self.current_level_data()
        if lvl is None:
            return
        for key, ent in self._const_entries.items():
            ent.delete(0, tk.END)
            ent.insert(0, str(lvl["cave_constants"].get(key, 0)))

    def _apply_constants(self):
        lvl = self.current_level_data()
        if lvl is None:
            return
        try:
            for key, ent in self._const_entries.items():
                lvl["cave_constants"][key] = int(ent.get())
            self.level_canvas.redraw()
        except ValueError:
            messagebox.showerror("Invalid input", "Cave constants must be integers.")

    # ---- Edit menu ----

    def _delete_last_polyline(self):
        lvl = self.current_level_data()
        if lvl and lvl["cave_lines"]:
            lvl["cave_lines"].pop()
            self.level_canvas.redraw()

    def _clear_cave_lines(self):
        lvl = self.current_level_data()
        if lvl:
            lvl["cave_lines"].clear()
            self.level_canvas.redraw()

    # ---- Sprite management ----

    def _refresh_sprite_combo(self):
        names = [s["name"] for s in self.project["sprites"]]
        self._sprite_combo["values"] = names
        if names:
            self._sprite_combo.current(self._current_sprite_idx)

    def _sprite_selected(self, event=None):
        self._current_sprite_idx = self._sprite_combo.current()
        self._sync_sprite_bg_scale()
        self.sprite_canvas._bg_path_loaded = None  # force reload for new sprite
        self.sprite_canvas.redraw()
        self._update_vlc_text()

    def _sync_sprite_bg_scale(self):
        sprite = self.current_sprite_data()
        if sprite and sprite.get("bg_image"):
            s = sprite["bg_image"].get("scale", 1.0)
            self._sprite_bg_scale.set(s)
            self._sprite_bg_scale_label.config(text=f"{s:.1f}x")
        else:
            self._sprite_bg_scale.set(1.0)
            self._sprite_bg_scale_label.config(text="1.0x")

    def _add_sprite(self):
        s = new_sprite()
        n = len(self.project["sprites"]) + 1
        s["name"] = f"sprite_{n}"
        self.project["sprites"].append(s)
        self._current_sprite_idx = len(self.project["sprites"]) - 1
        self._refresh_sprite_combo()
        self.sprite_canvas.redraw()
        self._update_vlc_text()

    def _remove_sprite(self):
        if len(self.project["sprites"]) <= 1:
            messagebox.showinfo("Info", "Cannot remove the last sprite.")
            return
        del self.project["sprites"][self._current_sprite_idx]
        self._current_sprite_idx = max(0, self._current_sprite_idx - 1)
        self._refresh_sprite_combo()
        self.sprite_canvas.redraw()
        self._update_vlc_text()

    def _rename_sprite(self):
        self._simple_rename_dialog(
            "Rename Sprite",
            self.project["sprites"][self._current_sprite_idx]["name"],
            lambda name: self._do_rename_sprite(name))

    def _do_rename_sprite(self, name):
        self.project["sprites"][self._current_sprite_idx]["name"] = name
        self._refresh_sprite_combo()

    def _sprite_tool_changed(self):
        self.sprite_canvas.tool = self._sprite_tool_var.get()
        self.sprite_canvas._drag_idx = None
        self.sprite_canvas.redraw()

    # ---- Background image ----

    def _load_bg_image_for(self, target, canvas):
        """Prompt user to load a background image into target dict."""
        if not HAS_PIL:
            messagebox.showinfo("PIL Required",
                                "Install Pillow to use background images:\n  pip install Pillow")
            return
        path = filedialog.askopenfilename(
            filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp *.gif"), ("All files", "*.*")],
            title="Load Background Image",
        )
        if not path:
            return
        target["bg_image"] = {"path": path, "x": 0, "y": 0, "scale": 1.0}
        canvas._bg_path_loaded = None  # force reload
        canvas.redraw()

    def _load_level_bg(self):
        lvl = self.current_level_data()
        if lvl:
            self._load_bg_image_for(lvl, self.level_canvas)
            if lvl.get("bg_image"):
                self._level_bg_scale.set(lvl["bg_image"].get("scale", 1.0))
                self._level_bg_scale_label.config(text=f"{lvl['bg_image']['scale']:.1f}x")

    def _clear_level_bg(self):
        lvl = self.current_level_data()
        if lvl:
            lvl["bg_image"] = None
            self.level_canvas._bg_photo = None
            self.level_canvas._bg_path_loaded = None
            self.level_canvas.redraw()
            self._level_bg_scale.set(1.0)
            self._level_bg_scale_label.config(text="1.0x")

    def _on_level_bg_scale(self, val=None):
        lvl = self.current_level_data()
        s = round(self._level_bg_scale.get(), 1)
        self._level_bg_scale_label.config(text=f"{s:.1f}x")
        if lvl and lvl.get("bg_image"):
            lvl["bg_image"]["scale"] = s
            self.level_canvas.redraw()

    def _load_sprite_bg(self):
        sprite = self.current_sprite_data()
        if sprite:
            self._load_bg_image_for(sprite, self.sprite_canvas)
            if sprite.get("bg_image"):
                self._sprite_bg_scale.set(sprite["bg_image"].get("scale", 1.0))
                self._sprite_bg_scale_label.config(text=f"{sprite['bg_image']['scale']:.1f}x")

    def _clear_sprite_bg(self):
        sprite = self.current_sprite_data()
        if sprite:
            sprite["bg_image"] = None
            self.sprite_canvas._bg_photo = None
            self.sprite_canvas._bg_path_loaded = None
            self.sprite_canvas.redraw()
            self._sprite_bg_scale.set(1.0)
            self._sprite_bg_scale_label.config(text="1.0x")

    def _on_sprite_bg_scale(self, val=None):
        sprite = self.current_sprite_data()
        s = round(self._sprite_bg_scale.get(), 1)
        self._sprite_bg_scale_label.config(text=f"{s:.1f}x")
        if sprite and sprite.get("bg_image"):
            sprite["bg_image"]["scale"] = s
            self.sprite_canvas.redraw()

    def _clear_sprite_points(self):
        sprite = self.current_sprite_data()
        if sprite:
            sprite["points"].clear()
            self.sprite_canvas.redraw()
            self._update_vlc_text()

    def _run_sprite_test(self):
        sprite = self.current_sprite_data()
        if not sprite:
            messagebox.showerror("Error", "No sprite selected")
            return
        vlc = self.sprite_canvas._compute_vlc(sprite)
        if not vlc:
            messagebox.showerror("Error", "Sprite needs at least 2 points")
            return

        # Generate VLC array literal
        name = sprite["name"]
        vlc_lines = [f"    {vlc[0]},"]
        for i in range(1, len(vlc), 2):
            vlc_lines.append(f"    {vlc[i]:4d}, {vlc[i+1]:4d},")

        n_points = vlc[0]
        buf_size = n_points * 2

        src = f"""\
#include <vectrex.h>
#include <vectrex/stdlib.h>

#pragma vx_copyright "2026"
#pragma vx_title "SPRITE"
#pragma vx_music vx_music_1

static int8_t shape[] = {{
{chr(10).join(vlc_lines)}
}};

static int8_t rotbuf[{buf_size}];

int main(void) {{
    int8_t x = 0, y = 0;
    uint8_t scale = 0x60;
    int8_t angle = 0;

    set_beam_intensity(0x7F);
    set_scale(0x7F);
    stop_music();
    stop_sound();
    controller_enable_1_x();
    controller_enable_1_y();

    while (TRUE) {{
        wait_recal();
        controller_check_buttons();
        controller_check_joysticks();

        if (controller_joystick_1_left()) x -= 2;
        if (controller_joystick_1_right()) x += 2;
        if (controller_joystick_1_up()) y += 2;
        if (controller_joystick_1_down()) y -= 2;

        if (controller_button_1_1_held() && scale < 0xFF) scale += 1;
        if (controller_button_1_2_held() && scale > 0x10) scale -= 1;
        if (controller_button_1_3_held()) angle += 2;
        if (controller_button_1_4_held()) angle -= 2;

        zero_beam();
        intensity_a(0x7F);
        set_scale(0x7F);
        moveto_d(y, x);

        if (angle != 0) {{
            rot_vl_ab(angle, shape[0], &shape[1], rotbuf);
            set_scale(scale);
            draw_vl_a(shape[0], rotbuf);
        }} else {{
            set_scale(scale);
            draw_vlc(shape);
        }}
    }}

    return 0;
}}
"""
        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        test_dir = os.path.join(project_root, "test_sprite")
        os.makedirs(test_dir, exist_ok=True)
        src_path = os.path.join(test_dir, "sprite_test.c")
        bin_path = os.path.join(test_dir, "sprite_test.bin")

        with open(src_path, "w") as f:
            f.write(src)

        # Build with CMOC
        vectrec = os.path.expanduser("~/retro-tools/vectrec")
        cmoc = os.path.join(vectrec, "cmoc")
        stdlib = os.path.join(vectrec, "stdlib")

        if os.path.isfile(bin_path):
            os.remove(bin_path)
        self.update_status("Building sprite test...")
        self.root.update_idletasks()
        try:
            result = subprocess.run(
                [cmoc, f"-I{stdlib}", f"-L{stdlib}", "--vectrex",
                 "-o", bin_path, src_path],
                cwd=project_root, capture_output=True, text=True, timeout=30)
        except Exception as e:
            messagebox.showerror("Build Error", str(e))
            return
        if not os.path.isfile(bin_path):
            messagebox.showerror("Build Failed", result.stderr or result.stdout)
            self.update_status("Build failed")
            return

        # Run in emulator
        rom = self._emu_rom_var.get()
        if not os.path.isfile(rom):
            messagebox.showerror("Error", f"ROM not found: {rom}")
            return
        self._emu_stop()
        try:
            self._emu_cart_var.set(bin_path)
            self._emu = Emulator(rom, bin_path, parent=self._emu_container)
            self._emu.start()
            self._emu_pause_btn.config(text="Pause")
            self._emu_state_update()
            self.notebook.select(2)
            self.update_status(f"Running sprite test: {name}")
        except Exception as e:
            messagebox.showerror("Emulator Error", str(e))
            self._emu = None

    def _run_level_test(self):
        import re

        lvl = self.current_level_data()
        if not lvl:
            messagebox.showerror("Error", "No level selected")
            return

        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        src_dir = os.path.join(project_root, "src")
        test_dir = os.path.join(project_root, "test_level")
        os.makedirs(test_dir, exist_ok=True)

        # Generate test_level/hero.h with cave constants from current level.
        # Use wide-open collision boundaries so cave-boundary physics
        # effectively never trigger — cave lines are visual-only for now.
        consts = dict(lvl["cave_constants"])
        cave_lines = lvl.get("cave_lines", [])
        if cave_lines:
            consts["CAVE_LEFT"] = -128
            consts["CAVE_RIGHT"] = 127
            consts["CAVE_TOP"] = 127
            consts["CAVE_FLOOR"] = -128
            consts["SHAFT_LEFT"] = -128
            consts["SHAFT_RIGHT"] = 127
            consts["LEDGE_Y"] = -128

        with open(os.path.join(src_dir, "hero.h"), "r") as f:
            hero_h = f.read()
        for key in ["CAVE_LEFT", "CAVE_RIGHT", "CAVE_TOP", "CAVE_FLOOR",
                     "LEDGE_Y", "SHAFT_LEFT", "SHAFT_RIGHT"]:
            if key in consts:
                hero_h = re.sub(
                    rf"#define\s+{key}\s+[-\d]+",
                    f"#define {key}   {consts[key]}",
                    hero_h)
        with open(os.path.join(test_dir, "hero.h"), "w") as f:
            f.write(hero_h)

        # Extract level data used by multiple generators below
        miner = lvl.get("miner")

        # Generate wrapper .c files so src/ code picks up test_level/hero.h.
        # The #include "hero.h" in the wrapper finds test_level/hero.h (same dir),
        # setting the HERO_H include guard. When the src file's own #include "hero.h"
        # is reached, the guard prevents src/hero.h from being loaded.
        # enemies.c: override update_enemies with cave segment collision
        if cave_lines:
            segments = []
            for polyline in cave_lines:
                if len(polyline) < 2:
                    continue
                for j in range(1, len(polyline)):
                    x1, y1 = int(polyline[j-1][0]), int(polyline[j-1][1])
                    x2, y2 = int(polyline[j][0]), int(polyline[j][1])
                    segments.append((x1, y1, x2, y2))

            ec = ['#include "hero.h"', '',
                  '#define update_enemies _orig_update_enemies',
                  '#define update_dynamite _orig_update_dynamite',
                  '#include "../src/enemies.c"',
                  '#undef update_enemies',
                  '#undef update_dynamite', '',
                  f'#define CAVE_SEG_COUNT {len(segments)}',
                  'static const int8_t cave_segments[] = {']
            for s in segments:
                ec.append(f'    {s[0]}, {s[1]}, {s[2]}, {s[3]},')
            ec.extend(['};', '',
                'void update_enemies(void) {',
                '    uint8_t i, j;',
                '    int8_t x1, y1, x2, y2, seg_min, seg_max;',
                '',
                '    _orig_update_enemies();',
                '',
                '    // Bounce bats off vertical cave segments',
                '    for (i = 0; i < enemy_count; i++) {',
                '        if (!enemies[i].alive) continue;',
                '        for (j = 0; j < CAVE_SEG_COUNT; j++) {',
                '            x1 = cave_segments[j * 4];',
                '            y1 = cave_segments[j * 4 + 1];',
                '            x2 = cave_segments[j * 4 + 2];',
                '            y2 = cave_segments[j * 4 + 3];',
                '            if (x1 != x2) continue;',
                '            seg_min = y1 < y2 ? y1 : y2;',
                '            seg_max = y1 > y2 ? y1 : y2;',
                '            if (enemies[i].y + BAT_HH > seg_min &&',
                '                enemies[i].y - BAT_HH < seg_max) {',
                '                if (enemies[i].x + BAT_HW > x1 &&',
                '                    enemies[i].x < x1) {',
                '                    enemies[i].x = x1 - BAT_HW;',
                '                    enemies[i].vx = -enemies[i].vx;',
                '                } else if (enemies[i].x - BAT_HW < x1 &&',
                '                           enemies[i].x > x1) {',
                '                    enemies[i].x = x1 + BAT_HW;',
                '                    enemies[i].vx = -enemies[i].vx;',
                '                }',
                '            }',
                '        }',
                '    }',
                '}', ''])
            # update_dynamite: all walls destroyable (start at 0, not 3)
            ec.extend([
                'void update_dynamite(void) {',
                '    uint8_t i;',
                '    int8_t wcx, wcy;',
                '    if (dyn_active && !dyn_exploding) {',
                '        dyn_timer--;',
                '        if (dyn_timer == 0) {',
                '            dyn_exploding = 1;',
                '            dyn_expl_timer = EXPLOSION_TIME;',
                '        }',
                '        return;',
                '    }',
                '    if (dyn_exploding) {',
                '        dyn_expl_timer--;',
                '        if (dyn_expl_timer == EXPLOSION_TIME - 1) {',
                '            for (i = 0; i < cur_wall_count; i++) {',
                '                if (walls_destroyed & (1 << i)) continue;',
                '                wcx = wall_x(i) + (wall_w(i) / 2);',
                '                wcy = wall_y(i);',
                '                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,',
                '                                wcx, wcy, wall_w(i) / 2, wall_h(i))) {',
                '                    walls_destroyed = walls_destroyed | (uint8_t)(1 << i);',
                '                    score += 75;',
                '                }',
                '            }',
                '            for (i = 0; i < enemy_count; i++) {',
                '                if (!enemies[i].alive) continue;',
                '                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,',
                '                                enemies[i].x, enemies[i].y, BAT_HW, BAT_HH)) {',
                '                    enemies[i].alive = 0;',
                '                    score += 50;',
                '                }',
                '            }',
                '            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,',
                '                            player_x, player_y, PLAYER_HW, PLAYER_HH)) {',
                '                game_state = STATE_DYING;',
                '                death_timer = 30;',
                '            }',
                '        }',
                '        if (dyn_expl_timer == 0) {',
                '            dyn_active = 0;',
                '            dyn_exploding = 0;',
                '        }',
                '    }',
                '}'])
            with open(os.path.join(test_dir, "enemies.c"), "w") as f:
                f.write('\n'.join(ec) + '\n')
        else:
            ec = ['#include "hero.h"', '',
                  '#define update_dynamite _orig_update_dynamite',
                  '#include "../src/enemies.c"',
                  '#undef update_dynamite', '',
                  'void update_dynamite(void) {',
                  '    uint8_t i;',
                  '    int8_t wcx, wcy;',
                  '    if (dyn_active && !dyn_exploding) {',
                  '        dyn_timer--;',
                  '        if (dyn_timer == 0) {',
                  '            dyn_exploding = 1;',
                  '            dyn_expl_timer = EXPLOSION_TIME;',
                  '        }',
                  '        return;',
                  '    }',
                  '    if (dyn_exploding) {',
                  '        dyn_expl_timer--;',
                  '        if (dyn_expl_timer == EXPLOSION_TIME - 1) {',
                  '            for (i = 0; i < cur_wall_count; i++) {',
                  '                if (walls_destroyed & (1 << i)) continue;',
                  '                wcx = wall_x(i) + (wall_w(i) / 2);',
                  '                wcy = wall_y(i);',
                  '                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,',
                  '                                wcx, wcy, wall_w(i) / 2, wall_h(i))) {',
                  '                    walls_destroyed = walls_destroyed | (uint8_t)(1 << i);',
                  '                    score += 75;',
                  '                }',
                  '            }',
                  '            for (i = 0; i < enemy_count; i++) {',
                  '                if (!enemies[i].alive) continue;',
                  '                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,',
                  '                                enemies[i].x, enemies[i].y, BAT_HW, BAT_HH)) {',
                  '                    enemies[i].alive = 0;',
                  '                    score += 50;',
                  '                }',
                  '            }',
                  '            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,',
                  '                            player_x, player_y, PLAYER_HW, PLAYER_HH)) {',
                  '                game_state = STATE_DYING;',
                  '                death_timer = 30;',
                  '            }',
                  '        }',
                  '        if (dyn_expl_timer == 0) {',
                  '            dyn_active = 0;',
                  '            dyn_exploding = 0;',
                  '        }',
                  '    }',
                  '}']
            with open(os.path.join(test_dir, "enemies.c"), "w") as f:
                f.write('\n'.join(ec) + '\n')

        # player.c: override update_player_physics with cave segment collision
        if cave_lines:
            # Extract line segments from polylines
            segments = []
            for polyline in cave_lines:
                if len(polyline) < 2:
                    continue
                for j in range(1, len(polyline)):
                    x1, y1 = int(polyline[j-1][0]), int(polyline[j-1][1])
                    x2, y2 = int(polyline[j][0]), int(polyline[j][1])
                    segments.append((x1, y1, x2, y2))

            pc = ['#include "hero.h"', '',
                  '#define update_player_physics _orig_update_player_physics',
                  '#include "../src/player.c"',
                  '#undef update_player_physics', '',
                  f'#define CAVE_SEG_COUNT {len(segments)}',
                  'static const int8_t cave_segments[] = {']
            for s in segments:
                pc.append(f'    {s[0]}, {s[1]}, {s[2]}, {s[3]},')
            pc.extend(['};', '',
                'void update_player_physics(void) {',
                '    uint8_t i;',
                '    int8_t x1, y1, x2, y2, seg_min, seg_max;',
                '',
                '    _orig_update_player_physics();',
                '',
                '    for (i = 0; i < CAVE_SEG_COUNT; i++) {',
                '        x1 = cave_segments[i * 4];',
                '        y1 = cave_segments[i * 4 + 1];',
                '        x2 = cave_segments[i * 4 + 2];',
                '        y2 = cave_segments[i * 4 + 3];',
                '',
                '        if (y1 == y2) {',
                '            // Horizontal segment: floor or ceiling',
                '            seg_min = x1 < x2 ? x1 : x2;',
                '            seg_max = x1 > x2 ? x1 : x2;',
                '            if (player_x + PLAYER_HW > seg_min &&',
                '                player_x - PLAYER_HW < seg_max) {',
                '                if (player_y > y1 &&',
                '                    player_y - PLAYER_HH < y1) {',
                '                    player_y = y1 + PLAYER_HH;',
                '                    player_vy = 0;',
                '                    player_on_ground = 1;',
                '                } else if (player_y < y1 &&',
                '                           player_y + PLAYER_HH > y1) {',
                '                    player_y = y1 - PLAYER_HH;',
                '                    player_vy = 0;',
                '                }',
                '            }',
                '        } else if (x1 == x2) {',
                '            // Vertical segment: side wall',
                '            seg_min = y1 < y2 ? y1 : y2;',
                '            seg_max = y1 > y2 ? y1 : y2;',
                '            if (player_y + PLAYER_HH > seg_min &&',
                '                player_y - PLAYER_HH < seg_max) {',
                '                if (player_x < x1 &&',
                '                    player_x + PLAYER_HW > x1) {',
                '                    player_x = x1 - PLAYER_HW;',
                '                    player_vx = 0;',
                '                } else if (player_x > x1 &&',
                '                           player_x - PLAYER_HW < x1) {',
                '                    player_x = x1 + PLAYER_HW;',
                '                    player_vx = 0;',
                '                }',
                '            }',
                '        }',
                '    }',
                '}'])
            with open(os.path.join(test_dir, "player.c"), "w") as f:
                f.write('\n'.join(pc) + '\n')
        else:
            wrapper = '#include "hero.h"\n#include "../src/player.c"\n'
            with open(os.path.join(test_dir, "player.c"), "w") as f:
                f.write(wrapper)

        # Generate custom drawing.c: override draw_cave() to draw
        # editor cave lines + wall rectangles, override draw_miner()
        # to be a no-op when no miner was placed.
        dc = ['#include "hero.h"', '',
              '// Rename originals so we can override them',
              '#define draw_cave _orig_draw_cave',
              '#define draw_miner _orig_draw_miner',
              '#include "../src/drawing.c"',
              '#undef draw_cave',
              '#undef draw_miner', '']

        # Custom draw_cave: editor cave lines + wall rectangles
        dc.append('void draw_cave(void) {')
        dc.append('    uint8_t i;')
        dc.append('    uint8_t cave_int = dyn_exploding ? INTENSITY_BRIGHT : INTENSITY_DIM;')
        cave_lines = lvl.get("cave_lines", [])
        for polyline in cave_lines:
            if len(polyline) < 2:
                continue
            dc.append('    zero_beam();')
            dc.append('    intensity_a(cave_int);')
            dc.append('    set_scale(0x7F);')
            x0, y0 = polyline[0]
            dc.append(f'    moveto_d({int(y0)}, {int(x0)});')
            for j in range(1, len(polyline)):
                dy = int(polyline[j][1] - polyline[j - 1][1])
                dx = int(polyline[j][0] - polyline[j - 1][0])
                # draw_line_d takes int8_t (-128..127); split long segments
                while dy != 0 or dx != 0:
                    sy = max(-128, min(127, dy))
                    sx = max(-128, min(127, dx))
                    dc.append(f'    draw_line_d({sy}, {sx});')
                    dy -= sy
                    dx -= sx
        # Draw all walls as visible rectangles
        dc.append('    for (i = 0; i < cur_wall_count; i++) {')
        dc.append('        if (walls_destroyed & (1 << i)) continue;')
        dc.append('        zero_beam();')
        dc.append('        intensity_a(cave_int);')
        dc.append('        set_scale(0x7F);')
        dc.append('        moveto_d(wall_y(i) + wall_h(i), wall_x(i));')
        dc.append('        draw_line_d(0, wall_w(i));')
        dc.append('        draw_line_d(-wall_h(i) * 2, 0);')
        dc.append('        draw_line_d(0, -wall_w(i));')
        dc.append('        draw_line_d(wall_h(i) * 2, 0);')
        dc.append('    }')
        dc.append('}')
        dc.append('')

        # Custom draw_miner: only if user placed one
        if miner:
            dc.append('void draw_miner(void) { _orig_draw_miner(); }')
        else:
            dc.append('void draw_miner(void) {}')
        dc.append('')

        with open(os.path.join(test_dir, "drawing.c"), "w") as f:
            f.write('\n'.join(dc) + '\n')

        # Generate test_level/levels.h with only the current level
        walls = lvl["walls"]
        enemies = lvl["enemies"]
        ps = lvl.get("player_start")
        lh = ["// Generated by level_editor.py — test level", "",
              "#ifndef LEVELS_H", "#define LEVELS_H", "#include \"hero.h\"", ""]
        lh.append(f"#define L1_WALL_COUNT {len(walls)}")
        if walls:
            lh.append("static const int8_t l1_walls[] = {")
            for w in walls:
                lh.append(f"    {w['y']}, {w['x']}, {w['h']}, {w['w']},")
            lh.append("};")
        else:
            lh.append("static const int8_t l1_walls[] = { 0, 0, 0, 0 };")
        lh.append(f"#define L1_ENEMY_COUNT {len(enemies)}")
        if enemies:
            lh.append("static const int8_t l1_enemies[] = {")
            for e in enemies:
                lh.append(f"    {e['x']}, {e['y']}, {e['vx']},")
            lh.append("};")
        else:
            lh.append("static const int8_t l1_enemies[] = { 0, 0, 0 };")
        start_x = ps["x"] if ps else consts.get("CAVE_LEFT", -90) + 15
        start_y = ps["y"] if ps else consts.get("CAVE_TOP", 105) - 2
        miner_x = miner["x"] if miner else 0
        miner_y = miner["y"] if miner else consts.get("CAVE_FLOOR", -95) + 8
        lh.append(f"#define L1_START_X  {start_x}")
        lh.append(f"#define L1_START_Y  {start_y}")
        lh.append(f"#define L1_MINER_X  {miner_x}")
        lh.append(f"#define L1_MINER_Y  {miner_y}")
        lh.extend(["", "#endif", ""])
        with open(os.path.join(test_dir, "levels.h"), "w") as f:
            f.write("\n".join(lh))

        # Generate test_level/levels.c
        lc = """\
#include "hero.h"
#include "levels.h"

void set_level_data(void) {
    cur_walls = l1_walls;
    cur_wall_count = L1_WALL_COUNT;
    cur_enemies_data = l1_enemies;
    cur_enemy_count = L1_ENEMY_COUNT;
    cur_miner_x = L1_MINER_X;
    cur_miner_y = L1_MINER_Y;
}

void load_enemies(void) {
    uint8_t i;
    enemy_count = cur_enemy_count;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (i < enemy_count) {
            enemies[i].x = cur_enemies_data[i * 3];
            enemies[i].y = cur_enemies_data[i * 3 + 1];
            enemies[i].vx = cur_enemies_data[i * 3 + 2];
            enemies[i].alive = 1;
            enemies[i].anim = 0;
        } else {
            enemies[i].alive = 0;
        }
    }
    walls_destroyed = 0;
}

void init_level(void) {
    set_level_data();
    player_x = L1_START_X;
    player_y = L1_START_Y;
    player_vx = 0;
    player_vy = 0;
    player_facing = 1;
    player_on_ground = 0;
    player_thrusting = 0;
    laser_active = 0;
    dyn_active = 0;
    dyn_exploding = 0;
    anim_tick = 0;
    load_enemies();
    level_msg_timer = 30;
}

void start_new_game(void) {
    score = 0;
    player_lives = START_LIVES;
    player_fuel = START_FUEL;
    player_dynamite = START_DYNAMITE;
    current_level = 0;
    init_level();
    game_state = STATE_PLAYING;
}
"""
        with open(os.path.join(test_dir, "levels.c"), "w") as f:
            f.write(lc)

        # Generate test_level/main.c — starts directly in STATE_PLAYING
        mc_miner_rescue = "            check_miner_rescue();\n" if miner else ""
        mc_draw_miner = "            draw_miner();\n" if miner else ""
        mc = f"""\
#include "hero.h"

#pragma vx_copyright "2026"
#pragma vx_title "LEVEL TEST"
#pragma vx_music vx_music_1

int8_t player_x, player_y, player_vx, player_vy, player_facing;
uint8_t player_fuel, player_dynamite, player_lives;
uint8_t player_on_ground, player_thrusting, anim_tick;
int score;
uint8_t game_state, current_level, death_timer, level_msg_timer;
Enemy enemies[MAX_ENEMIES];
uint8_t enemy_count;
uint8_t laser_active;
int8_t laser_x, laser_y, laser_dir;
uint8_t laser_timer;
uint8_t dyn_active;
int8_t dyn_x, dyn_y;
uint8_t dyn_timer, dyn_exploding, dyn_expl_timer;
uint8_t walls_destroyed;
const int8_t *cur_walls;
uint8_t cur_wall_count;
const int8_t *cur_enemies_data;
uint8_t cur_enemy_count;
int8_t cur_miner_x, cur_miner_y;
char str_buf[16];

uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh) {{
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}}

int8_t wall_y(uint8_t i)  {{ return cur_walls[i * 4]; }}
int8_t wall_x(uint8_t i)  {{ return cur_walls[i * 4 + 1]; }}
int8_t wall_h(uint8_t i)  {{ return cur_walls[i * 4 + 2]; }}
int8_t wall_w(uint8_t i)  {{ return cur_walls[i * 4 + 3]; }}

uint8_t player_hits_wall(uint8_t i) {{
    int8_t wcx = wall_x(i) + (wall_w(i) / 2);
    int8_t wcy = wall_y(i);
    int8_t whw = wall_w(i) / 2;
    int8_t whh = wall_h(i);
    return box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                       wcx, wcy, whw, whh);
}}

void vectrex_init(void) {{
    set_beam_intensity(INTENSITY_NORMAL);
    set_scale(0x7F);
    stop_music();
    stop_sound();
    controller_enable_1_x();
    controller_enable_1_y();
}}

int main(void) {{
    vectrex_init();
    start_new_game();

    while (TRUE) {{
        wait_recal();
        intensity_a(INTENSITY_NORMAL);
        controller_check_buttons();

        if (game_state == STATE_PLAYING) {{
            anim_tick++;
            handle_input();
            update_player_physics();
            update_laser();
            update_dynamite();
            update_enemies();
{mc_miner_rescue}
            draw_cave();
            draw_enemies();
            draw_dynamite_and_explosion();
            draw_laser_beam();
            draw_player();
{mc_draw_miner}            draw_hud();

            if (level_msg_timer > 0) {{
                zero_beam();
                sprintf(str_buf, "TEST");
                print_str_c(0, -30, str_buf);
                level_msg_timer--;
            }}
        }}
        else if (game_state == STATE_DYING) {{
            death_timer--;
            if (death_timer & 2) draw_player();
            draw_cave();
            if (death_timer == 0) {{
                player_lives--;
                if (player_lives == 0) {{
                    start_new_game();
                }} else {{
                    init_level();
                    game_state = STATE_PLAYING;
                }}
            }}
        }}
        else if (game_state == STATE_LEVEL_COMPLETE) {{
            draw_cave();
            zero_beam();
            set_scale(0x7F);
            print_str_c(0, -70, "RESCUED!");
            level_msg_timer--;
            if (level_msg_timer == 0) {{
                init_level();
                game_state = STATE_PLAYING;
            }}
        }}
        else if (game_state == STATE_GAME_OVER) {{
            start_new_game();
        }}
    }}
    return 0;
}}
"""
        with open(os.path.join(test_dir, "main.c"), "w") as f:
            f.write(mc)

        # Build
        vectrec = os.path.expanduser("~/retro-tools/vectrec")
        cmoc = os.path.join(vectrec, "cmoc")
        stdlib = os.path.join(vectrec, "stdlib")
        bin_path = os.path.join(test_dir, "test.bin")
        cmd = [cmoc, f"-I{test_dir}", f"-I{src_dir}",
               f"-I{stdlib}", f"-L{stdlib}", "--vectrex",
               "-o", bin_path,
               os.path.join(test_dir, "main.c"),
               os.path.join(test_dir, "player.c"),
               os.path.join(test_dir, "enemies.c"),
               os.path.join(test_dir, "drawing.c"),
               os.path.join(test_dir, "levels.c")]

        if os.path.isfile(bin_path):
            os.remove(bin_path)
        self.update_status("Building level test...")
        self.root.update_idletasks()
        try:
            result = subprocess.run(
                cmd, cwd=project_root,
                capture_output=True, text=True, timeout=30)
        except Exception as e:
            messagebox.showerror("Build Error", str(e))
            return
        if not os.path.isfile(bin_path):
            messagebox.showerror("Build Failed", result.stderr or result.stdout)
            self.update_status("Build failed")
            return

        # Run in emulator
        rom = self._emu_rom_var.get()
        if not os.path.isfile(rom):
            messagebox.showerror("Error", f"ROM not found: {rom}")
            return
        self._emu_stop()
        try:
            self._emu_cart_var.set(bin_path)
            self._emu = Emulator(rom, bin_path, parent=self._emu_container)
            self._emu.start()
            self._emu_pause_btn.config(text="Pause")
            self._emu_state_update()
            self.notebook.select(2)
            self.update_status(f"Running level test: {lvl['name']}")
        except Exception as e:
            messagebox.showerror("Emulator Error", str(e))
            self._emu = None

    def _update_vlc_text(self):
        sprite = self.current_sprite_data()
        self._vlc_text.config(state="normal")
        self._vlc_text.delete("1.0", "end")
        if sprite:
            vlc = self.sprite_canvas._compute_vlc(sprite)
            if vlc:
                name = sprite["name"]
                lines = [f"static int8_t {name}[] = {{"]
                lines.append(f"    {vlc[0]},")
                for i in range(1, len(vlc), 2):
                    dy, dx = vlc[i], vlc[i + 1]
                    lines.append(f"    {dy:4d}, {dx:4d},")
                lines.append("};")
                self._vlc_text.insert("1.0", "\n".join(lines))
        self._vlc_text.config(state="disabled")

    def _on_tab_changed(self, event=None):
        idx = self.notebook.index("current")
        if idx == 0:
            self.level_canvas.redraw()
        elif idx == 1:
            self.sprite_canvas.redraw()
            self._update_vlc_text()

    # ---- Shared dialogs ----

    def _simple_rename_dialog(self, title, current, callback):
        dlg = tk.Toplevel(self.root)
        dlg.title(title)
        dlg.transient(self.root)
        dlg.grab_set()
        tk.Label(dlg, text="Name:").pack(padx=8, pady=(8, 2))
        ent = tk.Entry(dlg, width=24)
        ent.insert(0, current)
        ent.pack(padx=8, pady=2)
        ent.select_range(0, tk.END)
        ent.focus_set()

        def apply():
            callback(ent.get())
            dlg.destroy()

        tk.Button(dlg, text="OK", command=apply).pack(pady=8)
        dlg.bind("<Return>", lambda e: apply())

    # ---- File operations ----

    def _project_path(self):
        return getattr(self, '_save_path', None)

    def _new_project(self):
        self.project = new_project()
        self._current_level_idx = 0
        self._current_sprite_idx = 0
        self._save_path = None
        self._refresh_level_combo()
        self._refresh_sprite_combo()
        self._load_constants_to_entries()
        self.level_canvas.redraw()
        self.sprite_canvas.redraw()

    def _save_project(self):
        path = self._project_path()
        if path:
            self._do_save(path)
        else:
            self._save_project_as()

    def _save_project_as(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
            title="Save Project",
        )
        if path:
            self._save_path = path
            self._do_save(path)

    def _do_save(self, path):
        with open(path, "w") as f:
            json.dump(self.project, f, indent=2)
        self.update_status(f"Saved: {path}")

    def _load_project(self):
        path = filedialog.askopenfilename(
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")],
            title="Open Project",
        )
        if path:
            self._load_project_file(path)

    def _load_project_file(self, path):
        try:
            with open(path, "r") as f:
                data = json.load(f)
            self.project = data
            self._save_path = path
            self._current_level_idx = 0
            self._current_sprite_idx = 0
            self._refresh_level_combo()
            self._refresh_sprite_combo()
            self._load_constants_to_entries()
            self.level_canvas.redraw()
            self.sprite_canvas.redraw()
            self.update_status(f"Loaded: {path}")
        except Exception as e:
            messagebox.showerror("Load Error", str(e))

    # ---- Export ----

    def _export_headers(self):
        folder = filedialog.askdirectory(title="Export .h files to folder")
        if not folder:
            return
        try:
            self._export_levels_h(os.path.join(folder, "levels.h"))
            self._export_sprites_h(os.path.join(folder, "sprites.h"))
            self.update_status(f"Exported to {folder}/")
            messagebox.showinfo("Export Complete",
                                f"Exported:\n  {folder}/levels.h\n  {folder}/sprites.h")
        except Exception as e:
            messagebox.showerror("Export Error", str(e))

    def _export_levels_h(self, path):
        lines = [
            "// Generated by level_editor.py",
            "// Vectrex H.E.R.O. level data",
            "",
        ]
        for i, lvl in enumerate(self.project["levels"]):
            prefix = f"l{i + 1}"
            PREFIX = f"L{i + 1}"
            consts = lvl["cave_constants"]
            lines.append(f"// {'=' * 60}")
            lines.append(f"// {lvl['name']}")
            lines.append(f"// {'=' * 60}")
            lines.append("")

            # Cave constants
            for key in ["CAVE_LEFT", "CAVE_RIGHT", "CAVE_TOP", "CAVE_FLOOR"]:
                val = consts.get(key, 0)
                lines.append(f"#define {PREFIX}_{key}  {val}")
            lines.append("")

            # Walls
            walls = lvl["walls"]
            lines.append(f"#define {PREFIX}_WALL_COUNT {len(walls)}")
            if walls:
                lines.append(f"static const int8_t {prefix}_walls[] = {{")
                for j, w in enumerate(walls):
                    destr = " (destroyable)" if w.get("destroyable", False) else ""
                    lines.append(f"    {w['y']}, {w['x']}, {w['h']}, {w['w']},"
                                 f"   // wall {j}{destr}")
                lines.append("};")
            lines.append("")

            # Enemies
            enemies = lvl["enemies"]
            lines.append(f"#define {PREFIX}_ENEMY_COUNT {len(enemies)}")
            if enemies:
                lines.append(f"static const int8_t {prefix}_enemies[] = {{")
                for j, e in enumerate(enemies):
                    lines.append(f"    {e['x']}, {e['y']}, {e['vx']},   // enemy {j}")
                lines.append("};")
            lines.append("")

            # Player start
            ps = lvl["player_start"]
            if ps:
                lines.append(f"#define {PREFIX}_START_X  {ps['x']}")
                lines.append(f"#define {PREFIX}_START_Y  {ps['y']}")
                lines.append("")

            # Miner
            m = lvl["miner"]
            if m:
                lines.append(f"#define {PREFIX}_MINER_X  {m['x']}")
                lines.append(f"#define {PREFIX}_MINER_Y  {m['y']}")
                lines.append("")

            # Cave lines as comments (for reference)
            if lvl["cave_lines"]:
                lines.append(f"// Cave outline polylines for {lvl['name']}:")
                for j, poly in enumerate(lvl["cave_lines"]):
                    pts_str = " -> ".join(f"({p[0]},{p[1]})" for p in poly)
                    lines.append(f"//   path {j}: {pts_str}")
                lines.append("")

        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")

    # Default sprite data for required game shapes
    def _export_sprites_h(self, path):
        lines = [
            "// Generated by level_editor.py",
            "// Vectrex H.E.R.O. VLC sprite data",
            "",
        ]
        for sprite in self.project["sprites"]:
            pts = sprite["points"]
            if len(pts) < 2:
                continue
            name = sprite["name"]
            count = len(pts) - 1
            lines.append(f"static int8_t {name}[] = {{")
            lines.append(f"    {count},")
            for j in range(1, len(pts)):
                dy = clamp(pts[j][1] - pts[j - 1][1])
                dx = clamp(pts[j][0] - pts[j - 1][0])
                lines.append(f"    {dy:4d}, {dx:4d},")
            lines.append("};")
            lines.append("")

        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Vectrex H.E.R.O. Level & Sprite Editor")
    parser.add_argument('--project', default=None, help='Path to project JSON file to load at startup')
    parser.add_argument('--rom', default=None, help='Path to Vectrex system ROM')
    parser.add_argument('--cart', default=None, help='Path to cartridge ROM')
    args = parser.parse_args()

    root = tk.Tk()
    root.geometry("1100x700")
    root.minsize(1000, 660)
    app = App(root)

    # Override sprite canvas redraw to also update VLC text
    orig_sprite_redraw = app.sprite_canvas.redraw
    def sprite_redraw_with_vlc():
        orig_sprite_redraw()
        app._update_vlc_text()
    app.sprite_canvas.redraw = sprite_redraw_with_vlc

    if args.project:
        app._load_project_file(os.path.abspath(args.project))
    if args.rom:
        app._emu_rom_var.set(args.rom)
    if args.cart:
        app._emu_cart_var.set(args.cart)
    if args.rom:
        app.notebook.select(2)
        root.after(100, app._emu_start)

    root.mainloop()


if __name__ == "__main__":
    main()
