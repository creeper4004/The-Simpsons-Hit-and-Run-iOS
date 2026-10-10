# External iOS game data

Normal builds omit the game's data from the app/IPA. The app reads it from
`Documents/game`, visible in Files under **On My iPhone → Simpsons Hit & Run → game**.
App updates retain this folder. Deleting the app removes its Documents data.

For a fresh install, copy your game data into `game` so that it directly contains:

```
game/
  art/
  movies/
  scripts/
  sound/
  ambience.rcf
  carsound.rcf
  dialog.rcf
  music00.rcf
  music01.rcf
  music02.rcf
  music03.rcf
  nis.rcf
  scripts.rcf
  soundfx.rcf
  simpsons.ini
```

Additional language archives may also be present. Do not nest `game_assets` or
another `game` inside this folder. Close the app while transferring files, then
reopen it. Missing startup resources show a message explaining this location.
The startup check detects missing/empty sentinel files; it is not a full archive
integrity check.

Existing installs automatically move the previously extracted top-level game
assets into `game` on first launch, using renames within Documents rather than
copying the data again. Existing destination entries are retained. Saves, logs,
and touch controls remain in their original Documents locations.

Transfer game data once using Files or Finder's file-sharing panel. Subsequent
app updates transfer only the executable, libraries and small touch assets.
No separate asset transfer is needed for the phone that already has the data.

## Build option

`SRR2_IOS_EMBED_GAME_ASSETS` defaults to `OFF`. Reconfigure an existing build:

```
cmake -S . -B build_ios_device -DSRR2_IOS_EMBED_GAME_ASSETS=OFF
```

A subsequent app build also removes stale bundled `game_assets` left by earlier
builds. For a self-contained build, set the option to `ON`; this requires the
repository's `game_assets/` tree and seeds Documents/game when data is absent.
The `ios-project/build-ios.sh` script accepts the same name as an environment
variable. Small touch control resources remain bundled in both modes.

## Current rollout

Simulator and signed device Release builds succeeded with embedding disabled.
Both app bundles are approximately 28 MiB, compared with about 1.9 GiB before.
The real-device and simulator startup logs report `gameassets:migrated-ready`;
existing data was reused without a new asset transfer. FFmpeg's direct movie
path is also resolved under Documents/game (it bypasses the radFile drive).
