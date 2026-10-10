# The Simpsons: Hit & Run — iOS Port

An unofficial community port of *The Simpsons: Hit & Run* for iPhone and iPad, adapted from the Android port. Bring your own files from an owned retail PC copy of the game. Game data is not included in normal iOS builds.

**Start here: [Installation and game files](INSTALLATION.md).**

## Requirements

- An arm64 iPhone or iPad running iOS/iPadOS 15 or later (the current build target).
- An iOS build installed and signed for your device. An Android APK will not work on iOS.
- The retail PC game's data files and enough free storage for them. The data is roughly 2 GB; leave additional room for transfers and saves.

The minimum OS target does not guarantee acceptable performance on every device. This port is still being tested; frame drops, visual issues and crashes may occur. Full-story completion and stable 60 FPS have not been established for this iOS version.

## Getting started

1. Install the iOS app using the instructions accompanying your build.
2. Open it once to create its folders, then close it. A missing-game-data message is expected.
3. Copy your PC game data into **Files → On My iPhone / On My iPad → Simpsons Hit & Run → game**.
4. Reopen the app and play in landscape orientation.

The `game` folder must directly contain `art`, `movies`, `scripts`, `sound` and the `.rcf` archives. See the [complete folder layout and transfer instructions](INSTALLATION.md). Updates retain data; deleting the app removes its files and saves, so back them up first.

## Controls and help

- [Touch controls](TOUCH_CONTROLS.md)
- [Controller mappings](CONTROLLER_CONTROLS.md)
- [Cheat combinations](CHEATS.md)
- [Known issues and troubleshooting](KNOWN_ISSUES.md)
- [Current features](FEATURES.md)
- [iOS changelog](CHANGELOG.md)
- [Credits](CREDITS.md)

For an iOS bug, open an issue in this repository with your device, OS version, app version, game region/language and steps to reproduce it. Include screenshots and relevant logs when available. Please report iOS issues here rather than directing them to the Android project's community.

## Credits

This iOS adaptation builds on **[Carlox33's original Android port](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android)**, including its mobile input work, touch controls and icons, controller support and engine improvements. The Android project in turn credits **[ZenoArrows](https://github.com/ZenoArrows/The-Simpsons-Hit-and-Run)** for the earlier Nintendo Switch and PS Vita ports. Thank you to the original game team and the community contributors whose work made these ports possible. See [CREDITS.md](CREDITS.md) for dependencies and the preserved Android documentation.

This project is unofficial and is not affiliated with or endorsed by the game's creators or rights holders.

## Development

The iOS app uses SDL2, the game's engine, FFmpeg, OpenAL Soft and MGL's OpenGL-on-Metal translation. Build helpers are in [ios-project](ios-project); renderer build notes are in [mgl-ios/README](mgl-ios/README). Public builds should leave `SRR2_IOS_EMBED_GAME_ASSETS` disabled. [Release checklist](RELEASING.md).
