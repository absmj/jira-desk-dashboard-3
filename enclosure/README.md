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
