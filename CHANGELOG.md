# iOS changelog

## 0.2 — setup, settings and diagnostics

- Fixed MGL texture deletion leaving an active texture slot with a null pointer, which caused a renderer assertion during New Game/front-end transitions. Also corrected texture-mask shifts for higher texture units.

- Added a native iOS setup launcher with folder import, startup-file checks, import cancellation and a Play action.
- Imports stage new files before replacing game data and retain saves/settings outside the game folder.
- Added the app icon for iPhone and iPad.
- Integrated IndecentDad's physical-controller touch-control visibility fix.

- Added touch-opacity sliders, automatic/always-show/always-hide controller visibility, camera sensitivity and invert-Y settings.
- Added an optional compact FPS/frame-interval counter.
- Basic logging is the default; Detailed adds performance samples and bounded runtime traces. Repeated texture-upload chatter is disabled in Basic mode.
- Added diagnostic report export, log clearing, save-slot backup/export and settings reset.

Validated device/simulator builds, automated settings/logging/texture tests, native settings interactions and save-backup copying. The user confirmed folder import, the New Game crash fix, FPS display and controller visibility on an iPhone 17 Pro Max running iOS 27.2 beta. Full-story completion and broad device/edition compatibility remain unverified.

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
