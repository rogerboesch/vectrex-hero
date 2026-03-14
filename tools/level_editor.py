#!/usr/bin/env python3
"""
Vectrex H.E.R.O. Level & Sprite Editor

Visual editor for designing levels and VLC sprites for the Vectrex H.E.R.O. game.
Exports data as .h header files ready to #include in main.c.

Usage: python3 tools/level_editor.py
"""

import bisect
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
# BIOS function names (from vectrec/stdlib/std.inc)
# ---------------------------------------------------------------------------

BIOS_FUNCTIONS = {
    0xF000: "Cold_Start",
    0xF06C: "Warm_Start",
    0xF14C: "Init_VIA",
    0xF164: "Init_OS_RAM",
    0xF18B: "Init_OS",
    0xF192: "Wait_Recal",
    0xF1A2: "Set_Refresh",
    0xF1AA: "DP_to_D0",
    0xF1AF: "DP_to_C8",
    0xF1B4: "Read_Btns_Mask",
    0xF1BA: "Read_Btns",
    0xF1F5: "Joy_Analog",
    0xF1F8: "Joy_Digital",
    0xF256: "Sound_Byte",
    0xF259: "Sound_Byte_x",
    0xF25B: "Sound_Byte_raw",
    0xF272: "Clear_Sound",
    0xF27D: "Sound_Bytes",
    0xF284: "Sound_Bytes_x",
    0xF289: "Do_Sound",
    0xF28C: "Do_Sound_x",
    0xF29D: "Intensity_1F",
    0xF2A1: "Intensity_3F",
    0xF2A5: "Intensity_5F",
    0xF2A9: "Intensity_7F",
    0xF2AB: "Intensity_a",
    0xF2BE: "Dot_ix_b",
    0xF2C1: "Dot_ix",
    0xF2C3: "Dot_d",
    0xF2C5: "Dot_here",
    0xF2D5: "Dot_List",
    0xF2DE: "Dot_List_Reset",
    0xF2E6: "Recalibrate",
    0xF2F2: "Moveto_x_7F",
    0xF2FC: "Moveto_d_7F",
    0xF308: "Moveto_ix_FF",
    0xF30C: "Moveto_ix_7F",
    0xF30E: "Moveto_ix_b",
    0xF310: "Moveto_ix",
    0xF312: "Moveto_d",
    0xF34A: "Reset0Ref_D0",
    0xF34F: "Check0Ref",
    0xF354: "Reset0Ref",
    0xF35B: "Reset_Pen",
    0xF36B: "Reset0Int",
    0xF373: "Print_Str_hwyx",
    0xF378: "Print_Str_yx",
    0xF37A: "Print_Str_d",
    0xF385: "Print_List_hw",
    0xF38A: "Print_List",
    0xF38C: "Print_List_chk",
    0xF391: "Print_Ships_x",
    0xF393: "Print_Ships",
    0xF3AD: "Mov_Draw_VLc_a",
    0xF3B1: "Mov_Draw_VL_b",
    0xF3B5: "Mov_Draw_VLcs",
    0xF3B7: "Mov_Draw_VL_ab",
    0xF3B9: "Mov_Draw_VL_a",
    0xF3BC: "Mov_Draw_VL",
    0xF3BE: "Mov_Draw_VL_d",
    0xF3CE: "Draw_VLc",
    0xF3D2: "Draw_VL_b",
    0xF3D6: "Draw_VLcs",
    0xF3D8: "Draw_VL_ab",
    0xF3DA: "Draw_VL_a",
    0xF3DD: "Draw_VL",
    0xF3DF: "Draw_Line_d",
    0xF404: "Draw_VLp_FF",
    0xF408: "Draw_VLp_7F",
    0xF40C: "Draw_VLp_scale",
    0xF40E: "Draw_VLp_b",
    0xF410: "Draw_VLp",
    0xF434: "Draw_Pat_VL_a",
    0xF437: "Draw_Pat_VL",
    0xF439: "Draw_Pat_VL_d",
    0xF46E: "Draw_VL_mode",
    0xF495: "Print_Str",
    0xF511: "Random_3",
    0xF517: "Random",
    0xF533: "Init_Music_Buf",
    0xF53F: "Clear_x_b",
    0xF542: "Clear_C8_RAM",
    0xF545: "Clear_x_256",
    0xF548: "Clear_x_d",
    0xF550: "Clear_x_b_80",
    0xF552: "Clear_x_b_a",
    0xF55A: "Dec_3_Counters",
    0xF55E: "Dec_6_Counters",
    0xF563: "Dec_Counters",
    0xF56D: "Delay_3",
    0xF571: "Delay_2",
    0xF575: "Delay_1",
    0xF579: "Delay_0",
    0xF57A: "Delay_b",
    0xF57D: "Delay_RTS",
    0xF57E: "Bitmask_a",
    0xF584: "Abs_a_b",
    0xF58B: "Abs_b",
    0xF593: "Rise_Run_Angle",
    0xF5D9: "Get_Rise_Idx",
    0xF5DB: "Get_Run_Idx",
    0xF5EF: "Get_Rise_Run",
    0xF5FF: "Rise_Run_X",
    0xF601: "Rise_Run_Y",
    0xF603: "Rise_Run_Len",
    0xF610: "Rot_VL_ab",
    0xF616: "Rot_VL",
    0xF61F: "Rot_VL_Mode_a",
    0xF62B: "Rot_VL_Mode",
    0xF637: "Rot_VL_dft",
    0xF65B: "Xform_Run_a",
    0xF65D: "Xform_Run",
    0xF661: "Xform_Rise_a",
    0xF663: "Xform_Rise",
    0xF67F: "Move_Mem_a_1",
    0xF683: "Move_Mem_a",
    0xF687: "Init_Music_chk",
    0xF68D: "Init_Music",
    0xF692: "Init_Music_x",
    0xF7A9: "Select_Game",
    0xF84F: "Clear_Score",
    0xF85E: "Add_Score_a",
    0xF87C: "Add_Score_d",
    0xF8B7: "Strip_Zeros",
    0xF8C7: "Compare_Score",
    0xF8D8: "New_High_Score",
    0xF8E5: "Obj_Will_Hit_u",
    0xF8F3: "Obj_Will_Hit",
    0xF8FF: "Obj_Hit",
    0xF92E: "Explosion_Snd",
    0xF9F4: "Char_Table",
    0xFBD4: "Char_Table_End",
    0xFF9F: "Draw_Grid_VL",
}
_BIOS_ADDRS = sorted(BIOS_FUNCTIONS.keys())


def _bios_name(pc):
    """Return the BIOS function name for a PC in ROM space, or None."""
    if pc < 0xF000:
        return None
    idx = bisect.bisect_right(_BIOS_ADDRS, pc) - 1
    if idx >= 0:
        return BIOS_FUNCTIONS[_BIOS_ADDRS[idx]]
    return None


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


def extract_segments(cave_lines):
    """Extract axis-aligned line segments from cave polylines as (x1, y1, x2, y2)."""
    segments = []
    for polyline in cave_lines:
        if len(polyline) < 2:
            continue
        for j in range(1, len(polyline)):
            x1, y1 = int(polyline[j-1][0]), int(polyline[j-1][1])
            x2, y2 = int(polyline[j][0]), int(polyline[j][1])
            segments.append((x1, y1, x2, y2))
    return segments


def _polyline_segments(polyline):
    """Convert a polyline (list of [x,y] points) into (dy,dx) segments,
    clamped to int8_t range."""
    segments = []
    for j in range(1, len(polyline)):
        dy = int(polyline[j][1] - polyline[j - 1][1])
        dx = int(polyline[j][0] - polyline[j - 1][0])
        while dy != 0 or dx != 0:
            sy = max(-128, min(127, dy))
            sx = max(-128, min(127, dx))
            segments.append((sy, sx))
            dy -= sy
            dx -= sx
    return segments


def cave_room_data(cave_lines):
    """Build flat int8_t data for a room's cave polylines.

    Format: for each polyline: num_segments, start_y, start_x, dy, dx, ...
    Terminated by a 0 byte.

    Merges polylines that share an endpoint (start==end or start==start etc.)
    into a single group to reduce zero_beam overhead.
    """
    # Build list of (start, end, segments) for each polyline
    polys = []
    for polyline in cave_lines:
        if len(polyline) < 2:
            continue
        segs = _polyline_segments(polyline)
        if not segs:
            continue
        start = (int(polyline[0][0]), int(polyline[0][1]))
        end = (int(polyline[-1][0]), int(polyline[-1][1]))
        polys.append([start, end, segs])

    # Greedily merge polylines that share an endpoint
    merged = True
    while merged:
        merged = False
        for i in range(len(polys)):
            for j in range(i + 1, len(polys)):
                si, ei, segi = polys[i]
                sj, ej, segj = polys[j]
                if ei == sj:
                    # i's end == j's start: append j to i
                    polys[i] = [si, ej, segi + segj]
                    polys.pop(j)
                    merged = True
                    break
                elif ei == ej:
                    # i's end == j's end: append reversed j to i
                    polys[i] = [si, sj, segi + [(-dy, -dx) for dy, dx in reversed(segj)]]
                    polys.pop(j)
                    merged = True
                    break
                elif si == sj:
                    # i's start == j's start: prepend reversed j to i
                    polys[i] = [ej, ei, [(-dy, -dx) for dy, dx in reversed(segj)] + segi]
                    polys.pop(j)
                    merged = True
                    break
                elif si == ej:
                    # i's start == j's end: prepend j to i
                    polys[i] = [sj, ei, segj + segi]
                    polys.pop(j)
                    merged = True
                    break
            if merged:
                break

    # Emit flat data
    data = []
    for start, end, segs in polys:
        data.append(len(segs))
        data.append(start[1])  # y
        data.append(start[0])  # x
        for sy, sx in segs:
            data.append(sy)
            data.append(sx)
    data.append(0)  # terminator
    return data


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

def new_room(number=1):
    return {
        "number": number,
        "exit_left": None,
        "exit_right": None,
        "exit_top": None,
        "exit_bottom": None,
        "cave_lines": [],       # list of polylines, each is list of [x, y]
        "walls": [],            # list of {"y": int, "x": int, "h": int, "w": int, "destroyable": bool}
        "enemies": [],          # list of {"x": int, "y": int, "vx": int}
        "miner": None,          # {"x": int, "y": int} or None
        "player_start": None,   # {"x": int, "y": int} or None
        "cave_constants": {
            "CAVE_LEFT": -90, "CAVE_RIGHT": 90,
            "CAVE_TOP": 105, "CAVE_FLOOR": -95,
        },
        "has_lava": False,
        "bg_image": None,       # {"path": str, "x": int, "y": int, "scale": float} or None
    }


