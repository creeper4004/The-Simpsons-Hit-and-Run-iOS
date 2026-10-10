# iOS public-release checklist

## Packaging

- Build the device Release app with `SRR2_IOS_EMBED_GAME_ASSETS=OFF` and verify the final app/IPA contains no retail PC data or stale `game_assets` folder.
- Set the app version/build for the intended release and use your own signing team/profile.
- Supply an installable IPA with its supported signing/install instructions, or clearly label a source-only release. Simulator apps cannot run on phones.
- Include this README, installation guide, credits and applicable third-party notices with the release.
- Retain dependency copyright/license notices and satisfy their distribution requirements, including the FFmpeg build's applicable terms.

## Physical-device checks before publishing

- Fresh install without game files shows a helpful missing-data message and appears in Files.
- Import a complete retail data set via Files and Finder; verify startup, sound and movies.
- Test touch input, a paired controller, landscape display and the full gameplay viewport.
- Save, relaunch, resume, update over the existing app and verify progress survives.
- Test mission transitions, longer driving routes and background/resume. Record tested devices and OS versions.
- Verify an update reuses `Documents/game` without another transfer, and an older install migrates correctly.
- Publish known limitations and the exact region/language supported by the build.

Documentation preparation does not replace these release checks. No public binary is produced or published by editing the docs.
