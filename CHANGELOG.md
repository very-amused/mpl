# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## Unreleased

MPL is in AGGRESSIVE BUG HUNTING MODE for v0.4.5+ until we hit v0.5.0!

### v0.5.0
The next major release in terms of new features will be v0.5.0.

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