def new_level(name="Level 1"):
    return {
        "name": name,
        "rooms": [new_room()],
    }


def new_sprite():
    return {
        "name": "sprite",
        "frames": [{"points": []}],  # list of frames, each with [x, y] line segments
        "bg_image": None,       # {"path": str, "x": int, "y": int, "scale": float} or None
    }


def new_project():
    return {
        "levels": [new_level()],
        "sprites": [new_sprite()],
    }


def migrate_project(data):
    """Auto-migrate old format (flat levels) to room-based format."""
    for lvl in data.get("levels", []):
        if "rooms" not in lvl:
            # Old format: level dict has room data directly
            room = {k: v for k, v in lvl.items() if k != "name"}
            room["number"] = 1
            room.pop("name", None)
            for exit_key in ("exit_left", "exit_right", "exit_top", "exit_bottom"):
                if exit_key not in room:
                    room[exit_key] = None
            lvl["rooms"] = [room]
            # Remove old keys from level dict (keep "name" and "rooms")
            for k in list(room.keys()):
                if k in lvl and k not in ("name", "rooms"):
                    del lvl[k]

    # Migrate rooms: add has_lava default
    for lvl in data.get("levels", []):
        for room in lvl.get("rooms", []):
            if "has_lava" not in room:
                room["has_lava"] = False

    # Migrate enemies: add type field
    for lvl in data.get("levels", []):
        for room in lvl.get("rooms", []):
            for e in room.get("enemies", []):
                if "type" not in e:
                    e["type"] = "bat"

    # Migrate sprites: old "points" format → "frames" format
    for sprite in data.get("sprites", []):
        if "points" in sprite and "frames" not in sprite:
            sprite["frames"] = [{"points": sprite.pop("points")}]

    # Merge bat_frame1 + bat_frame2 into single "bat" sprite
    sprites = data.get("sprites", [])
    bat_frames = {}
    bat_indices = []
    for i, s in enumerate(sprites):
        if s["name"].startswith("bat_frame"):
            bat_frames[s["name"]] = s
            bat_indices.append(i)
    if "bat_frame1" in bat_frames and "bat_frame2" in bat_frames:
        merged = {
            "name": "bat",
            "frames": bat_frames["bat_frame1"]["frames"] + bat_frames["bat_frame2"]["frames"],
            "bg_image": bat_frames["bat_frame1"].get("bg_image") or bat_frames["bat_frame2"].get("bg_image"),
        }
        # Remove old entries (reverse order to preserve indices)
        for i in sorted(bat_indices, reverse=True):
            sprites.pop(i)
        # Insert merged sprite at first bat position
        sprites.insert(min(bat_indices), merged)

    return data


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
        return self.app.current_room_data()

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
        if self.app._level_bg_show_var.get():
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
            # Resize handle at bottom-left corner (hidden when locked)
            if not bg.get("locked", False):
                hw = self._bg_photo.width() // 2
                hh = self._bg_photo.height() // 2
                hx, hy = cx - hw, cy + hh
                self.create_rectangle(hx - 4, hy - 4, hx + 4, hy + 4,
                                      outline="#888", fill="#444", tags="bg_handle")

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

        # Draw lava indicator if room has lava
        if lvl.get("has_lava"):
            lava_sin = [0, 3, 4, 3, 0, -3, -4, -3]
            step = (cr - cl) / 8
            points = []
            for i in range(9):
                wx = cl + step * i
                wy = cf + lava_sin[i & 7]
                px, py = self._cx(wx, wy)
                points.extend([px, py])
            if len(points) >= 4:
                self.create_line(*points, fill="#ff6600", width=2,
                                 smooth=True, tags="lava")

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
            is_spider = e.get("type", "bat") == "spider"
            color = "#cc33ff" if is_spider else "#ff3333"
            self.create_oval(cx - r, cy - r, cx + r, cy + r,
                             outline=color, width=2, tags=("enemy", f"enemy_{i}"))
            if is_spider:
                # Thread line going up from spider
                self.create_line(cx, cy - r, cx, cy - r - 12, fill=color,
                                 width=1, dash=(2, 2),
                                 tags=("enemy", f"enemy_{i}"))
                # Direction arrow (vertical)
                arrow_dy = -12 if e["vx"] > 0 else 12 if e["vx"] < 0 else 0
                if arrow_dy:
                    self.create_line(cx + r + 4, cy, cx + r + 4, cy + arrow_dy,
                                     fill="#dd66ff", arrow=tk.LAST, width=2,
                                     tags=("enemy", f"enemy_{i}"))
            else:
                # Bat wings
                self.create_line(cx - r, cy, cx - r - 5, cy - 4, fill=color,
                                 tags=("enemy", f"enemy_{i}"))
                self.create_line(cx + r, cy, cx + r + 5, cy - 4, fill=color,
                                 tags=("enemy", f"enemy_{i}"))
                # Direction arrow (horizontal)
                arrow_dx = 12 if e["vx"] > 0 else -12 if e["vx"] < 0 else 0
                if arrow_dx:
                    self.create_line(cx, cy + r + 4, cx + arrow_dx, cy + r + 4,
                                     fill="#ff6666", arrow=tk.LAST, width=2,
                                     tags=("enemy", f"enemy_{i}"))
            label = "S" if is_spider else "B"
            self.create_text(cx, cy, text=label, fill=color,
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
        # Background (skip if locked)
        if lvl.get("bg_image") and self._bg_photo:
            if not lvl["bg_image"].get("locked", False):
                # Resize handle (bottom-left corner)
                bg = lvl["bg_image"]
                bcx, bcy = self._cx(bg.get("x", 0), bg.get("y", 0))
                hw = self._bg_photo.width() // 2
                hh = self._bg_photo.height() // 2
                hx, hy = bcx - hw, bcy + hh
                if abs(cx - hx) < tol + 6 and abs(cy - hy) < tol + 6:
                    return ("bg_resize", 0)
                # Drag to reposition (lowest priority)
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
            return f"Enemy {self._drag_idx} ({e.get('type', 'bat')}): x={e['x']}, y={e['y']}, vx={e['vx']}"
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
        elif self._drag_type == "bg_resize":
            bg = lvl.get("bg_image", {})
            return f"Background resize: scale={bg.get('scale', 1.0):.2f}"
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
                if self._drag_type == "bg_resize":
                    bg = self.level.get("bg_image", {})
                    self._bg_resize_start_scale = bg.get("scale", 1.0)
                    self._bg_resize_start_y = event.y
                self.app.update_status(self._selection_info())
            else:
                self._drag_type = None
                self._drag_idx = None

        elif self.tool == "cave_line":
            self._polyline_pts.append([vx, vy])
            self._redraw_polyline_preview()

        elif self.tool == "enemy":
            self.level["enemies"].append({"x": vx, "y": vy, "vx": 1, "type": "bat"})
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
            elif self._drag_type == "bg_resize" and lvl.get("bg_image"):
                # Drag down = grow, drag up = shrink
                dy = event.y - self._bg_resize_start_y
                new_scale = max(0.1, self._bg_resize_start_scale + dy * 0.005)
                new_scale = round(new_scale, 2)
                lvl["bg_image"]["scale"] = new_scale
                self.app._level_bg_scale.set(new_scale)
                self.app._level_bg_scale_label.config(text=f"{new_scale:.1f}x")
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
            # Keep selection alive so Delete key works;
            # selection is cleared on next click on empty space
            pass

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
            self._enemy_prop_dialog(e)
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

    def _enemy_prop_dialog(self, e):
        dlg = tk.Toplevel(self)
        dlg.title("Enemy Properties")
        dlg.transient(self.winfo_toplevel())
        dlg.grab_set()
        tk.Label(dlg, text="x:").grid(row=0, column=0, padx=5, pady=2, sticky="e")
        ent_x = tk.Entry(dlg, width=10)
        ent_x.insert(0, str(e["x"]))
        ent_x.grid(row=0, column=1, padx=5, pady=2)
        tk.Label(dlg, text="y:").grid(row=1, column=0, padx=5, pady=2, sticky="e")
        ent_y = tk.Entry(dlg, width=10)
        ent_y.insert(0, str(e["y"]))
        ent_y.grid(row=1, column=1, padx=5, pady=2)
        tk.Label(dlg, text="vx:").grid(row=2, column=0, padx=5, pady=2, sticky="e")
        ent_vx = tk.Entry(dlg, width=10)
        ent_vx.insert(0, str(e["vx"]))
        ent_vx.grid(row=2, column=1, padx=5, pady=2)
        tk.Label(dlg, text="type:").grid(row=3, column=0, padx=5, pady=2, sticky="e")
        type_var = tk.StringVar(value=e.get("type", "bat"))
        type_combo = ttk.Combobox(dlg, textvariable=type_var, values=["bat", "spider"],
                                  state="readonly", width=8)
        type_combo.grid(row=3, column=1, padx=5, pady=2)

        def apply():
            try:
                e.update({
                    "x": int(ent_x.get()),
                    "y": int(ent_y.get()),
                    "vx": int(ent_vx.get()),
                    "type": type_var.get(),
                })
                dlg.destroy()
                self.redraw()
            except ValueError:
                messagebox.showerror("Invalid input", "Please enter valid integers.")

        tk.Button(dlg, text="OK", command=apply).grid(row=4, column=0, columnspan=2, pady=8)
        ent_x.focus_set()
        dlg.bind("<Return>", lambda ev: apply())

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
        self._drag_bg_resize = False  # dragging resize handle
        self._bg_resize_start_scale = 1.0
        self._bg_resize_start_y = 0
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

    @property
    def frame(self):
        return self.app.current_frame_data()

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
        frame = self.frame
        if not frame:
            return None
        tol = CLICK_TOLERANCE + 4
        for i, pt in enumerate(frame["points"]):
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
        if self.app._sprite_bg_show_var.get():
            self._draw_bg_image(sprite)
        frame = self.frame
        pts = frame["points"] if frame else []
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
        # Bounding box (centered at origin)
        if pts:
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            hw = max(abs(min(xs)), abs(max(xs)))
            hh = max(abs(min(ys)), abs(max(ys)))
            bx1, by1 = self._cx(-hw, -hh)
            bx2, by2 = self._cx(hw, hh)
            self.create_rectangle(bx1, by1, bx2, by2,
                                  outline="#445566", dash=(4, 4), width=1)
            self.create_text(10, 15, text=f"BBox: HW={hw} HH={hh}",
                             fill="#668888", font=("Courier", 9), anchor="w")
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
            # Resize handle at bottom-left corner (hidden when locked)
            if not bg.get("locked", False):
                hw = self._bg_photo.width() // 2
                hh = self._bg_photo.height() // 2
                hx, hy = cx - hw, cy + hh
                self.create_rectangle(hx - 4, hy - 4, hx + 4, hy + 4,
                                      outline="#888", fill="#444", tags="bg_handle")

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
        vlc = self._compute_vlc(self.frame)
        if vlc:
            txt = "VLC: " + ", ".join(str(v) for v in vlc)
            self.create_text(10, SPRITE_CANVAS_SIZE - 15, text=txt,
                             fill="#666688", font=("Courier", 9), anchor="w")

    def _compute_vlc(self, frame):
        if not frame:
            return None
        pts = frame["points"]
        if len(pts) < 2:
            return None
        count = len(pts) - 1
        data = [count - 1]  # VLC format: count-1 for draw_vlc BIOS call
        for i in range(1, len(pts)):
            dy = pts[i][1] - pts[i - 1][1]
            dx = pts[i][0] - pts[i - 1][0]
            data.extend([clamp(dy), clamp(dx)])
        return data

    def _on_motion(self, event):
        vx, vy = self._vx(event.x, event.y)
        tool_str = f"  |  Tool: {self.tool}" if self.tool == "select" else ""
        sel_str = ""
        if self._drag_idx is not None and self.frame:
            pt = self.frame["points"][self._drag_idx]
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
            self._drag_bg_resize = False
            if hit is not None:
                self.redraw()
                pt = self.frame["points"][hit]
                self.app.update_status(
                    f"Sprite point {hit}: ({pt[0]}, {pt[1]})")
            elif sprite.get("bg_image") and self._bg_photo and not sprite["bg_image"].get("locked", False):
                bg = sprite["bg_image"]
                # Check resize handle (bottom-left corner)
                bcx, bcy = self._cx(bg.get("x", 0), bg.get("y", 0))
                hw = self._bg_photo.width() // 2
                hh = self._bg_photo.height() // 2
                hx, hy = bcx - hw, bcy + hh
                tol = CLICK_TOLERANCE + 6
                if abs(event.x - hx) < tol and abs(event.y - hy) < tol:
                    self._drag_bg_resize = True
                    self._bg_resize_start_scale = bg.get("scale", 1.0)
                    self._bg_resize_start_y = event.y
                    self.app.update_status(
                        f"Background resize: scale={bg.get('scale', 1.0):.2f}")
                else:
                    self._drag_bg = True
                    self.app.update_status(
                        f"Background: x={bg.get('x', 0)}, y={bg.get('y', 0)}")
                self.redraw()
            else:
                self.redraw()
        else:
            # Draw mode: add point
            vx, vy = self._vx(event.x, event.y)
            frame = self.frame
            if frame is not None:
                frame["points"].append([vx, vy])
            self.redraw()

    def _on_drag(self, event):
        if self.tool == "select":
            sprite = self.sprite
            if not sprite:
                return
            vx, vy = self._vx(event.x, event.y)
            frame = self.frame
            if frame and self._drag_idx is not None and 0 <= self._drag_idx < len(frame["points"]):
                frame["points"][self._drag_idx] = [vx, vy]
                self.redraw()
                self.app.update_status(
                    f"Sprite point {self._drag_idx}: ({vx}, {vy})")
            elif self._drag_bg_resize and sprite.get("bg_image"):
                dy = event.y - self._bg_resize_start_y
                new_scale = max(0.1, self._bg_resize_start_scale + dy * 0.005)
                new_scale = round(new_scale, 2)
                sprite["bg_image"]["scale"] = new_scale
                self.app._sprite_bg_scale.set(new_scale)
                self.app._sprite_bg_scale_label.config(text=f"{new_scale:.1f}x")
                self.redraw()
                self.app.update_status(f"Background resize: scale={new_scale:.2f}")
            elif self._drag_bg and sprite.get("bg_image"):
                sprite["bg_image"]["x"] = vx
                sprite["bg_image"]["y"] = vy
                self.redraw()
                self.app.update_status(f"Background: x={vx}, y={vy}")

    def _on_release(self, event):
        pass  # keep selection active after drag

    def _on_delete(self, event):
        if self._drag_idx is not None:
            frame = self.frame
            if frame and 0 <= self._drag_idx < len(frame["points"]):
                del frame["points"][self._drag_idx]
                self._drag_idx = None
                self.redraw()

    def _on_right_click(self, event):
        """Remove last point."""
        frame = self.frame
        if frame and frame["points"]:
            frame["points"].pop()
            self.redraw()


# ---------------------------------------------------------------------------
# Main Application
# ---------------------------------------------------------------------------

class App:
    def __init__(self, root):
        self.root = root
        self.root.title("CMOC Studio — Vectrex Level & Sprite Editor")
        self.root.configure(bg="#1a1a2e")
        self.project = new_project()
        self._current_level_idx = 0
        self._current_room_idx = 0
        self._current_sprite_idx = 0
        self._current_frame_idx = 0

        self._build_menu()
        self._build_ui()
        self.level_canvas.redraw()
        self.sprite_canvas.redraw()

    def current_level_data(self):
        idx = self._current_level_idx
        if 0 <= idx < len(self.project["levels"]):
            return self.project["levels"][idx]
        return None

    def current_room_data(self):
        lvl = self.current_level_data()
        if lvl is None:
            return None
        rooms = lvl.get("rooms", [])
        if 0 <= self._current_room_idx < len(rooms):
            return rooms[self._current_room_idx]
        return None

    def current_sprite_data(self):
        idx = self._current_sprite_idx
        if 0 <= idx < len(self.project["sprites"]):
            return self.project["sprites"][idx]
        return None

    def current_frame_data(self):
        sprite = self.current_sprite_data()
        if sprite and 0 <= self._current_frame_idx < len(sprite["frames"]):
            return sprite["frames"][self._current_frame_idx]
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

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=4)

        # Room selector
        ttk.Label(left, text="Room", font=("Helvetica", 11, "bold")).pack(pady=(4, 4))
        self._room_label = ttk.Label(left, text="Room 1 / 1", font=("Courier", 9))
        self._room_label.pack(anchor="w", padx=4)
        self._update_room_label()

        room_btn_frame = ttk.Frame(left)
        room_btn_frame.pack(fill="x", padx=4, pady=4)
        ttk.Button(room_btn_frame, text="<", width=2,
                   command=self._prev_room).pack(side="left", padx=1)
        ttk.Button(room_btn_frame, text=">", width=2,
                   command=self._next_room).pack(side="left", padx=1)
        ttk.Button(room_btn_frame, text="+", width=2,
                   command=self._add_room).pack(side="left", padx=1)
        ttk.Button(room_btn_frame, text="-", width=2,
                   command=self._remove_room).pack(side="left", padx=1)
        ttk.Button(room_btn_frame, text="Copy", width=4,
                   command=self._copy_room).pack(side="left", padx=1)

        # Room exits
        exit_frame = ttk.LabelFrame(left, text="Room Exits")
        exit_frame.pack(fill="x", padx=4, pady=4)
        self._exit_vars = {}
        for exit_dir in ("left", "right", "top", "bottom"):
            row = ttk.Frame(exit_frame)
            row.pack(fill="x", padx=2, pady=1)
            ttk.Label(row, text=exit_dir.capitalize() + ":", width=7,
                      font=("Courier", 8)).pack(side="left")
            var = tk.StringVar(value="")
            ent = ttk.Entry(row, textvariable=var, width=5)
            ent.pack(side="left", padx=2)
            self._exit_vars[exit_dir] = var
            var.trace_add("write", lambda *a, d=exit_dir: self._on_exit_changed(d))
        self._load_exit_fields()

        # Room properties
        prop_frame = ttk.LabelFrame(left, text="Room Properties")
        prop_frame.pack(fill="x", padx=4, pady=4)
        self._lava_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(prop_frame, text="Has Lava",
                         variable=self._lava_var,
                         command=self._on_lava_changed).pack(anchor="w", padx=2, pady=1)

        ttk.Separator(left, orient="horizontal").pack(fill="x", pady=4)

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

        move_row = ttk.Frame(left)
        move_row.pack(padx=4, pady=4)
        ttk.Button(move_row, text="\u2190",
                   command=lambda: self._move_level_element(-1, 0), width=3).pack(side="left", padx=1)
        ttk.Button(move_row, text="\u2192",
                   command=lambda: self._move_level_element(1, 0), width=3).pack(side="left", padx=1)
        ttk.Button(move_row, text="\u2191",
                   command=lambda: self._move_level_element(0, 1), width=3).pack(side="left", padx=1)
        ttk.Button(move_row, text="\u2193",
                   command=lambda: self._move_level_element(0, -1), width=3).pack(side="left", padx=1)

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
        self._level_bg_show_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(right, text="Show BG",
                        variable=self._level_bg_show_var,
                        command=self._on_level_bg_show).pack(fill="x", padx=4, pady=2)
        self._level_bg_lock_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(right, text="Lock BG",
                        variable=self._level_bg_lock_var,
                        command=self._on_level_bg_lock).pack(fill="x", padx=4, pady=2)
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

        ttk.Label(left, text="Frame", font=("Helvetica", 10, "bold")).pack(
            anchor="w", padx=4, pady=(0, 4))
        self._frame_label = ttk.Label(left, text="Frame 1 / 1", font=("Courier", 9))
        self._frame_label.pack(anchor="w", padx=4)
        frame_btn = ttk.Frame(left)
        frame_btn.pack(fill="x", padx=4, pady=4)
        ttk.Button(frame_btn, text="<", width=2,
                   command=self._prev_frame).pack(side="left", padx=1)
        ttk.Button(frame_btn, text=">", width=2,
                   command=self._next_frame).pack(side="left", padx=1)
        ttk.Button(frame_btn, text="+", width=2,
                   command=self._add_frame).pack(side="left", padx=1)
        ttk.Button(frame_btn, text="-", width=2,
                   command=self._remove_frame).pack(side="left", padx=1)
        ttk.Button(frame_btn, text="Copy", width=4,
                   command=self._copy_frame).pack(side="left", padx=1)

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

        mirror_row = ttk.Frame(left)
        mirror_row.pack(padx=4, pady=4)
        ttk.Button(mirror_row, text="Mirror H",
                   command=self._mirror_sprite_h).pack(side="left", padx=2)
        ttk.Button(mirror_row, text="Mirror V",
                   command=self._mirror_sprite_v).pack(side="left", padx=2)

        move_row = ttk.Frame(left)
        move_row.pack(padx=4, pady=4)
        ttk.Button(move_row, text="\u2190",
                   command=self._move_sprite_left, width=3).pack(side="left", padx=1)
        ttk.Button(move_row, text="\u2192",
                   command=self._move_sprite_right, width=3).pack(side="left", padx=1)
        ttk.Button(move_row, text="\u2191",
                   command=self._move_sprite_up, width=3).pack(side="left", padx=1)
        ttk.Button(move_row, text="\u2193",
                   command=self._move_sprite_down, width=3).pack(side="left", padx=1)

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
        self._sprite_bg_show_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(right, text="Show BG",
                        variable=self._sprite_bg_show_var,
                        command=self._on_sprite_bg_show).pack(fill="x", padx=4, pady=2)
        self._sprite_bg_lock_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(right, text="Lock BG",
                        variable=self._sprite_bg_lock_var,
                        command=self._on_sprite_bg_lock).pack(fill="x", padx=4, pady=2)
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
        self._emu_container = tk.Frame(outer, bg="#333333")
        self._emu_container.pack(side="left", fill="both", expand=True, padx=4, pady=4)

        # Right panel: CPU state
        right = ttk.Frame(outer, width=250)
        right.pack(side="right", fill="y", padx=(0, 4), pady=4)
        right.pack_propagate(False)

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
            ]
            bios = _bios_name(state['PC'])
            lines.append(f"> {bios}" if bios else "> -")
            lines.extend([
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
                "",
                f"Cycles  {state['cycles']}",
                f"Min     {state['cycles_min']}",
                f"Max     {state['cycles_max']}",
            ])
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

    def _move_level_element(self, dx, dy):
        lc = self.level_canvas
        lvl = lc.level
        if not lvl or not lc._drag_type:
            return
        if lc._drag_type == "cave_point":
            pi, vi = lc._drag_idx
            lvl["cave_lines"][pi][vi][0] += dx
            lvl["cave_lines"][pi][vi][1] += dy
        elif lc._drag_type == "enemy":
            lvl["enemies"][lc._drag_idx]["x"] += dx
            lvl["enemies"][lc._drag_idx]["y"] += dy
        elif lc._drag_type == "wall":
            lvl["walls"][lc._drag_idx]["x"] += dx
            lvl["walls"][lc._drag_idx]["y"] += dy
        elif lc._drag_type == "miner":
            lvl["miner"]["x"] += dx
            lvl["miner"]["y"] += dy
        elif lc._drag_type == "player_start":
            lvl["player_start"]["x"] += dx
            lvl["player_start"]["y"] += dy
        else:
            return
        lc.redraw()
        self.update_status(lc._selection_info())

    # ---- Level management ----

    def _refresh_level_combo(self):
        labels = [f"Level {i+1}" for i in range(len(self.project["levels"]))]
        self._level_combo["values"] = labels
        if labels:
            self._level_combo.current(self._current_level_idx)

    def _level_selected(self, event=None):
        self._current_level_idx = self._level_combo.current()
        self._current_room_idx = 0
        self._update_room_label()
        self._load_exit_fields()
        self._load_constants_to_entries()
        self._sync_level_bg_scale()
        self.level_canvas._bg_path_loaded = None  # force reload for new level
        self.level_canvas.redraw()

    def _sync_level_bg_scale(self):
        room = self.current_room_data()
        if room and room.get("bg_image"):
            s = room["bg_image"].get("scale", 1.0)
            self._level_bg_scale.set(s)
            self._level_bg_scale_label.config(text=f"{s:.1f}x")
            self._level_bg_lock_var.set(room["bg_image"].get("locked", False))
            self._level_bg_show_var.set(room["bg_image"].get("show", True))
        else:
            self._level_bg_scale.set(1.0)
            self._level_bg_scale_label.config(text="1.0x")
            self._level_bg_lock_var.set(False)
            self._level_bg_show_var.set(True)

    def _add_level(self):
        lvl = new_level()
        self.project["levels"].append(lvl)
        self._current_level_idx = len(self.project["levels"]) - 1
        self._current_room_idx = 0
        self._refresh_level_combo()
        self._update_room_label()
        self._load_exit_fields()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    def _remove_level(self):
        if len(self.project["levels"]) <= 1:
            messagebox.showinfo("Info", "Cannot remove the last level.")
            return
        del self.project["levels"][self._current_level_idx]
        self._current_level_idx = max(0, self._current_level_idx - 1)
        self._current_room_idx = 0
        self._refresh_level_combo()
        self._update_room_label()
        self._load_exit_fields()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    # ---- Room management ----

    def _update_room_label(self):
        lvl = self.current_level_data()
        if lvl:
            n = len(lvl.get("rooms", []))
            self._room_label.config(text=f"Room {self._current_room_idx + 1} / {n}")
        else:
            self._room_label.config(text="Room - / -")

    def _prev_room(self):
        if self._current_room_idx > 0:
            self._current_room_idx -= 1
            self._update_room_label()
            self._load_exit_fields()
            self._load_constants_to_entries()
            self._sync_level_bg_scale()
            self.level_canvas._bg_path_loaded = None
            self.level_canvas.redraw()

    def _next_room(self):
        lvl = self.current_level_data()
        if lvl and self._current_room_idx < len(lvl.get("rooms", [])) - 1:
            self._current_room_idx += 1
            self._update_room_label()
            self._load_exit_fields()
            self._load_constants_to_entries()
            self._sync_level_bg_scale()
            self.level_canvas._bg_path_loaded = None
            self.level_canvas.redraw()

    def _add_room(self):
        lvl = self.current_level_data()
        if lvl is None:
            return
        rooms = lvl["rooms"]
        # Find next room number
        max_num = max(r["number"] for r in rooms) if rooms else 0
        room = new_room(max_num + 1)
        rooms.append(room)
        self._current_room_idx = len(rooms) - 1
        self._update_room_label()
        self._load_exit_fields()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    def _copy_room(self):
        import copy
        lvl = self.current_level_data()
        if lvl is None:
            return
        rooms = lvl["rooms"]
        src = rooms[self._current_room_idx]
        max_num = max(r["number"] for r in rooms) if rooms else 0
        room = copy.deepcopy(src)
        room["number"] = max_num + 1
        room.pop("name", None)
        # Clear exits — they reference the source room's neighbours
        room["exit_left"] = None
        room["exit_right"] = None
        room["exit_top"] = None
        room["exit_bottom"] = None
        rooms.append(room)
        self._current_room_idx = len(rooms) - 1
        self._update_room_label()
        self._load_exit_fields()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    def _remove_room(self):
        lvl = self.current_level_data()
        if lvl is None:
            return
        rooms = lvl["rooms"]
        if len(rooms) <= 1:
            messagebox.showinfo("Info", "Cannot remove the last room.")
            return
        idx = self._current_room_idx
        removed_num = rooms[idx]["number"]
        if not messagebox.askyesno("Remove Room",
                                    f"Remove room {idx + 1}? Exit references will be cleared."):
            return
        del rooms[self._current_room_idx]
        # Clean up exit references to removed room
        for r in rooms:
            for exit_key in ("exit_left", "exit_right", "exit_top", "exit_bottom"):
                if r.get(exit_key) == removed_num:
                    r[exit_key] = None
        self._current_room_idx = max(0, self._current_room_idx - 1)
        self._update_room_label()
        self._load_exit_fields()
        self._load_constants_to_entries()
        self.level_canvas.redraw()

    def _load_exit_fields(self):
        room = self.current_room_data()
        for exit_dir, var in self._exit_vars.items():
            val = room.get(f"exit_{exit_dir}") if room else None
            var.set(str(val) if val is not None else "")
        # Load lava state
        if hasattr(self, '_lava_var'):
            self._lava_var.set(room.get("has_lava", False) if room else False)

    def _on_lava_changed(self):
        room = self.current_room_data()
        if room is not None:
            room["has_lava"] = self._lava_var.get()
            self.level_canvas.redraw()

    def _on_exit_changed(self, exit_dir):
        room = self.current_room_data()
        if room is None:
            return
        val = self._exit_vars[exit_dir].get().strip()
        if val == "":
            room[f"exit_{exit_dir}"] = None
        else:
            try:
                room[f"exit_{exit_dir}"] = int(val)
            except ValueError:
                pass

    # ---- Cave constants ----

    def _load_constants_to_entries(self):
        room = self.current_room_data()
        if room is None:
            return
        for key, ent in self._const_entries.items():
            ent.delete(0, tk.END)
            ent.insert(0, str(room["cave_constants"].get(key, 0)))

    def _apply_constants(self):
        room = self.current_room_data()
        if room is None:
            return
        try:
            for key, ent in self._const_entries.items():
                room["cave_constants"][key] = int(ent.get())
            self.level_canvas.redraw()
        except ValueError:
            messagebox.showerror("Invalid input", "Cave constants must be integers.")

    # ---- Edit menu ----

    def _delete_last_polyline(self):
        room = self.current_room_data()
        if room and room["cave_lines"]:
            room["cave_lines"].pop()
            self.level_canvas.redraw()

    def _clear_cave_lines(self):
        room = self.current_room_data()
        if room:
            room["cave_lines"].clear()
            self.level_canvas.redraw()

    # ---- Sprite management ----

    def _refresh_sprite_combo(self):
        names = [s["name"] for s in self.project["sprites"]]
        self._sprite_combo["values"] = names
        if names:
            self._sprite_combo.current(self._current_sprite_idx)

    def _sprite_selected(self, event=None):
        self._current_sprite_idx = self._sprite_combo.current()
        self._current_frame_idx = 0
        self._sync_sprite_bg_scale()
        self.sprite_canvas._bg_path_loaded = None  # force reload for new sprite
        self.sprite_canvas.redraw()
        self._update_vlc_text()
        self._update_frame_label()

    def _sync_sprite_bg_scale(self):
        sprite = self.current_sprite_data()
        if sprite and sprite.get("bg_image"):
            s = sprite["bg_image"].get("scale", 1.0)
            self._sprite_bg_scale.set(s)
            self._sprite_bg_scale_label.config(text=f"{s:.1f}x")
            self._sprite_bg_lock_var.set(sprite["bg_image"].get("locked", False))
            self._sprite_bg_show_var.set(sprite["bg_image"].get("show", True))
        else:
            self._sprite_bg_scale.set(1.0)
            self._sprite_bg_scale_label.config(text="1.0x")
            self._sprite_bg_lock_var.set(False)
            self._sprite_bg_show_var.set(True)

    # ---- Frame management ----

    def _update_frame_label(self):
        sprite = self.current_sprite_data()
        if sprite:
            n = len(sprite["frames"])
            self._frame_label.config(text=f"Frame {self._current_frame_idx + 1} / {n}")
        else:
            self._frame_label.config(text="Frame - / -")

    def _prev_frame(self):
        if self._current_frame_idx > 0:
            self._current_frame_idx -= 1
            self._update_frame_label()
            self.sprite_canvas.redraw()

    def _next_frame(self):
        sprite = self.current_sprite_data()
        if sprite and self._current_frame_idx < len(sprite["frames"]) - 1:
            self._current_frame_idx += 1
            self._update_frame_label()
            self.sprite_canvas.redraw()

    def _add_frame(self):
        sprite = self.current_sprite_data()
        if sprite:
            sprite["frames"].append({"points": []})
            self._current_frame_idx = len(sprite["frames"]) - 1
            self._update_frame_label()
            self.sprite_canvas.redraw()

    def _remove_frame(self):
        sprite = self.current_sprite_data()
        if not sprite or len(sprite["frames"]) <= 1:
            messagebox.showinfo("Info", "Cannot remove the last frame.")
            return
        del sprite["frames"][self._current_frame_idx]
        if self._current_frame_idx >= len(sprite["frames"]):
            self._current_frame_idx = len(sprite["frames"]) - 1
        self._update_frame_label()
        self.sprite_canvas.redraw()

    def _copy_frame(self):
        import copy
        frame = self.current_frame_data()
        sprite = self.current_sprite_data()
        if sprite and frame:
            new_frame = {"points": copy.deepcopy(frame["points"])}
            sprite["frames"].append(new_frame)
            self._current_frame_idx = len(sprite["frames"]) - 1
            self._update_frame_label()
            self.sprite_canvas.redraw()

    def _add_sprite(self):
        s = new_sprite()
        n = len(self.project["sprites"]) + 1
        s["name"] = f"sprite_{n}"
        self.project["sprites"].append(s)
        self._current_sprite_idx = len(self.project["sprites"]) - 1
        self._current_frame_idx = 0
        self._refresh_sprite_combo()
        self._update_frame_label()
        self.sprite_canvas.redraw()
        self._update_vlc_text()

    def _remove_sprite(self):
        if len(self.project["sprites"]) <= 1:
            messagebox.showinfo("Info", "Cannot remove the last sprite.")
            return
        del self.project["sprites"][self._current_sprite_idx]
        self._current_sprite_idx = max(0, self._current_sprite_idx - 1)
        self._current_frame_idx = 0
        self._refresh_sprite_combo()
        self._update_frame_label()
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
        room = self.current_room_data()
        if room:
            self._load_bg_image_for(room, self.level_canvas)
            if room.get("bg_image"):
                self._level_bg_scale.set(room["bg_image"].get("scale", 1.0))
                self._level_bg_scale_label.config(text=f"{room['bg_image']['scale']:.1f}x")

    def _clear_level_bg(self):
        room = self.current_room_data()
        if room:
            room["bg_image"] = None
            self.level_canvas._bg_photo = None
            self.level_canvas._bg_path_loaded = None
            self.level_canvas.redraw()
            self._level_bg_scale.set(1.0)
            self._level_bg_scale_label.config(text="1.0x")

    def _on_level_bg_show(self):
        room = self.current_room_data()
        if room and room.get("bg_image"):
            room["bg_image"]["show"] = self._level_bg_show_var.get()
        self.level_canvas.redraw()

    def _on_level_bg_lock(self):
        room = self.current_room_data()
        if room and room.get("bg_image"):
            room["bg_image"]["locked"] = self._level_bg_lock_var.get()
            self.level_canvas.redraw()

    def _on_level_bg_scale(self, val=None):
        room = self.current_room_data()
        s = round(self._level_bg_scale.get(), 1)
        self._level_bg_scale_label.config(text=f"{s:.1f}x")
        if room and room.get("bg_image"):
            room["bg_image"]["scale"] = s
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

    def _on_sprite_bg_show(self):
        sprite = self.current_sprite_data()
        if sprite and sprite.get("bg_image"):
            sprite["bg_image"]["show"] = self._sprite_bg_show_var.get()
        self.sprite_canvas.redraw()

    def _on_sprite_bg_lock(self):
        sprite = self.current_sprite_data()
        if sprite and sprite.get("bg_image"):
            sprite["bg_image"]["locked"] = self._sprite_bg_lock_var.get()
            self.sprite_canvas.redraw()

    def _on_sprite_bg_scale(self, val=None):
        sprite = self.current_sprite_data()
        s = round(self._sprite_bg_scale.get(), 1)
        self._sprite_bg_scale_label.config(text=f"{s:.1f}x")
        if sprite and sprite.get("bg_image"):
            sprite["bg_image"]["scale"] = s
            self.sprite_canvas.redraw()

    def _clear_sprite_points(self):
        frame = self.current_frame_data()
        if frame:
            frame["points"].clear()
            self.sprite_canvas.redraw()
            self._update_vlc_text()

    def _mirror_sprite_h(self):
        frame = self.current_frame_data()
        if frame:
            for pt in frame["points"]:
                pt[0] = -pt[0]
            self.sprite_canvas.redraw()
            self._update_vlc_text()

    def _mirror_sprite_v(self):
        frame = self.current_frame_data()
        if frame:
            for pt in frame["points"]:
                pt[1] = -pt[1]
            self.sprite_canvas.redraw()
            self._update_vlc_text()

    def _move_sprite_points(self, dx, dy):
        frame = self.current_frame_data()
        if frame:
            for pt in frame["points"]:
                pt[0] += dx
                pt[1] += dy
            self.sprite_canvas.redraw()
            self._update_vlc_text()

    def _move_sprite_left(self):
        self._move_sprite_points(-1, 0)

    def _move_sprite_right(self):
        self._move_sprite_points(1, 0)

    def _move_sprite_up(self):
        self._move_sprite_points(0, -1)

    def _move_sprite_down(self):
        self._move_sprite_points(0, 1)

    def _run_sprite_test(self):
        sprite = self.current_sprite_data()
        if not sprite:
            messagebox.showerror("Error", "No sprite selected")
            return

        frames = sprite["frames"]
        num_frames = len(frames)

        # Compute VLC for all frames
        all_vlcs = []
        max_points = 0
        for fi, fr in enumerate(frames):
            vlc = self.sprite_canvas._compute_vlc(fr)
            if not vlc:
                messagebox.showerror("Error",
                                     f"Frame {fi + 1} needs at least 2 points")
                return
            all_vlcs.append(vlc)
            if vlc[0] > max_points:
                max_points = vlc[0]

        # Generate shape arrays with hw, hh, oy, ox prefix
        shape_decls = []
        for fi, vlc in enumerate(all_vlcs):
            lines = [f"    0, 0, 0, 0,  // hw, hh, oy, ox (sprite test placeholder)",
                     f"    {vlc[0]},"]
            for i in range(1, len(vlc), 2):
                lines.append(f"    {vlc[i]:4d}, {vlc[i+1]:4d},")
            shape_decls.append(
                f"static int8_t shape_{fi}[] = {{\n"
                + "\n".join(lines)
                + "\n};"
            )

        shapes_code = "\n\n".join(shape_decls)
        buf_size = max_points * 2

        if num_frames > 1:
            anim_vars = f"\n#define ANIM_SPEED 8"

            # Generate frame selection using if/else chain
            sel_lines = ["        anim_tick++;",
                         f"        switch ((anim_tick / ANIM_SPEED) % {num_frames}) {{"]
            for fi in range(num_frames):
                sel_lines.append(f"            case {fi}: cur_shape = shape_{fi}; break;")
            sel_lines.append(f"            default: cur_shape = shape_0; break;")
            sel_lines.append("        }")
            anim_logic = "\n".join(sel_lines)

            draw_logic = """\
        if (angle != 0) {
            rot_vl_ab(angle, cur_shape[4] + 1, &cur_shape[5], rotbuf);
            set_scale(scale);
            draw_vl_a(cur_shape[4] + 1, rotbuf);
        } else {
            set_scale(scale);
            draw_vlc(&cur_shape[4]);
        }"""

            extra_vars = "    int8_t *cur_shape;\n    uint8_t anim_tick = 0;\n"
        else:
            anim_vars = ""
            anim_logic = ""
            draw_logic = """\
        if (angle != 0) {
            rot_vl_ab(angle, shape_0[4] + 1, &shape_0[5], rotbuf);
            set_scale(scale);
            draw_vl_a(shape_0[4] + 1, rotbuf);
        } else {
            set_scale(scale);
            draw_vlc(&shape_0[4]);
        }"""
            extra_vars = ""

        src = f"""\
#include <vectrex.h>
#include <vectrex/stdlib.h>

#pragma vx_copyright "2026"
#pragma vx_title "SPRITE"
#pragma vx_music vx_music_1

{shapes_code}
{anim_vars}

static int8_t rotbuf[{buf_size}];

int main(void) {{
    int8_t x = 0, y = 0;
    uint8_t scale = 0x60;
    int8_t angle = 0;
{extra_vars}
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

{anim_logic}

        zero_beam();
        intensity_a(0x7F);
        set_scale(0x7F);
        moveto_d(y, x);

{draw_logic}
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
            self.update_status(f"Running sprite test: {sprite['name']}")
        except Exception as e:
            messagebox.showerror("Emulator Error", str(e))
            self._emu = None

    def _extract_segments(self, cave_lines):
        """Extract line segments from cave polylines."""
        segments = []
        for polyline in cave_lines:
            if len(polyline) < 2:
                continue
            for j in range(1, len(polyline)):
                x1, y1 = int(polyline[j-1][0]), int(polyline[j-1][1])
                x2, y2 = int(polyline[j][0]), int(polyline[j][1])
                segments.append((x1, y1, x2, y2))
        return segments

    def _gen_dynamite_code(self):
        """Return C code for update_dynamite with all walls destroyable."""
        return [
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
            '                                enemies[i].x, enemies[i].y, SPRITE_HW(bat_f0), SPRITE_HH(bat_f0)) {',
            '                    enemies[i].alive = 0;',
            '                    score += 50;',
            '                }',
            '            }',
            '            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,',
            '                            player_x, player_y, SPRITE_HW(player), SPRITE_HH(player)) {',
            '                game_state = STATE_DYING;',
            '                death_timer = 30;',
            '            }',
            '        }',
            '        if (dyn_expl_timer == 0) {',
            '            dyn_active = 0;',
            '            dyn_exploding = 0;',
            '        }',
            '    }',
            '}',
        ]

    def _run_level_test(self):
        import re

        lvl = self.current_level_data()
        if not lvl:
            messagebox.showerror("Error", "No level selected")
            return

        rooms = lvl.get("rooms", [])
        if not rooms:
            messagebox.showerror("Error", "Level has no rooms")
            return

        num_rooms = len(rooms)

        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        src_dir = os.path.join(project_root, "src")
        test_dir = os.path.join(project_root, "test_level")
        os.makedirs(test_dir, exist_ok=True)

        # Check if any room has cave_lines
        any_cave_lines = any(rm.get("cave_lines") for rm in rooms)

        # Generate test_level/hero.h with cave constants from first room.
        # Use wide-open collision boundaries when cave lines exist.
        consts = dict(rooms[0].get("cave_constants", {}))
        if any_cave_lines:
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

        # Build room number -> 0-based index mapping
        room_num_to_idx = {}
        for ri, rm in enumerate(rooms):
            room_num_to_idx[rm["number"]] = ri

        # ---- enemies.c ----
        # src/enemies.c now handles segment collision, miner checking, and
        # all-walls-destroyable dynamite natively — no overrides needed.
        with open(os.path.join(test_dir, "enemies.c"), "w") as f:
            f.write('#include "hero.h"\n#include "../src/enemies.c"\n')

        # ---- player.c ----
        # src/player.c now handles segment collision and per-room bounds natively.
        with open(os.path.join(test_dir, "player.c"), "w") as f:
            f.write('#include "hero.h"\n#include "../src/player.c"\n')

        # ---- drawing.c ----
        # src/drawing.c draw_miner() already checks cur_has_miner — no override needed.
        with open(os.path.join(test_dir, "drawing.c"), "w") as f:
            f.write('#include "hero.h"\n#include "../src/drawing.c"\n')

        # ---- sprites.h ----
        # hero.h now includes sprites.h; provide a forwarding header in test_level/
        with open(os.path.join(test_dir, "sprites.h"), "w") as f:
            f.write('#include "../src/sprites.h"\n')

        # ---- levels.h ----
        lh = ["// Generated by level_editor.py — test level", "",
              "#ifndef LEVELS_H", "#define LEVELS_H", '#include "hero.h"', ""]

        for ri, rm in enumerate(rooms):
            prefix = f"l1r{ri + 1}"
            PREFIX = f"L1R{ri + 1}"
            walls = rm["walls"]
            enemies = rm["enemies"]
            ps = rm.get("player_start")
            miner = rm.get("miner")

            lh.append(f"#define {PREFIX}_WALL_COUNT {len(walls)}")
            if walls:
                lh.append(f"static const int8_t {prefix}_walls[] = {{")
                for w in walls:
                    lh.append(f"    {w['y']}, {w['x']}, {w['h']}, {w['w']},")
                lh.append("};")
            else:
                lh.append(f"static const int8_t {prefix}_walls[] = {{ 0, 0, 0, 0 }};")

            lh.append(f"#define {PREFIX}_ENEMY_COUNT {len(enemies)}")
            if enemies:
                lh.append(f"static const int8_t {prefix}_enemies[] = {{")
                for e in enemies:
                    type_int = 1 if e.get("type", "bat") == "spider" else 0
                    lh.append(f"    {e['x']}, {e['y']}, {e['vx']}, {type_int},")
                lh.append("};")
            else:
                lh.append(f"static const int8_t {prefix}_enemies[] = {{ 0, 0, 0, 0 }};")

            start_x = ps["x"] if ps else consts.get("CAVE_LEFT", -90) + 15
            start_y = ps["y"] if ps else consts.get("CAVE_TOP", 105) - 2
            miner_x = miner["x"] if miner else 0
            miner_y = miner["y"] if miner else consts.get("CAVE_FLOOR", -95) + 8
            lh.append(f"#define {PREFIX}_START_X  {start_x}")
            lh.append(f"#define {PREFIX}_START_Y  {start_y}")
            lh.append(f"#define {PREFIX}_MINER_X  {miner_x}")
            lh.append(f"#define {PREFIX}_MINER_Y  {miner_y}")

            # Cave polyline data
            cave_lines = rm.get("cave_lines", [])
            data = cave_room_data(cave_lines)
            lh.append(f"static const int8_t {prefix}_cave[] = {{")
            for k in range(0, len(data), 12):
                chunk = data[k:k+12]
                lh.append("    " + ", ".join(str(v) for v in chunk) + ",")
            lh.append("};")

            # Cave segments (for collision)
            segs = extract_segments(cave_lines)
            lh.append(f"#define {PREFIX}_SEG_COUNT {len(segs)}")
            if segs:
                lh.append(f"static const int8_t {prefix}_cave_segs[] = {{")
                for s in segs:
                    lh.append(f"    {s[0]}, {s[1]}, {s[2]}, {s[3]},")
                lh.append("};")
            else:
                lh.append(f"static const int8_t {prefix}_cave_segs[] = {{ 0, 0, 0, 0 }};")
            lh.append("")

        # Room lookup tables
        lh.append("// Room lookup tables")
        lh.append(f"#define NUM_ROOMS {num_rooms}")
        lh.append("static const int8_t * const l1_room_caves[] = {")
        for ri in range(num_rooms):
            lh.append(f"    l1r{ri+1}_cave,")
        lh.append("};")
        lh.append("static const int8_t * const l1_room_walls[] = {")
        for ri in range(num_rooms):
            lh.append(f"    l1r{ri+1}_walls,")
        lh.append("};")
        lh.append("static const uint8_t l1_room_wall_counts[] = {")
        for ri in range(num_rooms):
            lh.append(f"    L1R{ri+1}_WALL_COUNT,")
        lh.append("};")
        lh.append("static const int8_t * const l1_room_enemies[] = {")
        for ri in range(num_rooms):
            lh.append(f"    l1r{ri+1}_enemies,")
        lh.append("};")
        lh.append("static const uint8_t l1_room_enemy_counts[] = {")
        for ri in range(num_rooms):
            lh.append(f"    L1R{ri+1}_ENEMY_COUNT,")
        lh.append("};")
        lh.append("static const int8_t l1_room_starts[] = {")
        for ri in range(num_rooms):
            lh.append(f"    L1R{ri+1}_START_X, L1R{ri+1}_START_Y,")
        lh.append("};")
        lh.append("static const int8_t l1_room_miners[] = {")
        for ri in range(num_rooms):
            lh.append(f"    L1R{ri+1}_MINER_X, L1R{ri+1}_MINER_Y,")
        lh.append("};")
        lh.append("static const int8_t * const l1_room_cave_segs[] = {")
        for ri in range(num_rooms):
            lh.append(f"    l1r{ri+1}_cave_segs,")
        lh.append("};")
        lh.append("static const uint8_t l1_room_seg_counts[] = {")
        for ri in range(num_rooms):
            lh.append(f"    L1R{ri+1}_SEG_COUNT,")
        lh.append("};")
        lh.append("static const uint8_t l1_room_has_miner[] = {")
        for ri, rm in enumerate(rooms):
            lh.append(f"    {1 if rm.get('miner') else 0},")
        lh.append("};")
        lh.append("static const uint8_t l1_room_has_lava[] = {")
        for ri, rm in enumerate(rooms):
            lh.append(f"    {1 if rm.get('has_lava') else 0},")
        lh.append("};")

        # Room exits table (convert 1-based room numbers to 0-based indices)
        lh.append("static const uint8_t l1_room_exits[] = {")
        for ri, rm in enumerate(rooms):
            exits = []
            for exit_key in ("exit_left", "exit_right", "exit_top", "exit_bottom"):
                room_num = rm.get(exit_key)
                if room_num is not None and room_num in room_num_to_idx:
                    exits.append(str(room_num_to_idx[room_num]))
                else:
                    exits.append("NONE")
            lh.append(f"    {', '.join(exits)},   // room {ri}")
        lh.append("};")

        # Room bounds (left, right, top, floor per room)
        lh.append("static const int8_t l1_room_bounds[] = {")
        for ri, rm in enumerate(rooms):
            rc = rm.get("cave_constants", {})
            bl = max(-128, min(127, int(rc.get("CAVE_LEFT", -90))))
            br = max(-128, min(127, int(rc.get("CAVE_RIGHT", 90))))
            bt = max(-128, min(127, int(rc.get("CAVE_TOP", 105))))
            bf = max(-128, min(127, int(rc.get("CAVE_FLOOR", -95))))
            lh.append(f"    {bl}, {br}, {bt}, {bf},   // room {ri}")
        lh.append("};")

        lh.extend(["", "#endif", ""])
        with open(os.path.join(test_dir, "levels.h"), "w") as f:
            f.write("\n".join(lh))

        # ---- levels.c ----
        lc = f"""\
