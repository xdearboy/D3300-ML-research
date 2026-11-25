# D3300 physical input evidence — boundary note

Date checked: 2026-11-25. This note supplements the static C 1.02 analysis;
it is **not** firmware evidence and does not change any ROM-derived mapping.

## Observed in public photographs

1. The rear-cover photograph in the eBay listing below visibly shows distinct
   physical controls labelled `MENU` (left column) and `OK` (centre of the
   multi-selector). This is consistent with Nikon's official D3300 product
   download page, which provides the camera's operating manuals.
2. A separate listing photographs a flexible printed circuit (FPC), visibly
   marked `CINDY089809`, with multiple exposed traces, contact pads and
   connector fingers. The photograph alone establishes neither a net list nor
   a pin numbering convention.

Sources (retrieved 2026-11-25):

* Nikon's [D3300 download centre](https://downloadcenter.nikonimglib.com/en/products/21/D3300.html)
  (official operating manuals and firmware distribution).
* [Rear-cover photograph](https://www.ebay.com/itm/295974086230) and its
  [full-resolution image](https://i.ebayimg.com/images/g/CwAAAOSwzNNkNhEl/s-l1200.jpg).
* [Rear-button FPC listing](https://www.ebay.com/itm/405376585422) and its
  [full-resolution image](https://i.ebayimg.com/images/g/vksAAOSwjn9ZjXB8/s-l1600.jpg).

The marketplace photographs are secondary visual evidence, not Nikon circuit
documentation. They are deliberately linked rather than copied into this
repository.

## Explicit non-conclusions

The photographs do **not** establish any of the following:

* an FPC connector pin to `MENU` or `OK` trace assignment;
* a switch-matrix row/column topology or electrical polarity;
* correspondence between either physical control and raw queue values `0x0B`
  or `0x0C`;
* correspondence between either physical control and GPIO pin 8 / the
  type-2 path at `0x9011DDA8`;
* a safe observation or injection point in the camera.

Consequently, the ROM result remains the authoritative boundary: `0x0B ->
0x1B` and `0x0C -> 0x01` are verified **logical** translations only. Mapping
those logical IDs to the physical button legends requires one of: a legitimate
schematic/net list, an unambiguous high-resolution two-sided traced FPC with
connector pin numbering, or authorised live observation. None is introduced
by this note.

## Next safe evidence to seek

Prefer a Nikon-published service document or a clearly identified replacement
part drawing that labels both connector pins and switch nets. If no such
document is public, keep the physical mapping unassigned; do not extrapolate
it from button placement or trace proximity.
