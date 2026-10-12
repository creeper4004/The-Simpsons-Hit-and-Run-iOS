# iOS changelog

## Unreleased

- Added a native iOS setup launcher with folder import, startup-file checks, import cancellation and a Play action.
- Imports stage new files before replacing game data and retain saves/settings outside the game folder.
- Added the app icon for iPhone and iPad.
- Integrated IndecentDad's physical-controller touch-control visibility fix.

The launcher has passed device/simulator builds and startup-file validation tests; full-size folder import still needs physical-device testing.

## 0.1 — first public iOS release

- Device IPA for arm64 iPhone/iPad, requiring users to sign it for their device.
- Retail game data is excluded; import your own PC game files into `Documents/game`.
- Includes touch controls, controller support and the recent iOS streaming improvements.
- See [installation instructions](INSTALLATION.md) and [known limitations](KNOWN_ISSUES.md).

## Public-release preparation

- Replaced Android installation instructions with iPhone/iPad setup and Files/Finder game-data transfer instructions.
- Added credits for Carlox33's Android port and ZenoArrows' earlier platform ports.
- Preserved the original multilingual Android documentation as historical reference.
- Documented current iOS limitations, backups and bug-report information.

## Recent iOS development

- External game data now lives in `Documents/game`; normal builds omit PC game assets from the app bundle.
- Older Documents-root data migrates into `game`, keeping saves and settings separate.
- Added persistent runtime logs and startup loading of saved progress.
- Improved loader scheduling and streaming diagnostics; restored swap encoder setup after a clipped gameplay viewport regression.

Detailed investigation records remain in the `IOS_*.md` files and [notes](notes).

[Original Android version history](docs/android-history/CHANGELOG.md).