#include "hero.h"
#include "levels.h"

const int8_t *room_cave_lines[MAX_ROOMS];
const int8_t *room_cave_segs[MAX_ROOMS];
uint8_t room_seg_counts[MAX_ROOMS];
uint8_t room_has_miner[MAX_ROOMS];
uint8_t room_has_lava[MAX_ROOMS];
const int8_t *room_walls[MAX_ROOMS];
uint8_t room_wall_counts[MAX_ROOMS];
const int8_t *room_enemies_data[MAX_ROOMS];
uint8_t room_enemy_counts[MAX_ROOMS];
int8_t room_starts[MAX_ROOMS * 2];
int8_t room_miners[MAX_ROOMS * 2];
uint8_t room_exits[MAX_ROOMS * 4];
int8_t room_bounds[MAX_ROOMS * 4];

void set_level_data(void) {{
    uint8_t i;
    for (i = 0; i < NUM_ROOMS; i++) {{
        room_cave_lines[i] = l1_room_caves[i];
        room_cave_segs[i] = l1_room_cave_segs[i];
        room_seg_counts[i] = l1_room_seg_counts[i];
        room_has_miner[i] = l1_room_has_miner[i];
        room_has_lava[i] = l1_room_has_lava[i];
        room_walls[i] = l1_room_walls[i];
        room_wall_counts[i] = l1_room_wall_counts[i];
        room_enemies_data[i] = l1_room_enemies[i];
        room_enemy_counts[i] = l1_room_enemy_counts[i];
        room_starts[i * 2] = l1_room_starts[i * 2];
        room_starts[i * 2 + 1] = l1_room_starts[i * 2 + 1];
        room_miners[i * 2] = l1_room_miners[i * 2];
        room_miners[i * 2 + 1] = l1_room_miners[i * 2 + 1];
        room_exits[i * 4] = l1_room_exits[i * 4];
        room_exits[i * 4 + 1] = l1_room_exits[i * 4 + 1];
        room_exits[i * 4 + 2] = l1_room_exits[i * 4 + 2];
        room_exits[i * 4 + 3] = l1_room_exits[i * 4 + 3];
        room_bounds[i * 4] = l1_room_bounds[i * 4];
        room_bounds[i * 4 + 1] = l1_room_bounds[i * 4 + 1];
        room_bounds[i * 4 + 2] = l1_room_bounds[i * 4 + 2];
        room_bounds[i * 4 + 3] = l1_room_bounds[i * 4 + 3];
    }}
    set_room_data();
}}

