# iOS known issues and bug reports

This port is still being tested. Do not assume the Android port's compatibility or performance claims apply to iOS.

- Driving and busy scenes can have frame drops. Recent streaming fixes still need broader route and mission testing.
- Mission transitions and longer sessions need further memory and stability testing.
- Backgrounding and returning to the app need broader physical-device testing.
- Full-story completion, all region/language combinations and every controller model have not been verified on iOS.
- The game-data startup check catches missing or empty key files, but cannot detect every incomplete or corrupt asset.

For setup errors, follow [installation troubleshooting](INSTALLATION.md#troubleshooting). Android-era gameplay issues are preserved in [the upstream notes](docs/android-history/KNOWN_ISSUES.md); they are not a confirmed iOS test list.

## Reporting an issue

Open an issue in this repository and include:

- Device model and iOS/iPadOS version.
- App version/build and installation method.
- PC data region/language and whether it is an unmodified retail installation.
- Steps to reproduce, level/mission, expected result and actual result.
- Screenshot or video when helpful.

In **Files → On My iPhone / On My iPad → Simpsons Hit & Run**, share `srr2_health.log` and `srr2_console.log` after the problem and before relaunching. If you already relaunched, include the matching `.previous.log` files too. Include `srr2_crash.txt` if present; it may contain older entries, so mention when the problem happened. Review logs before sharing for any personal information. Do not attach game assets.

Developer investigation details: [iOS runtime diagnostics](IOS_RUNTIME_DIAGNOSTICS.md).
