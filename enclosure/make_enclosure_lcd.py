#!/usr/bin/env python3
"""Wall-mount enclosure for the 16x2 HD44780 variant (branch feature/lcd1602-display). Prototype, battery powered.

    pip install manifold3d numpy
    python3 enclosure/make_enclosure_lcd.py        # writes enclosure/stl_lcd/*.stl and *.json
    python3 enclosure/check_fit_lcd.py             # interference check of the placeholder components

The square 78 mm case of make_enclosure.py cannot hold a 80 x 36 mm 1602 module, so this is a wider case: 98 x 56 mm.
Same conventions as the square version: Z = 0 is the FRONT face on the print bed, Z grows toward the back, Y up, X(u) turns
"distance from the viewer's left edge" into model X. The lid is printed inner face down.

Inside, front to back:
  z  0 - 2      front plate (window 66 x 16, two label pockets: logo on top, team name below)
  z  2 - 11.5   LCD module on 4 M3 posts (glass/bezel against the plate, PCB at the back)
  z 11.5 - 20   I2C backpack (PCF8574) over the upper part of the PCB back
  z 20.5 - 27.4 layer 1: ESP32-C3 SuperMini, TP4056, DS3231 (USB-C slots in the bottom wall)
  z 27.8 - 37.8 layer 2: LiPo battery in a cradle, DFPlayer Mini (microSD slot in the right wall)
Buttons POWER / VOL- / VOL+ are tact switches with long plungers on the TOP edge, above the module (see the square version).
EVERY component size is a typical value from memory, NOT measured. Measure your parts and edit the numbers.
"""
import json
import math
import pathlib
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

import brand

# ---- outer shape -------------------------------------------------------------------------------------
W, H = 98.0, 56.0   # width (X) and height (Y)
R = 5.0             # outer corner radius
T = 2.0             # wall thickness
FRONT_T = 2.0       # front plate thickness
BODY_D = 39.0       # body depth (front face to rear rim)
LID_T = 4.5         # lid thickness (3 mm magnets + a 1.3 mm skin)
R_SEG = 48


def X(u):
    """u = distance from the viewer's left edge when looking at the front -> model X."""
    return W - u


CX = W / 2          # horizontal centre in u

# ---- LCD module (1602, typical 80 x 36 mm PCB) ------------------------------------------------------------------
MOD_W, MOD_H = 80.0, 36.0
MOD_V = 22.0                     # PCB centre height
WIN_W, WIN_H = 66.0, 16.0        # viewing window (active area is about 56 x 11.5 mm)
MOD_HOLES = (75.0, 31.0)         # mounting hole spacing (x, y), M3
MOD_POST_D, MOD_POST_H, MOD_PILOT = 5.4, 7.9, 2.5   # posts from the plate to the PCB front face; pilot for M3 screws
MOD_PCB_Z = (FRONT_T + MOD_POST_H, FRONT_T + MOD_POST_H + 1.6)      # PCB front/back face (9.9 .. 11.5)

# ---- brand label pockets -------------------------------------------------------------------------------------
LABEL_DEPTH = 0.6
INLAY_GAP = 0.2
# Pockets follow the real outlines: the Bank Respublika logo (brand/BR_digital_logo.svg) above the window, "Retail Loan Team"
# (Outfit SemiBold, closest free font by eye, not the bank's brand font) below it. The pocket is the outline grown by INLAY_GAP;
# the inlay is the outline itself, printed in brand blue #3327FF.
TOP_LABEL = dict(u=CX, v=42.5, w=39.4, kind='logo')
LOW_LABEL = dict(u=CX, v=8.5, w=48.0, kind='text')

# ---- speaker (left wall, faces sideways) --------------------------------------------------------------------------
SPK_D = 20.0
SPK_V, SPK_Z = 26.0, 24.0
SPK_HOLE = 1.8
SPK_RING_H = 5.0

# ---- openings -----------------------------------------------------------------------------------------------------------------
USBC = (10.0, 4.0)
USB_MCU = dict(u=19.0, z=23.7)    # ESP32-C3 SuperMini (programming)
USB_CHG = dict(u=43.0, z=24.2)    # TP4056 (charging)
SD_SLOT = (15.0, 3.0)             # microSD slot in the RIGHT wall (v length, z height)
SD_V, SD_Z = 33.0, 31.0

# ---- screws, magnets, keyhole ---------------------------------------------------------------------------------------------
POST_D, POST_HOLE, POST_HOLE_DEPTH = 5.6, 2.6, 12.0
LID_HOLE, LID_CSK_D = 3.4, 6.4
MAG_D, MAG_H = 10.2, 3.2
MAG_POS = [(18.0, 14.0), (80.0, 14.0), (18.0, 42.0), (80.0, 42.0)]   # (u, v)
KEY_HEAD_D, KEY_SLOT_W = 9.0, 4.5
KEY_U, KEY_V, KEY_SLOT_LEN = CX, 47.0, 4.0

