# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## Unreleased

### v0.4.5
MPL is in AGGRESSIVE BUG HUNTING MODE for v0.4.5+ until we hit v0.5.0! The next release, v0.4.5, is going to fix a *hardware* problem with automatic sample rate switching in certain DACs (including my own Schiit Modius). Sample rate switching can introduce a delay during which whatever frames are sent to the DAC get dropped, causing a period of silence at the start of any track that causes sample rate switching. The solution is to detect when a DAC is going to switch sample rates (we need to use the Pipewire API to do this) and allow the user to configure a delay period for MPL to send quiet noise at the new sample rate. Once the noise has triggered the sample rate switch, we THEN start sending audio frames, ensuring that every frame at the beginning of a track gets heard!

### v0.5.0
The major release will be v0.5.0. This release will see a massive increase in the power of mpl.conf keybinds delivered by creating a modular UserInterface struct that's exposed to keybind functions. This will allow us to do things like "bind this key to show current config", "bind this key to change log level", "bind this key to show/hide this UI element", etc. Since keybinds are run on the main thread, the best part is there's absolutely no contention between the abstract UI methods and the main UI loop!

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