void set_room_data(void) {{
    cur_cave_lines = room_cave_lines[current_room];
    cur_cave_segs = room_cave_segs[current_room];
    cur_seg_count = room_seg_counts[current_room];
    cur_cave_left = room_bounds[current_room * 4];
    cur_cave_right = room_bounds[current_room * 4 + 1];
    cur_cave_top = room_bounds[current_room * 4 + 2];
    cur_cave_floor = room_bounds[current_room * 4 + 3];
    cur_has_miner = room_has_miner[current_room];
    cur_has_lava = room_has_lava[current_room];
    cur_walls = room_walls[current_room];
    cur_wall_count = room_wall_counts[current_room];
    cur_enemies_data = room_enemies_data[current_room];
    cur_enemy_count = room_enemy_counts[current_room];
    cur_miner_x = room_miners[current_room * 2];
    cur_miner_y = room_miners[current_room * 2 + 1];
}}

void load_enemies(void) {{
    uint8_t i;
    enemy_count = cur_enemy_count;
    for (i = 0; i < MAX_ENEMIES; i++) {{
        if (i < enemy_count) {{
            enemies[i].x = cur_enemies_data[i * 4];
            enemies[i].y = cur_enemies_data[i * 4 + 1];
            enemies[i].vx = cur_enemies_data[i * 4 + 2];
            enemies[i].type = (uint8_t)cur_enemies_data[i * 4 + 3];
            enemies[i].home_y = enemies[i].y;
            enemies[i].alive = 1;
            enemies[i].anim = 0;
        }} else {{
            enemies[i].alive = 0;
        }}
    }}
    walls_destroyed = 0;
}}

