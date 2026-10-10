# Install on iPhone or iPad

## What you need

An arm64 device on iOS/iPadOS 15 or later, an iOS build signed for your device, and your own retail PC copy of *The Simpsons: Hit & Run*. Use the installed retail PC data, not console-disc files or development assets. No game download is provided here.

## 1. Install the app

Follow the installation/signing instructions supplied with the iOS build you downloaded. If your download is source code rather than an IPA, it must be built and signed before it can run on a device. APK files are for Android and cannot be used here.

Open **Simpsons Hit & Run** once. The app creates its data folders and may display a message that game data is missing. Close the app before copying files.

## 2. Prepare your game files

From your installed retail PC game folder, copy the following data into a folder named `game`, preserving filenames and subfolders:

```text
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

Keep the complete contents of the four directories. Windows executables and installers are not needed. Copy `simpsons.ini` if present in your installation.

Keep any additional language archives supplied with your copy: `dialogs.rcf` (Spanish), `dialogf.rcf` (French), and `dialogg.rcf` (German). Use data matching the build's region/language. The current startup check also requires a nonempty `dialog.rcf`; do not rename a different language's archive to bypass it.

## 3. Transfer the data

### Using Files on your device

1. Make your prepared folder available to Files, for example through iCloud Drive or an external drive.
2. Open **Files → Browse → On My iPhone** (or **On My iPad**) **→ Simpsons Hit & Run**.
3. Open the app's `game` folder and copy the prepared folder's **contents** into it. Wait for the transfer to finish and ensure cloud files have downloaded locally.
4. Confirm that `game/art` exists. Avoid `game/game/art` or `game/game_assets/art`.

### Using a Mac

1. Connect and trust your device, then select it in Finder.
2. Open the **Files** tab and locate **Simpsons Hit & Run**.
3. Drag your prepared `game` folder into the app's file-sharing area. The folder belongs directly in the app's Documents area.
4. Wait for the transfer to finish. Check the layout in the device's Files app before launching.

## 4. Launch and play

Reopen the app in landscape orientation. Touch resources are included with the app; you do not need to download them separately. See [touch controls](TOUCH_CONTROLS.md) and [controller mappings](CONTROLLER_CONTROLS.md).

## Updates and backups

Installing an update over the same app normally retains `game` and saves. You do not need to transfer the game data again. **Deleting the app deletes its Documents folder**, including game data and saves. Copy the app's Documents contents to a safe location before deleting it or changing your installation method.

Older iOS installs with data at the top of Documents migrate those files into `game` on launch. Existing files in `game` are retained. Saves, logs and touch settings remain outside `game`.

## Troubleshooting

- **Missing/incomplete game data:** verify the layout, wait for copying to finish and check that files are nonempty. In particular, `art/frontend/scrooby/bootup.p3d`, `art/frontend/scrooby/ingame.p3d`, `scripts/ss.mfk` and the listed archives must be present. The startup check is not a full integrity check.
- **App absent from Files:** launch it once, then reopen Files and look under local device storage.
- **Audio or movie problems:** check that you copied `movies`, `sound` and all archives from the same retail installation, with the language data expected by your build.
- **Crash or visual problems:** see [known issues](KNOWN_ISSUES.md) and include your device, OS, build and reproduction steps in an issue.

Technical details: [external iOS game data](ios-project/GAME-DATA.md).
