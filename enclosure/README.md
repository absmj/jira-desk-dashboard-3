# Enclosure prototype (78 x 78 mm, battery powered, wall mounted)

- `make_enclosure.py` - parametric generator (`pip install manifold3d numpy`), writes `stl/`.
- `stl/body.stl`, `stl/lid.stl`, `stl/inlays.stl` - printable parts (all watertight manifolds).
  Print the body front face down and the lid inner face down (the model is already in that orientation).
- `viewer.html` - interactive 3D preview. Serve this folder (`python3 -m http.server`) and open it;
  it loads `stl/body.json` and `stl/lid.json` (written by the generator next to the STLs).

## How it is built
Square 78 x 78 x 32.5 mm. Behind the display the parts are stacked in two layers:
display PCB | DFPlayer Mini, DS3231, ESP32-C3 SuperMini, TP4056 charger | flat LiPo battery (about 50 x 34 x 10).
The generator's placeholder boxes were checked against the shell: no overlaps with the typical sizes below.

## Battery and buttons
- **Battery cradle:** a 1.2 mm ring (0.6 mm clearance) around a flat LiPo (about 50 x 34 x 10 mm, e.g. 103450), joined to the
  bottom and side walls by three ribs. It keeps the cell in place sideways; the lid (with a foam pad) holds it from behind.
  Leave a notch or gap for the battery lead and put the TP4056 / switch wiring through the gap between the ribs.
- **Buttons (top edge, pressed from above while the device hangs):** POWER (u = 18 mm), VOL- (46), VOL+ (60). They are
  6 x 6 mm tact switches with a **long plunger and no printed caps**: POWER `6x6x9` (tip 1 mm below the surface, in a counterbore, hard to hit
  by accident), VOL `6x6x11` (tip 1 mm proud). The switches are soldered to a small perfboard strip that is screwed
  (M2 self-tapping, pilot holes in the frames) to the underside of three frames hanging from the top wall. The marks
  `+`, `-` and the power symbol are engraved 0.4 mm into the top face behind each button.
  Switch heights are the part to verify: the plunger tips must reach the stated levels with **your** switches
  (edit `BUTTONS`, `BTN_*` in `make_enclosure.py`).
- **POWER caveat:** a tact switch only gives a pulse. It can wake the board and start a sync, or be read as "sleep",
  but it cannot cut the battery by itself. A true off needs a latching switch or a slide switch in the battery line
  (not modelled). See `docs/battery-plan.md`.
- `check_fit.py` runs an interference check of the placeholder components against the printed parts and each other
  (typical sizes, not measured). It exits non-zero on a collision. It was also checked to detect a deliberately wrong battery position.

## Hanging
- 4 pockets for 10 x 3 mm disc magnets in the lid (magnetic whiteboard / Kanban board).
- A keyhole slot for a screw or pin (cork board, wall). Smooth whiteboards can let a magnet-only device slide: add a thin rubber sticker or more magnets.

## Not verified (typical values from memory, measure your parts)
Display module, window, DFPlayer, DS3231, SuperMini, TP4056, battery, the 20 mm speaker, magnets.
Edit the constants at the top of `make_enclosure.py`, rerun it, and keep the numbers in `viewer.html`
(the block under "Same numbers as ...") in sync.

## Branding
The front plate has two 0.6 mm recesses that follow real outlines: the Bank Respublika logo (above the window, 39.4 mm wide, from
`brand/BR_digital_logo.svg`, supplied by the project owner) and "Retail Loan Team" (below it, 48 mm wide). `inlays.stl` holds
matching solids (0.2 mm clearance per side); print them in brand blue `#3327FF` (the fill colour in the SVG) and glue them in, or use stickers.
`inlays_placed.json` is the same geometry at its assembled position, used by `viewer_lcd.html`.

* The team name is set in **Outfit SemiBold** (SIL OFL, `brand/`), picked by eye as the closest free font to the logo lettering. It is **not** the bank's official brand font; swap `brand/Outfit_600SemiBold.ttf` for the real one if you have it.
* Thin strokes of the logo are about 1 mm: use a 0.2 mm nozzle layer or paint-fill if the 0.4 mm nozzle blurs them.
* Using the bank's logo on a device shown outside the team is a brand-approval question for the bank, not a technical one.
* `brand.py` converts SVG paths and font outlines to polygons (needs `fonttools svgpathtools manifold3d`).

## 16x2 LCD variant (branch `feature/lcd1602-display`)
`make_enclosure_lcd.py` (98 x 56 mm), `check_fit_lcd.py`, `viewer_lcd.html`, output in `stl_lcd/`. See `docs/lcd1602.md`.
The square files above are the Nokia 5110 version and are unchanged.
