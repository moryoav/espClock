# Third-party notices

## Ant source artwork

The reduced ant walking silhouettes were derived from **“walking ant (with parts and rigged SPRITER file)”** by DudeMan on OpenGameArt. The source asset is released under **CC0**:

https://opengameart.org/content/walking-ant-with-parts-and-rigged-spriter-file

The two reduced 14×19 reference frames are included under `assets/`; the firmware contains equivalent bit masks in `include/ant_sprite.h`.

## Arduino_GFX

The project downloads **GFX Library for Arduino** version 1.6.0 as a PlatformIO dependency. It is not bundled in this archive and remains governed by its upstream license:

https://github.com/moononournation/Arduino_GFX

## Hardware references

Display and touch initialization were informed by public JC3248W535 / AXS15231B examples and the board's published data package. The hardware constants are isolated in `include/hardware_jc3248w535.h` so they can be audited or adjusted for a different board revision.
