# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.5.0]
### Added
- MPL now has a built-in shell which supports all config functions! `shell_open()` is bound to `:` by default.

### Internal
- Reworked all of the terminal I/O handling to support the new shell.

## [0.4.10]
### Added
- `play()` and `pause()` config functions

### Fixed
- Segfault when providing an invalid track path
- Logic error in `TrackQueue_play` that could cause us to get stuck in a pause state

### Internal
- Rewrote the config parsing to use two-stage lexing + parsing and build a parse tree. This is to support the more powerful syntax needed for a shell
- NOTE: This *major* parsing rewrite completely replaced the (comparably) *minor* parsing rewrite done in v0.4.9

## [0.4.9]
### Internal
- Rewrote how config functions (previously called keybind functions) are defined and parsed. See [src/config/functions/README.md]

## [0.4.8]
### Added
- show_metadata() keybind function
- default keybind m = show_metadata()

### Fixed
- Seeks now immediately update the timecode displayed, including when playback is paused

### Internal
- Wrote a UserInterface API similar to the AudioBackend API
- Each UserInterface provides ini, deinit, and a mainloop in which it handles events
- MPL now has the groundwork for additional UI's in the near future

## [0.4.7]
### Fixed
- Rewrote and vastly improved thread control for BufferThread
- Unbroke Windows builds (caused by strndup in the new Settings_STR parsing, we just needed to include the existing polyfill)

## [0.4.6]
### Fixed
- Removed debug breakpoint that was left in pipewire.c

## [0.4.5]
### Added
- Pipewire AudioBackend support (Enable with `audio_backend = "pipewire";` in mpl.conf)
- `audio_backend` setting in mpl.conf
- Support for string settings values in mpl.conf

### TODO
- Pipewire's AudioBackend currently doesn't respect the `ab_buffer_ms` setting.

## [0.4.4]

### Added
- Official Windows support! Now that we aren't using a hacky implementation of EventQueue, MPL can be considered usable on Windows.

### Internal
- Completely rewrote EventQueue to be platform independent
- Moved the bulk of what was in main to UI_CLI_mainloop

## [0.4.3]

### Added
- Windows implementation of InputThread
- Windows WASAPI AudioBackend support

## [0.4.2]

### Added
- Integration with FAST (Fast Audio Server for Testing), allowing us to test compatibility on any platform *before* hooking up that platform's audio API(s).
- `ui_timecode_ms` setting. When set to `true`, MPL displays milliseconds in timecodes (i.e track position, track duration).
- Support for boolean settings.