void init_level(void) {{
    uint8_t j;
    current_room = 0;
    for (j = 0; j < MAX_ROOMS; j++) room_walls_destroyed[j] = 0;
    set_level_data();
    player_x = room_starts[0];
    player_y = room_starts[1];
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
}}

void start_new_game(void) {{
    score = 0;
    player_lives = START_LIVES;
    player_fuel = START_FUEL;
    player_dynamite = START_DYNAMITE;
    current_level = 0;
    init_level();
    game_state = STATE_PLAYING;
}}
"""
        with open(os.path.join(test_dir, "levels.c"), "w") as f:
            f.write(lc)

        # ---- main.c ----
        # src functions already check cur_has_miner internally
        mc_miner_rescue = "            check_miner_rescue();\n"
        mc_lava_death = """\
            // Lava death check
            if (cur_has_lava && player_y - SPRITE_HH(player) <= cur_cave_floor + LAVA_HEIGHT) {{
                game_state = STATE_DYING;
                death_timer = 30;
            }}
"""
        mc_draw_miner = "            draw_miner();\n"
        mc_draw_lava = "            draw_lava();\n"
        mc = f"""\
#include "hero.h"

#pragma vx_copyright "2026"
#pragma vx_title "LEVEL TEST"
#pragma vx_music vx_music_1

