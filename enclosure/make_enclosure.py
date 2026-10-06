#!/usr/bin/env python3
"""Minimal square wall-mount enclosure for the Jira Sprint Desk Dashboard (prototype, battery powered).

    pip install manifold3d numpy
    python3 enclosure/make_enclosure.py          # writes enclosure/stl/*.stl and *.json

The device hangs on a Kanban board: flat square front, a back lid with magnet pockets
(magnetic whiteboard) and a keyhole slot (screw / pin on a cork or wall board).

Coordinates (mm), in PRINT orientation:
  Z = 0 is the FRONT face, lying on the print bed; Z grows toward the back.
  Y = up (the board's vertical), X = width. Looking at the front face, X grows to the
  LEFT, so a position measured from the viewer's left edge is written X(u).
  Gravity is -Y when the device hangs.

Parts
  body.stl    front plate + walls: display window, 2 label pockets, speaker grille (viewer-right wall),
              two USB-C slots (bottom wall: programming + charging), microSD slot (viewer-left wall),
              4 screw posts
  lid.stl     back plate, printed inner face down: countersunk screws, 4 magnet pockets, keyhole
  inlays.stl  two thin plates that fill the label pockets (print in the brand colour)

Inside, behind the display, parts are stacked in two layers (front to back):
  display PCB + header | layer 1: DS3231, DFPlayer Mini | layer 2: LiPo battery, ESP32-C3 SuperMini, TP4056
EVERY component size below is a typical value from memory, NOT measured from your parts.
Measure the real modules with calipers and edit the numbers before printing.
"""
import json
import math
import pathlib
import struct

import numpy as np
from manifold3d import CrossSection, JoinType, Manifold

# ---- outer shape -------------------------------------------------------------------------
S = 78.0           # square side
R = 5.0            # outer corner radius
T = 2.0            # wall thickness
FRONT_T = 2.0      # front plate thickness
BODY_D = 28.0      # body depth (front face to rear rim)
LID_T = 4.5        # lid thickness (3 mm magnets + a 1.3 mm skin)
R_SEG = 48


def X(u):
    """u = distance from the viewer's left edge when looking at the front -> model X."""
    return S - u


# ---- display (Nokia 5110 module, typical) -----------------------------------------------------
PCB = 45.6                    # square pocket for the 45 mm display PCB (+0.6 clearance)
WIN_W, WIN_H = 34.0, 24.0     # visible window (active glass area is about 30 x 20 mm)
DISP_V = 39.0                 # window centre height
RIB_W, RIB_H = 1.6, 4.5       # ribs that hold the PCB against the front plate

# ---- brand label pockets (sticker / inlay) -------------------------------------------------------
LABEL_DEPTH = 0.6
INLAY_GAP = 0.2
TOP_LABEL = dict(u=39.0, v=70.5, w=58.0, h=6.0)    # "BANK RESPUBLIKA"
LOW_LABEL = dict(u=39.0, v=7.5, w=58.0, h=6.0)     # "RETAIL LOAN TEAM"

# ---- speaker (small round, mounted on the viewer-right wall, faces sideways) -------------------------
SPK_D = 20.0
SPK_V, SPK_Z = 45.0, 14.0
SPK_HOLE = 1.8
SPK_RING_H = 5.0

# ---- openings ---------------------------------------------------------------------------------------------------------
USBC = (10.0, 4.0)            # USB-C slot size (width, height)
USB_MCU = dict(u=21.0, z=13.0)    # ESP32-C3 SuperMini (programming)
USB_CHG = dict(u=48.5, z=13.0)    # TP4056 charger module (battery charging)
SD_SLOT = (15.0, 3.0)         # microSD slot in the viewer-left wall (y length, z height)
SD_V, SD_Z = 53.0, 12.5

# ---- screws, magnets, keyhole ---------------------------------------------------------------------------------------
POST_D, POST_HOLE, POST_HOLE_DEPTH = 6.4, 2.6, 12.0
LID_HOLE, LID_CSK_D = 3.4, 6.4
MAG_D, MAG_H = 10.2, 3.2      # pocket for a 10 x 3 mm disc magnet (check yours!)
MAG_POS = [(20.0, 20.0), (58.0, 20.0), (20.0, 52.0), (58.0, 52.0)]   # (u, v)
KEY_HEAD_D, KEY_SLOT_W = 9.0, 4.5
KEY_U, KEY_V, KEY_SLOT_LEN = 39.0, 64.0, 4.0

POST_OFF = T + 3.8
POSTS = [(X(POST_OFF), POST_OFF), (X(S - POST_OFF), POST_OFF),
         (X(POST_OFF), S - POST_OFF), (X(S - POST_OFF), S - POST_OFF)]


# ---- helpers ------------------------------------------------------------------------------------------------------------------
def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl_z(x, y, z0, z1, d, d2=None):
    return Manifold.cylinder(z1 - z0, d / 2, (d2 if d2 is not None else d) / 2, R_SEG).translate([x, y, z0])


def cyl_x(x0, x1, y, z, d):
    """Cylinder along X from x0 to x1, centred on (y, z)."""
    return Manifold.cylinder(x1 - x0, d / 2, d / 2, R_SEG).rotate([0, 90, 0]).translate([x0, y, z])


def union(parts):
    acc = parts[0]
    for p in parts[1:]:
        acc = acc + p
    return acc


def outline():
    return CrossSection.square([S, S]).offset(-R, JoinType.Miter, 2.0, 0).offset(R, JoinType.Round, 2.0, R_SEG)


