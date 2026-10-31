# Issue fixes and remaining checks

Local investigation of the open issue reports, October 29, 2026.

- #9: recognize PALANIM.EXE as the intro player, alongside NTSCANIM.EXE. European discs
  previously returned before playback, regardless of the skip setting. Tests cover both
  names, case differences, the four endings and an unknown program. This does not add
  playback of externally supplied videos.
- #11: keep XL's geometry rotation off guest fan objects. Wipeout 64 animates the fan's
  texture; rotating its plane as well made it tumble.
- #12: retain the console's additive thrust fill. DuckStation software-rendered captures
  confirm that it washes out over the bright Gare d'Europa starting grid too. The
  opaque workaround changed the design and exposed black texels; it has been reverted.
- #13: lower checkpoint boards beneath overhead scenery polygons on Wipeout 64 tracks.
  The board's highest vertex and the roof's lowest vertex determine clearance, with
  128 units of space. Heights are cached per section and board height for the race.
  Synthetic tests cover a translated roof, an uncovered section and different board
  heights. The reported Qoron IV location still needs a gameplay check.
- #6: fix the Windows Clang build's second-game BSS boundaries. Its initialized markers
  previously occupied a separate PE section, leaving the second game's state unchanged
  on restart. The final Windows executable now checks that its mutable second-game
  globals lie inside the reset ranges. The released 0.0.6 Arm binary fails this check;
  the rebuilt Arm and x64 binaries pass. A Windows x64 Wine startup run started player
  two without reproducing the reported crash. This does not establish that all desktop
  multiplayer crashes are resolved or verify a complete two-player race.
- #10 remains unverified. Main already contains a Wipeout 64 sky-height correction
  in 9149a84. The issue's Windows screenshot also shows scenery texture problems;
  the height correction alone does not establish that this report is resolved.
- #14 remains unresolved. No ship speeds, jump flags or physics constants were changed.
  The Altima VII Rapier jump needs a reproducible Qirex run before choosing a correction.

Run the focused tests with `tools/bugtest/run.sh`. They require a C compiler,
SDL3 and pkg-config, and no game data. Windows builds also run the reset-range check.
The native macOS, Windows x64 and Windows Arm builds succeed. Compiler diagnostics
for the changed game files are clean. No struct layouts or compiler flags changed;
`portorig.h` adds the checkpoint-height function declaration.

The broader I/O test build is blocked by existing test setup problems. Its SDL
configuration leaves PsxFieldCount and portPal undefined; the configuration without
SDL omits an SDL include path needed by libcd.c. The declaration checker reports
six existing typedef/spelling differences, unrelated to these changes. The parent
PC decompilation's progress.py gate does not exist in this PlayStation port checkout.
