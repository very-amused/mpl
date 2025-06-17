# MPL (Music Player Light)
A WIP music player with a focus on build-time modularity, stability, and performance.

## Compiling

### Configure
```sh
meson setup build
cd build
meson configure -Dbuildtype=release # debug is the default build type
# If you're using a keyboard layout covered by unsigned (extended) ASCII:
meson configure -Dascii_keybinds=true
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
