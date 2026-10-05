#!/usr/bin/env python3
"""Parametric desk enclosure for the Jira Sprint Desk Dashboard (prototype).

    pip install manifold3d numpy
    python3 enclosure/make_enclosure.py          # writes enclosure/stl/*.stl

Coordinates (mm): X = width (left -> right), Y = depth (front 0 -> back D),
Z = height (0 = bottom). The top is a sloped face: low at the front (Hf), high at the back (Hb).

Parts
  shell.stl    hollow wedge, open at the bottom: display window, two label pockets,
               speaker grille (right), SD-card slot (left), USB-C slot (back), 4 screw posts
  base.stl     bottom plate, 4 countersunk M3 holes
  inlays.stl   two thin plates that fill the label pockets (print them in the brand colour)

EVERY component size below is a typical value from memory, NOT measured from your parts.
Measure the real modules with calipers and edit the numbers before printing.
"""
import math
import pathlib
import struct

import numpy as np
from manifold3d import CrossSection, Manifold

# ---- outer shape ------------------------------------------------------------------------
W = 104.0        # width
D = 70.0         # depth
HF = 16.0        # front height
HB = 62.0        # back height
T = 2.4          # wall thickness
R_SEG = 48       # circle smoothness

# ---- display (Nokia 5110 module, typical) ----------------------------------------------------
PCB = 45.6       # square pocket for the display PCB (45 mm module + 0.6 clearance)
WIN_W, WIN_H = 34.0, 24.0    # visible window (glass active area is ~30 x 20 mm)
DISPLAY_S = 44.0             # window centre, measured along the slope from the front edge
RIB_W, RIB_H = 1.6, 4.5      # rib that holds the PCB against the window

# ---- brand label pockets (sticker / inlay) --------------------------------------------------------
LABEL_DEPTH = 0.6
LABEL_W = 60.0
TOP_LABEL = (76.0, 9.0)      # (centre along slope, height)  -> "BANK RESPUBLIKA"
LOW_LABEL = (11.0, 12.0)     #                               -> "RETAIL LOAN TEAM"
INLAY_GAP = 0.2              # inlay is smaller than the pocket by this much per side

# ---- side / back openings ----------------------------------------------------------------------------
SPK_D = 28.0                 # speaker diameter
SPK_Y, SPK_Z = 42.0, 22.0    # speaker centre on the right wall
SPK_HOLE = 2.2
USBC = (10.0, 4.0)           # width, height of the USB-C slot (back wall)
USBC_X, USBC_Z = 30.0, 9.0
SD_SLOT = (15.0, 3.0)        # microSD slot on the left wall (y length, z height)
SD_Y, SD_Z = 30.0, 7.0

# ---- screw posts -------------------------------------------------------------------------------------------
POST_D, POST_H, POST_HOLE = 7.0, 12.0, 2.6
BASE_HOLE, BASE_CSK_D = 3.4, 6.4

THETA = math.degrees(math.atan2(HB - HF, D))   # slope angle from horizontal
SLOPE_LEN = math.hypot(D, HB - HF)


def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])


def cyl_x(x0, x1, y, z, d):
    """Cylinder along X from x0 to x1 centred on (y, z)."""
    c = Manifold.cylinder(x1 - x0, d / 2, d / 2, R_SEG)       # along +Z
    c = c.rotate([0, 90, 0])                                   # +Z -> +X
    return c.translate([x0, y, z])


def cyl_z(x, y, z0, z1, d):
    return Manifold.cylinder(z1 - z0, d / 2, d / 2, R_SEG).translate([x, y, z0])


def slope_frame(m):
    """Local (x, s, n) -> global: s runs up the slope from the front edge, n points out of the face."""
    return m.rotate([THETA, 0, 0]).translate([0, 0, HF])


def side_profile_extrude(poly_yz, x0, x1):
    """Extrude a (y, z) polygon along X from x0 to x1."""
    cs = CrossSection([np.array(poly_yz, dtype=float)])
    m = Manifold.extrude(cs, x1 - x0)          # profile in XY, extruded along Z
    perm = np.array([[0, 0, 1, x0], [1, 0, 0, 0], [0, 1, 0, 0]], dtype=float)   # (a,b,c) -> (x=c, y=a, z=b)
    return m.transform(perm)


def outer_wedge():
    return side_profile_extrude([(0, 0), (D, 0), (D, HB), (0, HF)], 0, W)


def inner_wedge():
    drop = T / math.cos(math.radians(THETA))   # slope face moved inward by T
    z_at = lambda y: HF + (HB - HF) * y / D - drop
    return side_profile_extrude(
        [(T, -1), (D - T, -1), (D - T, z_at(D - T)), (T, z_at(T))], T, W - T)