int8_t player_x, player_y, player_vx, player_vy, player_facing;
uint8_t player_fuel, player_dynamite, player_lives;
uint8_t player_on_ground, player_thrusting, anim_tick;
int score;
uint8_t game_state, current_level, current_room, death_timer, level_msg_timer;
Enemy enemies[MAX_ENEMIES];
uint8_t enemy_count;
uint8_t laser_active;
int8_t laser_x, laser_y, laser_dir;
uint8_t laser_timer;
uint8_t dyn_active;
int8_t dyn_x, dyn_y;
uint8_t dyn_timer, dyn_exploding, dyn_expl_timer;
uint8_t walls_destroyed;
uint8_t room_walls_destroyed[MAX_ROOMS];
const int8_t *cur_cave_lines;
const int8_t *cur_cave_segs;
uint8_t cur_seg_count;
int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
uint8_t cur_has_miner;
uint8_t cur_has_lava;
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
    return box_overlap(player_x, player_y, SPRITE_HW(player), SPRITE_HH(player),
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
{mc_miner_rescue}{mc_lava_death}
            // Check room exits using per-room cave bounds
            {{
                uint8_t exit_room = NONE;
                if (player_x <= room_bounds[current_room * 4 + 0]) {{
                    exit_room = room_exits[current_room * 4 + 0];
                    if (exit_room != NONE) player_x = room_bounds[exit_room * 4 + 1];
                }} else if (player_x >= room_bounds[current_room * 4 + 1]) {{
                    exit_room = room_exits[current_room * 4 + 1];
                    if (exit_room != NONE) player_x = room_bounds[exit_room * 4 + 0];
                }} else if (player_y >= room_bounds[current_room * 4 + 2]) {{
                    exit_room = room_exits[current_room * 4 + 2];
                    if (exit_room != NONE) player_y = room_bounds[exit_room * 4 + 3];
                }} else if (player_y <= room_bounds[current_room * 4 + 3]) {{
                    exit_room = room_exits[current_room * 4 + 3];
                    if (exit_room != NONE) player_y = room_bounds[exit_room * 4 + 2];
                }}
                if (exit_room != NONE) {{
                    room_walls_destroyed[current_room] = walls_destroyed;
                    current_room = exit_room;
                    set_room_data();
                    load_enemies();
                    walls_destroyed = room_walls_destroyed[current_room];
                }}
            }}

            draw_cave();
{mc_draw_lava}            draw_enemies();
            draw_dynamite_and_explosion();
            draw_laser_beam();
            draw_player();
{mc_draw_miner}            draw_hud();
            draw_fuel_bar();

            if (level_msg_timer > 0) {{
                zero_beam();
                str_buf[0] = 'L'; str_buf[1] = 'E'; str_buf[2] = 'V';
                str_buf[3] = 'E'; str_buf[4] = 'L'; str_buf[5] = ' ';
                int_to_str((int)(current_level + 1), 6);
                draw_text(100, -35, str_buf, 0x50, 10);
                level_msg_timer--;
            }}
        }}
        else if (game_state == STATE_DYING) {{
            death_timer--;
            if (death_timer & 2) draw_player();
            draw_cave();
            draw_lava();
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
            draw_lava();
            zero_beam();
            set_scale(0x7F);
            draw_text(100, -35, "RESCUED", 0x50, 10);
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
               os.path.join(test_dir, "levels.c"),
               os.path.join(src_dir, "sprites.c"),
               os.path.join(src_dir, "font.c")]

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
            self.update_status(f"Running level test: Level {self._current_level_idx + 1}")
        except Exception as e:
            messagebox.showerror("Emulator Error", str(e))
            self._emu = None

    def _update_vlc_text(self):
        sprite = self.current_sprite_data()
        frame = self.current_frame_data()
        self._vlc_text.config(state="normal")
        self._vlc_text.delete("1.0", "end")
        if sprite and frame:
            vlc = self.sprite_canvas._compute_vlc(frame)
            if vlc:
                name = sprite["name"]
                if len(sprite["frames"]) > 1:
                    name = f"{name}_f{self._current_frame_idx}"
                lines = [f"static int8_t {name}[] = {{"]
                lines.append(f"    {vlc[0]},")
                for i in range(1, len(vlc), 2):
                    dy, dx = vlc[i], vlc[i + 1]
                    lines.append(f"    {dy:4d}, {dx:4d},")
                lines.append("};")
                # Bounding box info
                pts = frame["points"]
                if pts:
                    xs = [p[0] for p in pts]
                    ys = [p[1] for p in pts]
                    hw = max(abs(min(xs)), abs(max(xs)))
                    hh = max(abs(min(ys)), abs(max(ys)))
                    lines.append(f"")
                    lines.append(f"// BBox: HW={hw}  HH={hh}")
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
        self._current_room_idx = 0
        self._current_sprite_idx = 0
        self._current_frame_idx = 0
        self._save_path = None
        self._refresh_level_combo()
        self._update_room_label()
        self._load_exit_fields()
        self._refresh_sprite_combo()
        self._load_constants_to_entries()
        self.level_canvas.redraw()
        self.sprite_canvas.redraw()
        self._update_frame_label()

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
            self.project = migrate_project(data)
            self._save_path = path
            self._current_level_idx = 0
            self._current_room_idx = 0
            self._current_sprite_idx = 0
            self._current_frame_idx = 0
            self._refresh_level_combo()
            self._update_room_label()
            self._load_exit_fields()
            self._refresh_sprite_combo()
            self._load_constants_to_entries()
            self.level_canvas.redraw()
            self.sprite_canvas.redraw()
            self._update_frame_label()
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
            self._export_sprites(os.path.join(folder, "sprites.h"),
                                 os.path.join(folder, "sprites.c"))
            self.update_status(f"Exported to {folder}/")
            messagebox.showinfo("Export Complete",
                                f"Exported:\n  {folder}/levels.h\n  {folder}/sprites.h\n  {folder}/sprites.c")
        except Exception as e:
            messagebox.showerror("Export Error", str(e))

    def _export_levels_h(self, path):
        lines = [
            "// Generated by level_editor.py",
            "// Vectrex H.E.R.O. level data",
            "",
            "#ifndef LEVELS_H",
            "#define LEVELS_H",
            '#include "hero.h"',
            "",
        ]
        for i, lvl in enumerate(self.project["levels"]):
            li = i + 1  # 1-based level number
            rooms = lvl.get("rooms", [])
            lines.append(f"// {'=' * 60}")
            lines.append(f"// Level {li} ({len(rooms)} room(s))")
            lines.append(f"// {'=' * 60}")
            lines.append("")

            # Build room number -> 0-based index mapping
            room_num_to_idx = {}
            for ri, rm in enumerate(rooms):
                room_num_to_idx[rm["number"]] = ri

            # Per-room data arrays
            for ri, rm in enumerate(rooms):
                rj = ri + 1  # 1-based room number for naming
                prefix = f"l{li}r{rj}"
                PREFIX = f"L{li}R{rj}"

                # Walls
                walls = rm["walls"]
                lines.append(f"#define {PREFIX}_WALL_COUNT {len(walls)}")
                if walls:
                    lines.append(f"static const int8_t {prefix}_walls[] = {{")
                    for j, w in enumerate(walls):
                        destr = " (destroyable)" if w.get("destroyable", False) else ""
                        lines.append(f"    {w['y']}, {w['x']}, {w['h']}, {w['w']},"
                                     f"   // wall {j}{destr}")
                    lines.append("};")
                else:
                    lines.append(f"static const int8_t {prefix}_walls[] = {{ 0, 0, 0, 0 }};")
                lines.append("")

                # Enemies
                enemies = rm["enemies"]
                lines.append(f"#define {PREFIX}_ENEMY_COUNT {len(enemies)}")
                if enemies:
                    lines.append(f"static const int8_t {prefix}_enemies[] = {{")
                    for j, e in enumerate(enemies):
                        etype = e.get("type", "bat")
                        type_int = 1 if etype == "spider" else 0
                        lines.append(f"    {e['x']}, {e['y']}, {e['vx']}, {type_int},   // enemy {j} ({etype})")
                    lines.append("};")
                else:
                    lines.append(f"static const int8_t {prefix}_enemies[] = {{ 0, 0, 0, 0 }};")
                lines.append("")

                # Player start
                ps = rm.get("player_start")
                consts = rm.get("cave_constants", {})
                start_x = ps["x"] if ps else consts.get("CAVE_LEFT", -90) + 15
                start_y = ps["y"] if ps else consts.get("CAVE_TOP", 105) - 2
                lines.append(f"#define {PREFIX}_START_X  {start_x}")
                lines.append(f"#define {PREFIX}_START_Y  {start_y}")

                # Miner
                m = rm.get("miner")
                miner_x = m["x"] if m else 0
                miner_y = m["y"] if m else consts.get("CAVE_FLOOR", -95) + 8
                lines.append(f"#define {PREFIX}_MINER_X  {miner_x}")
                lines.append(f"#define {PREFIX}_MINER_Y  {miner_y}")
                lines.append("")

                # Cave polyline data (for drawing)
                cave_lines = rm.get("cave_lines", [])
                data = cave_room_data(cave_lines)
                lines.append(f"static const int8_t {prefix}_cave[] = {{")
                for k in range(0, len(data), 12):
                    chunk = data[k:k+12]
                    lines.append("    " + ", ".join(str(v) for v in chunk) + ",")
                lines.append("};")

                # Cave segments (for collision): x1, y1, x2, y2 per segment
                segs = extract_segments(cave_lines)
                lines.append(f"#define {PREFIX}_SEG_COUNT {len(segs)}")
                if segs:
                    lines.append(f"static const int8_t {prefix}_cave_segs[] = {{")
                    for s in segs:
                        lines.append(f"    {s[0]}, {s[1]}, {s[2]}, {s[3]},")
                    lines.append("};")
                else:
                    lines.append(f"static const int8_t {prefix}_cave_segs[] = {{ 0, 0, 0, 0 }};")
                lines.append("")

            # Room lookup tables for this level
            lp = f"l{li}"
            nr = len(rooms)
            lines.append(f"// Room lookup tables for level {li}")
            lines.append(f"static const int8_t * const {lp}_room_caves[] = {{")
            for ri in range(nr):
                lines.append(f"    {lp}r{ri+1}_cave,")
            lines.append("};")

            lines.append(f"static const int8_t * const {lp}_room_walls[] = {{")
            for ri in range(nr):
                lines.append(f"    {lp}r{ri+1}_walls,")
            lines.append("};")

            lines.append(f"static const uint8_t {lp}_room_wall_counts[] = {{")
            for ri in range(nr):
                lines.append(f"    L{li}R{ri+1}_WALL_COUNT,")
            lines.append("};")

            lines.append(f"static const int8_t * const {lp}_room_enemies[] = {{")
            for ri in range(nr):
                lines.append(f"    {lp}r{ri+1}_enemies,")
            lines.append("};")

            lines.append(f"static const uint8_t {lp}_room_enemy_counts[] = {{")
            for ri in range(nr):
                lines.append(f"    L{li}R{ri+1}_ENEMY_COUNT,")
            lines.append("};")

            lines.append(f"static const int8_t {lp}_room_starts[] = {{")
            for ri in range(nr):
                lines.append(f"    L{li}R{ri+1}_START_X, L{li}R{ri+1}_START_Y,")
            lines.append("};")

            lines.append(f"static const int8_t {lp}_room_miners[] = {{")
            for ri in range(nr):
                lines.append(f"    L{li}R{ri+1}_MINER_X, L{li}R{ri+1}_MINER_Y,")
            lines.append("};")

            lines.append(f"static const int8_t * const {lp}_room_cave_segs[] = {{")
            for ri in range(nr):
                lines.append(f"    {lp}r{ri+1}_cave_segs,")
            lines.append("};")

            lines.append(f"static const uint8_t {lp}_room_seg_counts[] = {{")
            for ri in range(nr):
                lines.append(f"    L{li}R{ri+1}_SEG_COUNT,")
            lines.append("};")

            lines.append(f"static const uint8_t {lp}_room_has_miner[] = {{")
            for ri, rm in enumerate(rooms):
                lines.append(f"    {1 if rm.get('miner') else 0},")
            lines.append("};")

            lines.append(f"static const uint8_t {lp}_room_has_lava[] = {{")
            for ri, rm in enumerate(rooms):
                lines.append(f"    {1 if rm.get('has_lava') else 0},")
            lines.append("};")

            # Room exits
            lines.append(f"static const uint8_t {lp}_room_exits[] = {{")
            for ri, rm in enumerate(rooms):
                exits = []
                for exit_key in ("exit_left", "exit_right", "exit_top", "exit_bottom"):
                    room_num = rm.get(exit_key)
                    if room_num is not None and room_num in room_num_to_idx:
                        exits.append(str(room_num_to_idx[room_num]))
                    else:
                        exits.append("NONE")
                lines.append(f"    {', '.join(exits)},   // room {ri}")
            lines.append("};")

            # Room bounds (left, right, top, floor per room)
            lines.append(f"static const int8_t {lp}_room_bounds[] = {{")
            for ri, rm in enumerate(rooms):
                rc = rm.get("cave_constants", {})
                bl = max(-128, min(127, int(rc.get("CAVE_LEFT", -90))))
                br = max(-128, min(127, int(rc.get("CAVE_RIGHT", 90))))
                bt = max(-128, min(127, int(rc.get("CAVE_TOP", 105))))
                bf = max(-128, min(127, int(rc.get("CAVE_FLOOR", -95))))
                lines.append(f"    {bl}, {br}, {bt}, {bf},   // room {ri}")
            lines.append("};")


        lines.append("#endif")

        with open(path, "w") as f:
            f.write("\n".join(lines) + "\n")

    def _export_sprites(self, h_path, c_path):
        """Export sprites.h (extern declarations) and sprites.c (definitions)."""
        h_lines = [
            "// Generated by level_editor.py",
            "// Vectrex H.E.R.O. VLC sprite data",
            "",
            "#ifndef SPRITES_H",
            "#define SPRITES_H",
            "",
        ]
        c_lines = [
            "// Generated by level_editor.py",
            "// Vectrex H.E.R.O. VLC sprite data",
            "",
            "#include <vectrex.h>",
            "",
        ]
        # Scale mapping: sprite name -> draw scale
        scale_map = {"player_right": 0x6A, "player_left": 0x6A, "dynamite": 0x30}
        default_scale = 0x60

        for sprite in self.project["sprites"]:
            name = sprite["name"]
            frames = sprite.get("frames", [])
            multi = len(frames) > 1
            draw_scale = scale_map.get(name, default_scale)

            # Compute bounding box across all frames (raw coords)
            raw_hw = 0
            raw_hh = 0
            for fr in frames:
                pts = fr["points"]
                if len(pts) < 2:
                    continue
                xs = [p[0] for p in pts]
                ys = [p[1] for p in pts]
                fhw = max(abs(min(xs)), abs(max(xs)))
                fhh = max(abs(min(ys)), abs(max(ys)))
                if fhw > raw_hw:
                    raw_hw = fhw
                if fhh > raw_hh:
                    raw_hh = fhh

            # Screen-space scaled half-widths (round to nearest)
            hw = round(raw_hw * draw_scale / 0x7F)
            hh = round(raw_hh * draw_scale / 0x7F)

            for fi, frame in enumerate(frames):
                pts = frame["points"]
                if len(pts) < 2:
                    continue
                arr_name = f"{name}_f{fi}" if multi else name
                count = len(pts) - 1
                h_lines.append(f"extern int8_t {arr_name}[];")
                oy = clamp(pts[0][1])  # offset from center to VLC start
                ox = clamp(pts[0][0])
                c_lines.append(f"int8_t {arr_name}[] = {{")
                c_lines.append(f"    {hw}, {hh},  // hw, hh (scaled: raw {raw_hw},{raw_hh} x 0x{draw_scale:02X}/0x7F)")
                c_lines.append(f"    {oy}, {ox},  // oy, ox (center to VLC start)")
                c_lines.append(f"    {count - 1},  // {count} vectors (VLC count-1)")
                for j in range(1, len(pts)):
                    dy = clamp(pts[j][1] - pts[j - 1][1])
                    dx = clamp(pts[j][0] - pts[j - 1][0])
                    c_lines.append(f"    {dy:4d}, {dx:4d},")
                c_lines.append("};")
                c_lines.append("")

        h_lines.extend(["", "#endif", ""])
        with open(h_path, "w") as f:
            f.write("\n".join(h_lines) + "\n")
        with open(c_path, "w") as f:
            f.write("\n".join(c_lines) + "\n")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _run_plain(args):
    """Emulator-only mode: no editors, no CPU state polling."""
    rom = args.rom
    cart = args.cart
    if not rom or not cart:
        print("--plain requires both --rom and --cart")
        sys.exit(1)
    if not os.path.isfile(rom):
        print(f"ROM not found: {rom}")
        sys.exit(1)
    if not os.path.isfile(cart):
        print(f"Cart not found: {cart}")
        sys.exit(1)

    root = tk.Tk()
    root.title("Vectrex H.E.R.O.")
    container = tk.Frame(root, bg="#000000")
    container.pack(fill="both", expand=True)
    emu = Emulator(rom, cart, parent=container)
    emu.start()
    root.mainloop()


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Vectrex H.E.R.O. Level & Sprite Editor")
    parser.add_argument('--project', default=None, help='Path to project JSON file to load at startup')
    parser.add_argument('--rom', default=None, help='Path to Vectrex system ROM')
    parser.add_argument('--cart', default=None, help='Path to cartridge ROM')
    parser.add_argument('--plain', action='store_true', help='Emulator only, no editors or CPU state')
    args = parser.parse_args()

    if args.plain:
        _run_plain(args)
        return

    root = tk.Tk()
    root.geometry("1300x800")
    root.minsize(1300, 800)
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