POST_OFF = T + 2.9
POSTS = [(X(POST_OFF), POST_OFF), (X(W - POST_OFF), POST_OFF),
         (X(POST_OFF), H - POST_OFF), (X(W - POST_OFF), H - POST_OFF)]

# ---- battery cradle (flat LiPo, e.g. 103450 = 10 x 34 x 50 mm; measure yours) -----------------------------------
BAT = dict(u0=12.0, u1=62.0, v0=6.0, v1=40.0, z0=27.8, z1=37.8)
BAT_GAP, BAT_WALL, BAT_Z_TOP = 0.6, 1.2, 38.3
BAT_RIBS_U = (33.0, 36.0)           # ribs to the bottom and top walls

# ---- buttons: top wall, three 6x6 tact switches with long plungers (above the module) ------------------------------
BTN_Z = 14.1
BTN_FRAME = 12.0
BTN_POCKET = 6.4
BTN_HOLE = 4.2
BTN_STRIP_TOP = H - T - 8.0
BTN_BODY_H = 3.5
BTN_CEIL = BTN_STRIP_TOP + BTN_BODY_H + 0.5
BTN_PILOT_D = 1.8
BUTTONS = {
    'pwr': dict(u=20.0, H=9.0, label='power'),     # tip 1 mm below the surface, in a counterbore
    'vdn': dict(u=68.0, H=11.0, label='minus'),    # tip 1 mm above the surface
    'vup': dict(u=82.0, H=11.0, label='plus'),
}
PWR_CBORE_D, PWR_CBORE_DEPTH = 8.0, 0.6
MARK_Z, MARK_DEPTH = BTN_Z + 8.9, 0.4


# ---- helpers ------------------------------------------------------------------------------------------------------------------
def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl_z(x, y, z0, z1, d, d2=None):
    return Manifold.cylinder(z1 - z0, d / 2, (d2 if d2 is not None else d) / 2, R_SEG).translate([x, y, z0])


def cyl_y(x, y0, y1, z, d):
    """Cylinder along Y from y0 to y1, centred on (x, z)."""
    return Manifold.cylinder(y1 - y0, d / 2, d / 2, R_SEG).rotate([-90, 0, 0]).translate([x, y0, z])


def ubox(u0, u1, v0, v1, z0, z1):
    return box(X(u1), X(u0), v0, v1, z0, z1)


def cyl_x(x0, x1, y, z, d):
    """Cylinder along X from x0 to x1, centred on (y, z)."""
    return Manifold.cylinder(x1 - x0, d / 2, d / 2, R_SEG).rotate([0, 90, 0]).translate([x0, y, z])


def union(parts):
    acc = parts[0]
    for p in parts[1:]:
        acc = acc + p
    return acc


def outline():
    return CrossSection.square([W, H]).offset(-R, JoinType.Miter, 2.0, 0).offset(R, JoinType.Round, 2.0, R_SEG)


def label_shape(L):
    """Brand outline as a CrossSection centred on (0, 0), already mirrored for the model's X axis."""
    cs = brand.logo(L['w']) if L['kind'] == 'logo' else brand.team_text(L['w'])
    return cs.mirror([1, 0])


def label_cut(L):
    x = X(L['u'])
    pocket = label_shape(L).offset(INLAY_GAP, JoinType.Round, 2.0, 16)
    return Manifold.extrude(pocket, LABEL_DEPTH + 1).translate([x, L['v'], -1])




def battery_cradle():
    b, a = BAT, BAT_GAP
    u0, u1, v0, v1 = b['u0'] - a, b['u1'] + a, b['v0'] - a, b['v1'] + a
    z0 = b['z0']
    ring = (ubox(u0 - BAT_WALL, u1 + BAT_WALL, v0 - BAT_WALL, v1 + BAT_WALL, z0, BAT_Z_TOP)
            - ubox(u0, u1, v0, v1, z0 - 1, BAT_Z_TOP + 1))
    ribs = [ubox(BAT_RIBS_U[0], BAT_RIBS_U[1], T - 0.1, v0 - BAT_WALL + 0.1, z0, BAT_Z_TOP),
            ubox(BAT_RIBS_U[0], BAT_RIBS_U[1], v1 + BAT_WALL - 0.1, H - T + 0.1, z0, BAT_Z_TOP)]
    return ring + union(ribs)


