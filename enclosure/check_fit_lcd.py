#!/usr/bin/env python3
"""Interference check for the 16x2 case: placeholder component boxes (typical sizes, NOT measured) vs the printed
parts and each other.   python3 enclosure/check_fit_lcd.py      (exit code 1 on any collision)"""
import sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import make_enclosure_lcd as M
from make_enclosure_lcd import ubox, cyl_y, cyl_x, X, W, H, CX

C = {}
pz0, pz1 = M.MOD_PCB_Z
C['lcd pcb'] = ubox(CX - 40, CX + 40, M.MOD_V - 18, M.MOD_V + 18, pz0, pz1)
C['lcd bezel'] = ubox(CX - 35.5, CX + 35.5, M.MOD_V - 12.1, M.MOD_V + 12.1, M.FRONT_T, pz0)
C['i2c backpack'] = ubox(CX - 37, CX + 5, M.MOD_V + 3.3, M.MOD_V + 18.3, pz1, pz1 + 8.5)     # lies over the upper PCB back
C['esp32-c3'] = ubox(10, 28, 2.2, 24.7, 21, 22.2)
C['esp usb-c'] = ubox(14.5, 23.5, 2.2, 9.7, 22.2, 25.2)
C['tp4056'] = ubox(34, 51, 2.2, 30.2, 21, 22.6)
C['tp4056 usb-c'] = ubox(38.5, 47.5, 2.2, 9.7, 22.6, 25.8)
C['ds3231'] = ubox(52.5, 90.0, 3.0, 25.0, 21, 22.6)
C['cr2032 holder'] = ubox(60, 80, 6.0, 18.0, 22.6, 27.2)
b = M.BAT
C['battery'] = ubox(b['u0'], b['u1'], b['v0'], b['v1'], b['z0'], b['z1'])
C['dfplayer'] = ubox(65, 86, 22.5, 43.5, 28.0, 29.6)
C['dfplayer sd socket'] = ubox(86, 95.5, 28.0, 38.0, 29.6, 32.0)
C['speaker'] = cyl_x(W - M.T - 4, W - M.T, M.SPK_V, M.SPK_Z, 20)
C['keypad strip'] = ubox(14, 88, M.BTN_STRIP_TOP - 1.6, M.BTN_STRIP_TOP, M.BTN_Z - 8, M.BTN_Z + 8)
for k, bt in M.BUTTONS.items():
    x = X(bt['u'])
    C['switch ' + k] = M.box(x - 3, x + 3, M.BTN_STRIP_TOP, M.BTN_STRIP_TOP + M.BTN_BODY_H, M.BTN_Z - 3, M.BTN_Z + 3)
    C['plunger ' + k] = cyl_y(x, M.BTN_STRIP_TOP + M.BTN_BODY_H, M.BTN_STRIP_TOP + bt['H'], M.BTN_Z, 3.5)

body, lid = M.build_body(), M.build_lid().translate([0, 0, M.BODY_D])
bad = 0
print('component vs printed parts')
for n, c in C.items():
    for pn, part in (('body', body), ('lid', lid)):
        v = (c ^ part).volume()
        if v > 0.05:
            bad += 1
            print(f'  COLLISION {n:20s} x {pn}: {v:7.2f} mm3')
print('component vs component')
names = list(C)
for i, a in enumerate(names):
    for bn in names[i + 1:]:
        v = (C[a] ^ C[bn]).volume()
        if v > 0.05 and not (('plunger' in a and 'switch' in bn) or ('switch' in a and 'plunger' in bn)):
            bad += 1
            print(f'  OVERLAP   {a:20s} x {bn:20s}: {v:7.2f} mm3')
for k, bt in M.BUTTONS.items():
    print(f'  {k}: plunger tip {M.BTN_STRIP_TOP + bt["H"] - H:+.1f} mm vs the top surface')
print('OK, no collisions' if not bad else f'{bad} problem(s)')
sys.exit(1 if bad else 0)
