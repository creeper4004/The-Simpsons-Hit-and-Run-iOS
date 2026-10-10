# iOS changelog

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

These entries describe development changes, not a claim that a public IPA has been published. Detailed investigation records remain in the `IOS_*.md` files and [notes](notes).

[Original Android version history](docs/android-history/CHANGELOG.md).