def module_posts():
    out = []
    for dx in (-1, 1):
        for dy in (-1, 1):
            u, v = CX + dx * MOD_HOLES[0] / 2, MOD_V + dy * MOD_HOLES[1] / 2
            out.append(cyl_z(X(u), v, FRONT_T - 0.1, FRONT_T + MOD_POST_H, MOD_POST_D))
    return union(out)


def module_pilots():
    out = []
    for dx in (-1, 1):
        for dy in (-1, 1):
            u, v = CX + dx * MOD_HOLES[0] / 2, MOD_V + dy * MOD_HOLES[1] / 2
            out.append(cyl_z(X(u), v, FRONT_T + MOD_POST_H - 6.0, FRONT_T + MOD_POST_H + 1, MOD_PILOT))
    return union(out)


def button_frames():
    out = []
    for b in BUTTONS.values():
        x = X(b['u'])
        out.append(box(x - BTN_FRAME / 2, x + BTN_FRAME / 2, BTN_STRIP_TOP, H - T + 0.1, FRONT_T - 0.1, BTN_Z + BTN_FRAME / 2))
    return union(out)


def button_cuts():
    cuts = []
    for b in BUTTONS.values():
        x = X(b['u'])
        h = BTN_POCKET / 2
        cuts.append(box(x - h, x + h, BTN_STRIP_TOP - 1, BTN_CEIL, BTN_Z - h, BTN_Z + h))
        cuts.append(cyl_y(x, BTN_CEIL - 0.1, H + 1, BTN_Z, BTN_HOLE))
        cuts.append(cyl_y(x, BTN_STRIP_TOP - 1, BTN_STRIP_TOP + 5, BTN_Z + 4.6, BTN_PILOT_D))
        if b['label'] == 'power':
            cuts.append(cyl_y(x, H - PWR_CBORE_DEPTH, H + 1, BTN_Z, PWR_CBORE_D))
        y0 = H - MARK_DEPTH
        if b['label'] == 'plus':
            cuts += [box(x - 2.0, x + 2.0, y0, H + 1, MARK_Z - 0.4, MARK_Z + 0.4),
                     box(x - 0.4, x + 0.4, y0, H + 1, MARK_Z - 2.0, MARK_Z + 2.0)]
        elif b['label'] == 'minus':
            cuts.append(box(x - 2.0, x + 2.0, y0, H + 1, MARK_Z - 0.4, MARK_Z + 0.4))
        else:
            ring = cyl_y(x, y0, H + 1, MARK_Z, 4.2) - cyl_y(x, y0 - 1, H + 2, MARK_Z, 3.4)
            ring = ring - box(x - 0.5, x + 0.5, y0 - 1, H + 2, MARK_Z, MARK_Z + 3)
            cuts += [ring, box(x - 0.4, x + 0.4, y0, H + 1, MARK_Z, MARK_Z + 2.4)]
    return union(cuts)


def build_body():
    out = outline()
    inner = out.offset(-T, JoinType.Round, 2.0, R_SEG)
    body = Manifold.extrude(out, BODY_D) - Manifold.extrude(inner, BODY_D - FRONT_T + 1).translate([0, 0, FRONT_T])

    for x, y in POSTS:                                           # lid screw posts in the four corners
        body = body + cyl_z(x, y, FRONT_T - 0.1, BODY_D, POST_D)
    body = body + module_posts() + battery_cradle() + button_frames()

    # speaker ring on the inside of the viewer-left wall (model X = W side), axis along X
    body = body + (cyl_x(W - T - SPK_RING_H, W - T + 0.4, SPK_V, SPK_Z, SPK_D + 3.2)
                   - cyl_x(W - T - SPK_RING_H - 1, W - T + 1, SPK_V, SPK_Z, SPK_D + 0.4))

    # ---- cuts --------------------------------------------------------------------------------------------
    body = body - box(X(CX) - WIN_W / 2, X(CX) + WIN_W / 2, MOD_V - WIN_H / 2, MOD_V + WIN_H / 2, -1, FRONT_T + 1)
    body = body - label_cut(TOP_LABEL) - label_cut(LOW_LABEL) - module_pilots()

    pitch = SPK_HOLE + 1.4                                       # hexagonal speaker grille through the left wall
    r_max = SPK_D / 2 - SPK_HOLE / 2 - 1.0
    holes, row, dz = [], 0, -r_max
    while dz <= r_max + 1e-6:
        dy = -r_max + ((pitch / 2) if row % 2 else 0.0)
        while dy <= r_max + 1e-6:
            if math.hypot(dy, dz) <= r_max:
                holes.append(cyl_x(W - T - 1, W + 1, SPK_V + dy, SPK_Z + dz, SPK_HOLE))
            dy += pitch
        dz += pitch * math.sqrt(3) / 2
        row += 1
    body = body - union(holes)

    for spec in (USB_MCU, USB_CHG):                              # two USB-C slots in the bottom wall
        ux = X(spec['u'])
        body = body - box(ux - USBC[0] / 2, ux + USBC[0] / 2, -1, T + 1, spec['z'] - USBC[1] / 2, spec['z'] + USBC[1] / 2)
    body = body - box(-1, T + 1, SD_V - SD_SLOT[0] / 2, SD_V + SD_SLOT[0] / 2,   # microSD slot, viewer-right wall (X = 0)
                      SD_Z - SD_SLOT[1] / 2, SD_Z + SD_SLOT[1] / 2)
    body = body - button_cuts()
    for x, y in POSTS:
        body = body - cyl_z(x, y, BODY_D - POST_HOLE_DEPTH, BODY_D + 1, POST_HOLE)
    return body


