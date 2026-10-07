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
The two label pockets are 0.6 mm deep: use stickers, or print `inlays.stl` in the brand colour.
Official logos and colours must come from the brand owners; the viewer only accepts an uploaded logo file.
