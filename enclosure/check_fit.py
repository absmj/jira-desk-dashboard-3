#!/usr/bin/env python3
"""Interference check: placeholder component boxes (typical sizes, NOT measured) against the printed body and each other.
    python3 enclosure/check_fit.py
Prints every overlap larger than 0.05 mm3. 'OK' only means: no collision between the boxes used here.
"""
import math
import sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import make_enclosure as M
from make_enclosure import ubox, cyl_y, X

C = {}
C['display pcb'] = ubox(39 - 22.5, 39 + 22.5, M.DISP_V - 22.5, M.DISP_V + 22.5, M.FRONT_T, M.FRONT_T + 1.6)
C['dfplayer'] = ubox(8, 29, 42, 63, 10, 11.6)
C['dfplayer sd socket'] = ubox(4, 14, 48, 58, 11.6, 13.6)
C['ds3231'] = ubox(31, 69, 42, 64, 9.5, 11.1)
C['cr2032 holder'] = ubox(38, 58, 47, 59, 11.1, 16.5)
C['esp32-c3'] = ubox(12, 30, 2.2, 24.7, 11, 12.2)
C['esp usb-c'] = ubox(16.5, 25.5, 2.2, 9.7, 12.2, 15.2)
C['tp4056'] = ubox(40, 57, 2.2, 30, 11, 12.6)
C['tp4056 usb-c'] = ubox(44, 53, 2.2, 9.7, 12.6, 15.8)
b = M.BAT
C['battery'] = ubox(b['u0'], b['u1'], b['v0'], b['v1'], b['z0'], b['z1'])
C['speaker'] = M.cyl_x(M.T, M.T + 4, M.SPK_V, M.SPK_Z, 20)
# keypad: strip + three switches with plungers (plunger 3.5 mm)
C['keypad strip'] = ubox(12, 66, M.BTN_STRIP_TOP - 1.6, M.BTN_STRIP_TOP, M.BTN_Z - 8, M.BTN_Z + 8)
for k, bt in M.BUTTONS.items():
    x = X(bt['u'])
    C['switch ' + k] = M.box(x - 3, x + 3, M.BTN_STRIP_TOP, M.BTN_STRIP_TOP + M.BTN_BODY_H, M.BTN_Z - 3, M.BTN_Z + 3)
    C['plunger ' + k] = cyl_y(x, M.BTN_STRIP_TOP + M.BTN_BODY_H, M.BTN_STRIP_TOP + bt['H'], M.BTN_Z, 3.5)

body, lid = M.build_body(), M.build_lid().translate([0, 0, M.BODY_D])
bad = 0
def vol(m): return m.volume()
print('component vs printed parts')
for n, c in C.items():
    for pn, part in (('body', body), ('lid', lid)):
        v = vol(c ^ part)
        if v > 0.05:
            bad += 1
            print(f'  COLLISION {n:20s} x {pn}: {v:7.2f} mm3')
print('component vs component')
names = list(C)
for i, a in enumerate(names):
    for bn in names[i + 1:]:
        v = vol(C[a] ^ C[bn])
        touching = ('plunger' in a and 'switch' in bn) or ('switch' in a and 'plunger' in bn)
        if v > 0.05 and not touching:
            bad += 1
            print(f'  OVERLAP   {a:20s} x {bn:20s}: {v:7.2f} mm3')
# plunger tips relative to the top surface
for k, bt in M.BUTTONS.items():
    tip = M.BTN_STRIP_TOP + bt['H']
    print(f'  {k}: plunger tip y={tip:.1f}, top surface y={M.S:g} -> {tip - M.S:+.1f} mm')
print('OK, no collisions' if not bad else f'{bad} problem(s)')
sys.exit(1 if bad else 0)
