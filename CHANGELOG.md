# Changelog

## 1.0.1

- Fix an out-of-bounds settings write that crashed when saving depth or other controls.
- Derive the save loop bound from the settings array and check key/value counts at compile time.
- Add save/reload regression coverage across all 29 depth positions.

## 1.0.0

- Initial Windows x64 release for Ansnake P5, 45 LEDs.
- Full-resolution, edge-centered depth Gaussian; uniform weighting along each edge.
- Corner-centered radial Gaussian within a quarter disk.
- One production algorithm, no downsample texture or sparse sample grid.
- Front-view four-corner layout calibration and reversible visual color wizard.
- HDR/SDR handling, 60 Hz target, latest-frame processing, tray controls.
- Explicit current-user sign-in startup registration and removal.