def build_lid():
    """Local Z: 0 = face against the body, LID_T = outer (back) face that touches the board."""
    lid = Manifold.extrude(outline(), LID_T)
    for x, y in POSTS:
        lid = lid - cyl_z(x, y, -1, LID_T + 1, LID_HOLE)
        lid = lid - cyl_z(x, y, LID_T - 1.7, LID_T + 0.01, LID_HOLE, LID_CSK_D)
    for u, v in MAG_POS:
        lid = lid - cyl_z(X(u), v, LID_T - MAG_H, LID_T + 1, MAG_D)
    kx = X(KEY_U)
    key = (cyl_z(kx, KEY_V, -1, LID_T + 1, KEY_HEAD_D)
           + cyl_z(kx, KEY_V + KEY_SLOT_LEN, -1, LID_T + 1, KEY_SLOT_W)
           + box(kx - KEY_SLOT_W / 2, kx + KEY_SLOT_W / 2, KEY_V, KEY_V + KEY_SLOT_LEN, -1, LID_T + 1))
    return lid - key


def build_inlays():
    """Both inlays side by side on the bed (front face down, same mirroring as the body), 0.6 mm thick."""
    top = Manifold.extrude(label_shape(TOP_LABEL), LABEL_DEPTH).translate([0, 0, 0])
    low = Manifold.extrude(label_shape(LOW_LABEL), LABEL_DEPTH).translate([0, -14, 0])
    return top + low


def build_inlays_placed():
    """The same two inlays at their assembled position (for the web viewer), flush in the pockets."""
    parts = [Manifold.extrude(label_shape(L), LABEL_DEPTH).translate([X(L['u']), L['v'], 0]) for L in (TOP_LABEL, LOW_LABEL)]
    return parts[0] + parts[1]


# ---- output -------------------------------------------------------------------------------------------------------------------------------
def write_stl(m, path):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3].astype(np.float32)
    f = np.asarray(mesh.tri_verts).astype(np.int64)
    tri = v[f]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    n = np.divide(n, ln, out=np.zeros_like(n), where=ln > 0).astype(np.float32)
    with open(path, 'wb') as fh:
        fh.write(b'Jira Desk Dashboard enclosure 1602'.ljust(80, b' '))
        fh.write(struct.pack('<I', len(f)))
        rec = np.zeros(len(f), dtype=[('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')])
        rec['n'], rec['v'] = n, tri
        fh.write(rec.tobytes())


def write_json(m, path):
    """Compact indexed mesh for the web viewer (STL stays the print format)."""
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, 'w') as fh:
        json.dump({'positions': [round(float(x), 3) for x in v.ravel()],
                   'index': [int(i) for i in f.ravel()]}, fh, separators=(',', ':'))


def report(name, m):
    bb = m.bounding_box()
    size = [round(bb[3 + i] - bb[i], 1) for i in range(3)]
    print(f"{name:7s} status={m.status().name:8s} tris={m.num_tri():6d} "
          f"volume={m.volume() / 1000:6.1f} cm3  size={size[0]}x{size[1]}x{size[2]} mm  genus={m.genus()}")


def main():
    out = pathlib.Path(__file__).resolve().parent / 'stl_lcd'
    out.mkdir(exist_ok=True)
    for name, part in (('body', build_body()), ('lid', build_lid()), ('inlays', build_inlays())):
        report(name, part)
        write_stl(part, out / f'{name}.stl')
        if name != 'inlays':
            write_json(part, out / f'{name}.json')
    write_json(build_inlays_placed(), out / 'inlays_placed.json')
    print(f"{W:g} x {H:g} x {BODY_D + LID_T:g} mm assembled; written to {out}")


if __name__ == '__main__':
    main()
