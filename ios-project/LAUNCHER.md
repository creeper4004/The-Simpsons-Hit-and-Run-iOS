# Native setup launcher

Version 0.2 shows a UIKit launcher before loading the game engine's UI.
The 0.1 IPA predates this feature.

- Existing `Documents/game` data is recognized; select Play to continue.
- Import game folder opens the iOS Files directory picker. Select the root of
  your installed retail PC data, containing art, movies, scripts, sound and RCFs.
- ZIPs, console ISOs and downloads are not supported.
- Imported data is copied into temporary staging. Only the game directories,
  RCF archives and simpsons.ini are copied; executables/installers are omitted.
- The same nonempty startup-file checks are used by the launcher and engine.
  They do not verify every archive, determine region or prove compatibility.
- Failed/cancelled imports leave the existing game directory in place.
  Cancellation takes effect after the current directory/file copy finishes.
- Replacing a complete existing installation requires confirmation. Saves,
  settings and logs outside game are retained. Interrupted swaps retain a
  recovery backup; temporary staging is cleaned on the next launcher check.
- Keep the app foregrounded during import. An import needs temporary free
  storage in addition to any existing game data. Cloud files must be accessible
  through their file provider; provider/storage errors are shown to the user.
- [Launcher settings](SETTINGS.md) cover touch controls, camera preferences,
  FPS display, diagnostic reports and save backup/export. Other game settings
  remain available in-game. Region follows the build.

The launcher uses system controls and a system car symbol, with no retail
screenshots, game backgrounds or bundled game data. It is independent of the
Home Screen icon.

Validation test:

```sh
clang++ -std=c++17 -Icode tests/ios/game_data_requirements_test.cpp -o /tmp/game-data-test
/tmp/game-data-test
```