def label_cut(L):
    x = X(L['u'])
    return box(x - L['w'] / 2, x + L['w'] / 2, L['v'] - L['h'] / 2, L['v'] + L['h'] / 2, -1, LABEL_DEPTH)


# ---- parts --------------------------------------------------------------------------------------------------------------------------
def build_body():
    out = outline()
    inner = out.offset(-T, JoinType.Round, 2.0, R_SEG)
    body = Manifold.extrude(out, BODY_D) - Manifold.extrude(inner, BODY_D - FRONT_T + 1).translate([0, 0, FRONT_T])

    for x, y in POSTS:                                           # screw posts in the four corners
        body = body + cyl_z(x, y, FRONT_T - 0.1, BODY_D, POST_D)

    cx, half = S / 2, PCB / 2                                    # ribs holding the display PCB
    z0, z1 = FRONT_T - 0.4, FRONT_T + RIB_H
    ring = (box(cx - half - RIB_W, cx + half + RIB_W, DISP_V - half - RIB_W, DISP_V + half + RIB_W, z0, z1)
            - box(cx - half, cx + half, DISP_V - half, DISP_V + half, z0 - 1, z1 + 1))
    body = body + ring

    # speaker ring on the inside of the viewer-right wall (model X = 0 side), axis along X
    body = body + (cyl_x(T - 0.4, T + SPK_RING_H, SPK_V, SPK_Z, SPK_D + 3.2)
                   - cyl_x(T - 1, T + SPK_RING_H + 1, SPK_V, SPK_Z, SPK_D + 0.4))

    # ---- cuts ------------------------------------------------------------------------------------------
    body = body - box(cx - WIN_W / 2, cx + WIN_W / 2, DISP_V - WIN_H / 2, DISP_V + WIN_H / 2, -1, FRONT_T + 1)
    body = body - label_cut(TOP_LABEL) - label_cut(LOW_LABEL)

    # speaker grille: hexagonal pattern of small round holes through the side wall
    pitch = SPK_HOLE + 1.4
    r_max = SPK_D / 2 - SPK_HOLE / 2 - 1.0
    holes, row, dz = [], 0, -r_max
    while dz <= r_max + 1e-6:
        dy = -r_max + ((pitch / 2) if row % 2 else 0.0)
        while dy <= r_max + 1e-6:
            if math.hypot(dy, dz) <= r_max:
                holes.append(cyl_x(-1, T + 1, SPK_V + dy, SPK_Z + dz, SPK_HOLE))
            dy += pitch
        dz += pitch * math.sqrt(3) / 2
        row += 1
    body = body - union(holes)

    # two USB-C slots in the bottom wall
    for spec in (USB_MCU, USB_CHG):
        ux = X(spec['u'])
        body = body - box(ux - USBC[0] / 2, ux + USBC[0] / 2, -1, T + 1,
                          spec['z'] - USBC[1] / 2, spec['z'] + USBC[1] / 2)
    # microSD slot (viewer-left wall = high X)
    body = body - box(S - T - 1, S + 1, SD_V - SD_SLOT[0] / 2, SD_V + SD_SLOT[0] / 2,
                      SD_Z - SD_SLOT[1] / 2, SD_Z + SD_SLOT[1] / 2)

    for x, y in POSTS:                                           # screw holes in the posts
        body = body - cyl_z(x, y, BODY_D - POST_HOLE_DEPTH, BODY_D + 1, POST_HOLE)
    return body


def build_lid():
    """Local Z: 0 = face against the body, LID_T = outer (back) face that touches the board."""
    lid = Manifold.extrude(outline(), LID_T)
    for x, y in POSTS:
        lid = lid - cyl_z(x, y, -1, LID_T + 1, LID_HOLE)
        lid = lid - cyl_z(x, y, LID_T - 1.7, LID_T + 0.01, LID_HOLE, LID_CSK_D)      # countersink
    for u, v in MAG_POS:
        lid = lid - cyl_z(X(u), v, LID_T - MAG_H, LID_T + 1, MAG_D)
    # keyhole: the wide hole lets the screw head into the case, the slot above it holds the shank
    kx = X(KEY_U)
    key = (cyl_z(kx, KEY_V, -1, LID_T + 1, KEY_HEAD_D)
           + cyl_z(kx, KEY_V + KEY_SLOT_LEN, -1, LID_T + 1, KEY_SLOT_W)
           + box(kx - KEY_SLOT_W / 2, kx + KEY_SLOT_W / 2, KEY_V, KEY_V + KEY_SLOT_LEN, -1, LID_T + 1))
    return lid - key


def build_inlays():
    parts = []
    for i, L in enumerate((TOP_LABEL, LOW_LABEL)):
        w, h = L['w'] - 2 * INLAY_GAP, L['h'] - 2 * INLAY_GAP
        parts.append(box(0, w, i * 12.0, i * 12.0 + h, 0, LABEL_DEPTH))
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
        fh.write(b'Jira Desk Dashboard enclosure'.ljust(80, b' '))
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
    out = pathlib.Path(__file__).resolve().parent / 'stl'
    out.mkdir(exist_ok=True)
    for name, part in (('body', build_body()), ('lid', build_lid()), ('inlays', build_inlays())):
        report(name, part)
        write_stl(part, out / f'{name}.stl')
        if name != 'inlays':
            write_json(part, out / f'{name}.json')
    print(f"{S:g} x {S:g} x {BODY_D + LID_T:g} mm assembled; written to {out}")


if __name__ == '__main__':
    main()
