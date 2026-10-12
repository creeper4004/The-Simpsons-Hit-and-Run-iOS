# Launcher settings in iOS 0.2

Open Settings on the setup screen before selecting Play. Preferences persist
across launches and app updates using the same app identity. Game data is not
included in the IPA.

## Touch controls and camera

- Button opacity and pressed opacity: 0–100%. Defaults are 25% and 85%.
  Zero opacity makes buttons transparent; it does not disable their input.
- Controller visibility: Auto hides controls while a physical controller is
  connected; Show keeps them visible; Hide disables touch controls entirely.
  Relaunch to change this if you chose Hide without a working controller.
- Horizontal and vertical camera sensitivity: 25–300% of the original iOS
  defaults. Each defaults to 100%. Invert camera Y defaults to on.

Opacity selections are applied through the existing touch configuration manager
when the engine starts, preserving custom button layouts. Launcher opacity
preferences take precedence over subsequent manual opacity edits to the text
file. Other touch layout editing remains available in-game.

## Performance and diagnostics

- Show FPS counter: off by default. Shows game frame submissions per second
  and average frame interval, updated roughly once per second. It is not a
  GPU execution-time measurement or Apple's Metal HUD.
- Detailed logging: off by default. Basic retains crashes, errors, startup/load
  progress and important events. Detailed adds five-second health sampling,
  renderer frame statistics and bounded runtime traces. Heavy vertex probes
  and full per-encoder verbose logging are not enabled by this switch.
- Share diagnostic report snapshots the last 2 MiB of each selected diagnostic
  file into one text report and opens the system share sheet. Review the report
  before sending it. No game data or save-slot contents are included.
- Clear logs asks for confirmation and clears diagnostic records. Logging
  continues afterward. Game data, saves and backups are kept.

No performance gain from Basic versus Detailed has been benchmarked.

## Save backup and export

Back up and export saves copies existing nonempty Save1–Save8 slot files into a
new dated folder in Documents/Save Backups, then opens the share sheet for the
copied files. The original slots remain in place. Cancelling the share sheet
keeps the local backup. If no slots exist, the app reports that instead.

Find local backups in Files → On My iPhone/iPad → Simpsons Hit & Run → Save
Backups. Deleting the app also deletes its local backups: use the share sheet
or Files to keep a copy elsewhere. To restore, close the app and copy the
original SaveN filenames from a compatible backup into the app's Documents
root (alongside game, not inside game), replacing a slot only if intended.
There is no automatic restore/import button in this release.

Settings reset returns these launcher options to defaults. It does not erase
save slots, game data or custom button layouts. Audio and other existing game
options remain accessible in-game.
