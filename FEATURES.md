# iOS features and status

- iPhone and iPad app targets, in landscape orientation.
- Integrated touch controls inherited from the Android port, with context-dependent layouts and in-game customization.
- External controller input; see [mappings](CONTROLLER_CONTROLS.md). Individual controllers still need iOS testing.
- Game data imported through Files or Finder into `Documents/game`, separate from the app bundle.
- Persistent saves and startup loading of saved progress.
- OpenGL rendering translated to Metal through MGL; FFmpeg movie playback and OpenAL Soft audio.
- Shareable runtime logs for troubleshooting.

The iOS version remains under testing. Performance varies by device and scene. Full-game completion, every language/region and all controllers have not been validated for a public iOS release. See [known issues](KNOWN_ISSUES.md).

The original Android feature descriptions are retained in [the historical documentation](docs/android-history/FEATURES.md).
