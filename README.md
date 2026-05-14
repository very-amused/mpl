# MPL (Music Player Light)
A WIP music player with a focus on build-time modularity, stability, and performance.

## Compiling

### Configure
```sh
meson setup build
cd build
meson configure -Dbuildtype=release # debug is the default build type

### Customization:
meson configure -Dnerdfont_icons=false # Enable Nerdfont icons (default: true)

### DEV OPTIONS:
# Build with emulated audio output (no sound):
meson configure -Dao_fast=true # useful for compatibility checking on platforms before implementing an AudioBackend
```

All compile-time options can be seen in meson.options. Use these to customize MPL to your liking.

### Build
```sh
ninja
```

### Install
```sh
sudo ninja install
```

### Uninstall
```sh
sudo ninja uninstall
```
