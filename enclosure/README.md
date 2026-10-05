# Enclosure prototype

- `make_enclosure.py` - parametric generator (`pip install manifold3d numpy`), writes `stl/`.
- `stl/shell.stl`, `stl/base.stl`, `stl/inlays.stl` - printable parts (all three are watertight manifolds).
- `viewer.html` - interactive 3D preview. Serve this folder (`python3 -m http.server`) and open it;
  it loads `stl/shell.json` and `stl/base.json` (written by the generator next to the STLs).

Component sizes are typical values from memory, not measured. Measure your modules and edit the
constants at the top of `make_enclosure.py`, then rerun it. Keep the numbers in `viewer.html`
(the block under "Same numbers as ...") in sync if you change the slope or label positions.

Branding: the two label pockets are 0.6 mm deep. Use stickers, or print `inlays.stl` in the brand colour.
Official logos and colours must come from the brand owners; the viewer only accepts an uploaded logo file.
