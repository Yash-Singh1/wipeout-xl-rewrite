# Controller vibration (the pads' motors)

The game's impact feedback, in the player's hands, and the launch thrust's growl. The console's pads had
no motors — the decompiled program only ever reads two 8-byte input buffers and never names an actuator —
so this is the port's own feature, driven by events the console already reacted to (its picture jolt, its
impact sounds, the launch thrust it judges at the start line). Nothing here changes what the game
computes.

## game/

* `include/port_platform.h`: declared `PortPadRumble(int amount)` and `PortPadRumbleLevel(int level)`,
  after `PortDisplayShake` (the same kind of hook: the picture's jolt in another form).
* `src/effects.c`, `SetShake`: one call, `PortPadRumble(amount)`, commented `Port:`. The shake's own
  amount is the strength, so a hit shakes the pads as hard as the picture and no harder. Covers the
  moments the game already reacts to with `SetShake`: the weapon hits and the quake weapon
  (weapon.c), the menu landing (menus.c:6653), the knock of a ship that is losing control
  (control.c:378). `SetShake(0)` (menus.c:530, ships.c:434) stops them, as it stops the picture.
* `src/weapon.c`, `DamageCollide`: one call, `PortPadRumble(4)`, inside the function's own
  `linkSendFlag == 0 && attractDelayFrames <= 0` guard and gated on `shipNo == ownShip`, commented
  `Port:`. This is the hit that only sounds: a wall or another ship (collide.c's `WingCollision`,
  `NoseCollision` and the ship-to-ship block, which play `PlayDirectionalNote` and call
  `DamageCollide(129/130, ...)`), and the two weapon types that do not shake the screen
  (weapon.c:960, 1492). `damageType != 0x80` leaves out the pit lane's repair (`CheckPitShips`), which
  is a negative damage and no hit at all.
* `src/control.c`, `UpdatePlayerShipStart`: one call, `PortPadRumbleLevel(level)`, at the end of the
  body, gated on `shipNo == ownShip`, with `boostAt`, `stallAt` and `rumbleLevel` added to the
  function's declarations, all commented `Port:`. This is the start's launch thrust, which the console
  already judges and this port has been leaving the player to feel out by ear: the countdown
  (`UpdatePlayerShipStart`) ramps `playerShip->thrust` as the throttle is held, and at the moment the
  race starts `UpdatePlayerShip` (`updateCount == PAL_TIME(90)`) boosts the ship for a thrust of
  `5/8 * maxThrust` to `6/8 * maxThrust` (its `launchThrust * 5` .. `launchThrust * 3 << 1`) and stalls
  it at `6/8` and above. The level rises with the throttle to 8 at that window's floor, is 8 through it,
  and is 4 past it: hard in the amount that boosts, quiet in the amount that stalls. The division is
  only in the branch where `boostAt > 0` (`thrust < boostAt` is false when it is 0), so there is none by
  zero. The function is not called for a demonstration: when the game goes to attract mode it moves the
  player's ship to `UpdateDroneShipStart`/`UpdateDroneShip` and clears `RACING` (ayrace.c,
  `CheckShipDestroyed`), and the level stops by itself a few fields later (below).

## platform/

* `psx/pad.h`, `psx/pad_sdl.c`: `PsxPadRumble(int amount)` (a jolt) and `PsxPadRumbleLevel(int level)`
  (a level held), both in the game's units, 2 to 20. `rumble_apply` runs the open gamepads with
  `SDL_RumbleGamepad` at whichever of the two is stronger, and stops them when both have passed.
  The strength is `32767 * sqrt(level / 20)`: the square root because a pad's motors are weakest exactly
  where the levels are lowest, so the knocks are felt and the rocket keeps its lead over them. Both
  motors run at that strength — the small one is the buzz a growl is felt through (a DualShock's right),
  which the half-strength fine motor of the earlier version was not. (This replaces that version's
  `level * 32767 / 20`, and the `DamageCollide` tap went from 3 to 4 with it, so a wall still taps.)
  A jolt runs `100 + amount * 20` ms and a weaker one that comes while it runs does not end it (a wall
  scrape does not end a rocket's). A level lives `HOLD_MS` (100 ms) after the last call, refreshed every
  frame by `UpdatePlayerShipStart`, and `rumble_tick` — from `PsxPadSdlField`, once a field — stops the
  motors when the game stops saying it. One player: every gamepad is the player's (as `Route` has it);
  with the two players' link devices assigned, the first player's controller alone. Nothing runs while
  the window is not focused.
* `main.c`: `PortPadRumble` and `PortPadRumbleLevel` pass `0` when the player has turned the motors off
  (`portSettings.noRumble`) or this is a test's hidden run, which also stops what was running, and scale
  the level by the strength the player set (`portSettings.rumbleStrength`, 0: the scale's own). They live
  here with `PortLogoFields`, `PortFarViewEnabled` and `PortFadeRange`, the other hooks that read the
  settings; `psxio` (which `tools/iotest` links without `platform/`) must not depend on them.
* `settings.h`, `settings.c`: `noRumble` (saved as `vibration = %d`, the `autosave`/`noAutoSave` idiom)
  and `rumbleStrength` (`vibration_strength = %d`, 0: 100 like `touchSize`).
* `debug_ui.cpp`: "Controller vibration" and its "Strength" slider (50-300%) in the debug console's Game
  tab.

## Oracle

`tools/oracle32.py diff B0` was **not** run: this machine's LLVM has no MIPS backend ("unable to create
target: 'No available targets are compatible with triple mipsel-unknown-none-elf'"), and the tool wants
Homebrew's. The expected result, when it is run, is three changed functions and no others:

* `SetShake` — gains the `PortPadRumble` call (as `ShakeScreen` gained `PortDisplayShake`).
* `DamageCollide` — gains the `PortPadRumble` call.
* `UpdatePlayerShipStart` — gains the `PortPadRumbleLevel` call and three locals.

None touches a representation, a size or an offset; the console's own computation is unchanged. The
locals are plain `int`s at the end of the function's own declaration block, so `ORACLE_NOSP` is not
needed to read the diff.

`tools/diag.py`'s clang command (run by hand: `tools/cc.sh` is a shell script and this is Windows) is
clean on `effects.c`, `weapon.c`, `control.c`. `tools/xdecl.py` reports nothing for the new names (its
other findings are pre-existing `struct X`/`X` typedef pairs and this toolchain's MSVC stdio headers).

## Not done, and unsure

* The two players' link: a jolt on the second player's ship is not sent over the link, so the second
  player's controller (which is plugged into the first machine) does not shake. `PortLinkTransfer`'s
  packet has no room for it and the game's own screen shake is not sent either.
* The web build's Gamepad API has no rumble in every browser; `SDL_RumbleGamepad` fails quietly there.
* No `WIPEOUT_RUMBLE` environment override, unlike the other settings; say if the tests want one.
* The levels of the launch growl (8 in the window, 4 past it) and the strength curve are tuned by
  reasoning, not by hand: there is no rumble-capable pad in this environment. The first version was
  reported weak on a DualShock 4, which is why the curve is now a square root and both motors run; the
  "Strength" slider (50-300%) is the player's own tuning on top. The numbers are one line each
  (`control.c`) and `rumble_strength` (pad_sdl.c).
