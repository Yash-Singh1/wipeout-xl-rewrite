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
- #13: fit the guest checkpoint board between the tunnel roof and the racing surface.
  Read quad corners in perimeter order instead of triangle-strip order. In low tunnels,
  scale the board uniformly so lowering it cannot bury its lower edge in the road.
  Matched Qoron IV replays verify the clearance; synthetic tests cover both boundaries.
- #6: fix the Windows Clang build's second-game BSS boundaries. Its initialized markers
  previously occupied a separate PE section, leaving the second game's state unchanged
  on restart. The final Windows executable now checks that its mutable second-game
  globals lie inside the reset ranges. The released 0.0.6 Arm binary fails this check;
  the rebuilt Arm and x64 binaries pass. A Windows x64 Wine startup run started player
  two without reproducing the reported crash. This does not establish that all desktop
  multiplayer crashes are resolved or verify a complete two-player race.
- #10: rebind Wipeout 64's placeholder sky model to its panorama tiles and their actual
  UV dimensions. Its original texture-zero bindings sampled outside the first 56x36
  image and produced black-and-white panels. Matched captures reproduce and verify
  this in the native Mac executable and the Windows x64 executable under Wine.
- #14: use Qirex's original Rapier profile on the original Wipeout guest circuits.
  The XL profile's greater thrust and different mass, resistance and skid overshot
  Altima VII's descent. Steering-assisted, full-throttle player-physics runs reproduce
  the launch with the XL profile and verify landing with the guest profile. XL and
  Wipeout 64 keep their existing profiles; Venom and other teams are unchanged.
- #2 was fixed in 0.0.4 and the reporter confirmed it. A rebuilt 0.0.3 and the current
  port provide the interpolated race captures used for the motion comparison.

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