def build_shell():
    shell = outer_wedge() - inner_wedge()

    # screw posts in the four inner corners
    px = [T + 4.5, W - T - 4.5]
    py = [T + 4.5, D - T - 4.5]
    for x in px:
        for y in py:
            shell = shell + cyl_z(x, y, 0, POST_H, POST_D)

    # ribs that hold the display PCB against the inside of the slope
    ox = W / 2
    half = PCB / 2
    rib_n0, rib_n1 = -T - RIB_H, -T + 0.4          # from inside the cavity into the wall
    s0, s1 = DISPLAY_S - half, DISPLAY_S + half
    ring = (box(ox - half - RIB_W, ox + half + RIB_W, s0 - RIB_W, s1 + RIB_W, rib_n0, rib_n1)
            - box(ox - half, ox + half, s0, s1, rib_n0 - 1, rib_n1 + 1))
    shell = shell + slope_frame(ring)

    # speaker ring on the inside of the right wall (axis along X)
    ring_spk = cyl_x(W - T - 5, W - T + 0.4, SPK_Y, SPK_Z, SPK_D + 3.6) - cyl_x(W - T - 6, W, SPK_Y, SPK_Z, SPK_D + 0.4)
    shell = shell + ring_spk

    # ---- cuts -----------------------------------------------------------------------------------
    # display window
    shell = shell - slope_frame(box(ox - WIN_W / 2, ox + WIN_W / 2,
                                    DISPLAY_S - WIN_H / 2, DISPLAY_S + WIN_H / 2, -T - 1, 1))
    # brand label pockets (shallow, from the outside)
    for centre, height in (TOP_LABEL, LOW_LABEL):
        shell = shell - slope_frame(box(ox - LABEL_W / 2, ox + LABEL_W / 2,
                                        centre - height / 2, centre + height / 2, -LABEL_DEPTH, 1))
    # speaker grille: hexagonal pattern of round holes inside a circle
    pitch = SPK_HOLE + 1.6
    r_max = SPK_D / 2 - SPK_HOLE / 2 - 0.6
    holes = []
    row = 0
    dz = -r_max
    while dz <= r_max + 1e-6:
        offset = (pitch / 2) if row % 2 else 0.0
        dy = -r_max + offset
        while dy <= r_max + 1e-6:
            if math.hypot(dy, dz) <= r_max:
                holes.append(cyl_x(W - T - 1, W + 1, SPK_Y + dy, SPK_Z + dz, SPK_HOLE))
            dy += pitch
        dz += pitch * math.sqrt(3) / 2
        row += 1
    shell = shell - _union(holes)
    # USB-C slot (back wall)
    shell = shell - box(USBC_X - USBC[0] / 2, USBC_X + USBC[0] / 2, D - T - 1, D + 1,
                        USBC_Z - USBC[1] / 2, USBC_Z + USBC[1] / 2)
    # microSD slot (left wall)
    shell = shell - box(-1, T + 1, SD_Y - SD_SLOT[0] / 2, SD_Y + SD_SLOT[0] / 2,
                        SD_Z - SD_SLOT[1] / 2, SD_Z + SD_SLOT[1] / 2)
    # screw holes in the posts
    for x in px:
        for y in py:
            shell = shell - cyl_z(x, y, 2, POST_H + 1, POST_HOLE)
    return shell


def _union(parts):
    acc = parts[0]
    for p in parts[1:]:
        acc = acc + p
    return acc


def build_base():
    base = box(0, W, 0, D, -T, 0)
    for x in (T + 4.5, W - T - 4.5):
        for y in (T + 4.5, D - T - 4.5):
            base = base - cyl_z(x, y, -T - 1, 1, BASE_HOLE)
            # countersink: cone widening toward the bottom face
            cone = Manifold.cylinder(1.7, BASE_CSK_D / 2, BASE_HOLE / 2, R_SEG).translate([x, y, -T - 0.01])
            base = base - cone
    return base


def build_inlays():
    ox = W / 2
    parts = []
    for centre, height in (TOP_LABEL, LOW_LABEL):
        p = box(ox - LABEL_W / 2 + INLAY_GAP, ox + LABEL_W / 2 - INLAY_GAP,
                centre - height / 2 + INLAY_GAP, centre + height / 2 - INLAY_GAP, 0, LABEL_DEPTH)
        parts.append(p)
    # laid out flat side by side, ready to print
    a = parts[0].translate([0, 0, 0])
    b = parts[1].translate([0, 20, 0])
    return a + b


def write_stl(m, path):
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3].astype(np.float32)
    f = np.asarray(mesh.tri_verts).astype(np.int64)
    tri = v[f]                                           # (n, 3, 3)
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    n = np.divide(n, ln, out=np.zeros_like(n), where=ln > 0).astype(np.float32)
    with open(path, 'wb') as fh:
        fh.write(b'Jira Desk Dashboard enclosure'.ljust(80, b' '))
        fh.write(struct.pack('<I', len(f)))
        rec = np.zeros(len(f), dtype=[('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')])
        rec['n'], rec['v'] = n, tri
        fh.write(rec.tobytes())
    return len(f)


def write_json(m, path):
    """Compact indexed mesh for the web viewer (STL stays the print format)."""
    import json
    mesh = m.to_mesh()
    v = np.asarray(mesh.vert_properties)[:, :3]
    f = np.asarray(mesh.tri_verts)
    with open(path, 'w') as fh:
        json.dump({'positions': [round(float(x), 3) for x in v.ravel()],
                   'index': [int(i) for i in f.ravel()]}, fh, separators=(',', ':'))


def report(name, m):
    bb = m.bounding_box()
    size = [round(bb[3 + i] - bb[i], 1) for i in range(3)]
    print(f"{name:11s} status={m.status().name:9s} tris={m.num_tri():6d} "
          f"volume={m.volume() / 1000:7.1f} cm3  size={size[0]}x{size[1]}x{size[2]} mm  genus={m.genus()}")


def main():
    out = pathlib.Path(__file__).resolve().parent / 'stl'
    out.mkdir(exist_ok=True)
    print(f"slope angle {THETA:.1f} deg, slope length {SLOPE_LEN:.1f} mm")
    for name, part in (('shell', build_shell()), ('base', build_base()), ('inlays', build_inlays())):
        report(name, part)
        write_stl(part, out / f'{name}.stl')
        if name != 'inlays':
            write_json(part, out / f'{name}.json')
    print('written to', out)


if __name__ == '__main__':
    main()
