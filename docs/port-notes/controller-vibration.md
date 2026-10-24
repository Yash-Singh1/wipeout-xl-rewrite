# Controller vibration (the pads' motors)

The game's impact feedback, in the player's hands. The console's pads had no motors — the decompiled
program only ever reads two 8-byte input buffers and never names an actuator — so this is the port's
own feature, driven by the events the console already reacted to (its picture jolt, its impact
sounds). Nothing here changes what the game computes.

## game/

* `include/port_platform.h`: declared `PortPadRumble(int amount)`, after `PortDisplayShake` (the same
  kind of hook: the picture's jolt in another form).
* `src/effects.c`, `SetShake`: one call, `PortPadRumble(amount)`, commented `Port:`. The shake's own
  amount is the strength, so a hit shakes the pads as hard as the picture and no harder. Covers the
  moments the game already reacts to with `SetShake`: the weapon hits and the quake weapon
  (weapon.c), the menu landing (menus.c:6653), the knock of a ship that is losing control
  (control.c:378). `SetShake(0)` (menus.c:530, ships.c:434) stops them, as it stops the picture.
* `src/weapon.c`, `DamageCollide`: one call, `PortPadRumble(3)`, inside the function's own
  `linkSendFlag == 0 && attractDelayFrames <= 0` guard and gated on `shipNo == ownShip`, commented
  `Port:`. This is the hit that only sounds: a wall or another ship (collide.c's `WingCollision`,
  `NoseCollision` and the ship-to-ship block, which play `PlayDirectionalNote` and call
  `DamageCollide(129/130, ...)`), and the two weapon types that do not shake the screen
  (weapon.c:960, 1492). `damageType != 0x80` leaves out the pit lane's repair (`CheckPitShips`), which
  is a negative damage and no hit at all.

Both call sites are the whole of it: every other `SetShake`/`DamageCollide` caller is an AI ship or a
menu, and the player's own hits all pass through these two.

## platform/

* `psx/pad.h`, `psx/pad_sdl.c`: `PsxPadRumble(int amount)` drives the open gamepads with
  `SDL_RumbleGamepad` (the big motor at the strength, the fine one at half). The game's shake units
  are 2 (a knock) to 20 (a rocket): `8192 + (32767 - 8192) * amount / 20` of strength, above a floor
  so the lightest knock is still felt, for `100 + amount * 20` ms. A jolt that is still running keeps
  its strength and its time, so the `DamageCollide` taps do not end a rocket's, and a wall scrape
  (which calls it every frame while in contact) keeps itself going. One player: every gamepad is the
  player's (as `Route` has it); with the two players' link devices assigned, the first player's
  controller alone. Nothing when the window is not focused.
* `main.c`: `PortPadRumble` passes `0` when the player has turned the motors off (`portSettings.noRumble`)
  or this is a test's hidden run, which also stops a jolt that was running. It lives here with
  `PortLogoFields`, `PortFarViewEnabled` and `PortFadeRange`, the other hooks that read the settings;
  `psxio` (which `tools/iotest` links without `platform/`) must not depend on them.
* `settings.h`, `settings.c`: `noRumble`, saved as `vibration = %d` (the `autosave`/`noAutoSave` idiom).
* `debug_ui.cpp`: "Controller vibration" in the debug console's Game tab.

## Oracle

`tools/oracle32.py diff B0` was **not** run: this machine's LLVM has no MIPS backend ("unable to
create target: 'No available targets are compatible with triple mipsel-unknown-none-elf'"), and the
tool wants Homebrew's. The expected result, when it is run, is two changed functions and no others:

* `SetShake` — gains the `PortPadRumble` call (as `ShakeScreen` gained `PortDisplayShake`).
* `DamageCollide` — gains the `PortPadRumble` call.

Neither touches a representation, a size or an offset; the console's own computation is unchanged.

`tools/diag.py`'s clang command (run by hand: `tools/cc.sh` is a shell script and this is Windows) is
clean on `effects.c`, `weapon.c` and `control.c`. `tools/xdecl.py` reports nothing for the two new
names (its other findings are pre-existing `struct X`/`X` typedef pairs and this toolchain's MSVC
stdio headers).

## Not done, and unsure

* The two players' link: a jolt on the second player's ship is not sent over the link, so the second
  player's controller (which is plugged into the first machine) does not shake. `PortLinkTransfer`'s
  packet has no room for it and the game's own screen shake is not sent either.
* The web build's Gamepad API has no rumble in every browser; `SDL_RumbleGamepad` fails quietly there.
* No `WIPEOUT_RUMBLE` environment override, unlike the other settings; say if the tests want one.
