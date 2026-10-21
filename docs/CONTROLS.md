# Controls, button glyphs and saves on the PC

The game is the PlayStation game. On the PC its input, its button glyphs and its
saves follow the Windows release where that release had an answer, and the console
where it did not.

## Button glyphs

The console draws every glyph through `DoButton(x, y, icon, text)` (menus.c), with
icons from `startwad.wad` loaded by `LoadButtons`:

| Icon | Console glyph | Used for |
|---|---|---|
| 0 | circle | card slot, screen position, NegCon set, default config |
| 1 | cross | SELECT / ACCEPT / CONTINUE |
| 2 | square | controller config, passwords |
| 3 | triangle | BACK (label drawn to the left) |
| 4, 5 | left / right arrows | hi-scores, class select |
| 6-9 | NegCon variants of 0-3 | |
| 10, 11 | up / down arrows | NegCon config |

Passwords (`DisplayPassword`, `EnterPassword`) are rows of face-button glyphs.
L1/L2/R1/R2 are only ever text ("L1 BUTTON").

The glyph set follows the device the player used last (a key, or a button or stick of a gamepad;
the scripted pad of the lockstep tests does not count). Until something is pressed, and for a
PlayStation pad and for pads of an unknown make, the console's icons stay exactly as they are.

- **Keyboard**: the Windows release's key caps and gold direction arrows, from its
  `PICS/HI-RES/HUD2.TGA` (`platform/keyglyphs.c`, made by `tools/gen_keyglyphs.py`). Cross is ENTER,
  triangle ESC and the arrows are the arrows, texel for texel as that release draws them. The release
  has no other caps: circle (SPACE), square (ALT) and the controller screen's S, F1, A and D are caps
  made of the same pieces (the cap's face, letters drawn as its ESC legend's, E, S and C its own).
- **Xbox pads**: A, B, X, Y buttons with letters in the pad's colours by position (south A, east B,
  west X, north Y).
- **Nintendo pads** (Switch, Joy-Con, GameCube): B, A, Y, X buttons by position (south B, east A,
  west Y, north X).

The key caps are drawn at the Windows release's size (half their texels in console pixels). The
gamepad glyphs are drawn by the port at four times the resolution of their page, with smooth edges,
and sampled filtered: they stay sharp at any window size.
- **PlayStation pads and other pads**: the console's icons.

The glyphs live in a 256x256 page of the platform's texture memory beyond the console's video
memory (`glyphs.c`, `GpuSetGlyphMemory`); `DoButton` rewrites its quad to point at it
(`PortButtonGlyph`). The password symbols (`DisplayPassword`, `EnterPassword`) are shapes to
read, not prompts, and stay the console's on every device. The controller screen shows, for the
keyboard, the key of each action (thrust S, fire SPACE, view F1, drop weapon ALT, left air brake A,
right air brake D) where the console shows the button bound to it.

Button names in the game's text follow the device (`PortButtonLabel`):

| Console text | Keyboard | Xbox | Nintendo |
|---|---|---|---|
| PRESS START (title) | PRESS ENTER | PRESS MENU | PRESS PLUS |
| L1 / L2 / R1 / R2 BUTTON (controller screen, gamepads) | Q / A / E / D | LB / LT / RB / RT | L / ZL / R / ZR |

## Keyboard

The same keys drive the menus and steer the ship, so their meaning depends on what the game is doing.
`race()` tells the platform every frame whether the player is steering (the ship is racing and the
pause menu is not up: `PortRaceControls`); a few fields after the last such call the menus are back.
The menus include the pause menu, the results, the high-score entry and the controller screen.

| Key | Menus | In a race |
|---|---|---|
| Enter | accept (Cross, and Start for the title) | Start (pause) |
| Escape | back (Triangle) | Start (pause) |
| Arrow keys | directional pad | steer, nose up / down |
| Space | Circle | fire |
| Left Alt | Square | drop weapon |
| S | | thrust |
| A / D | | left / right air brake |
| F1 or V | | change view |
| P | Start | Start |
| Q / E | L1 / R1 | L1 / R1 |
| Backspace | Select | |
| F9 | Select | Select |

The race keys press the button the player bound to that action in the controller configuration (the
game's `keyDef[]`; the console's defaults are thrust Cross, fire Circle, drop weapon Square, view
Triangle, air brakes L2 and R2), so the keys keep their meaning when the buttons are changed. A
key held when the game switches contexts and that means something else in the new one (Enter to
pause) is ignored until released, so it does not also accept the first menu entry.

Alt+Enter and F11 switch between window and full screen (the window handles them, the game never
sees them). So do the port's own keys: the backtick (`` ` ``) opens and closes the debug console, and Tab
freezes the game and saves the frame recorder's last seconds of drawing (the console's Frames tab), or
resumes it.

Text entry (names, passwords) uses the directional pad and Cross as on the console; typed letters
are not read.

The controller screen's rows are for gamepads; with the keyboard they show the fixed keys.

## Gamepads

Gamepads are SDL gamepads mapped to the console pad by position, with the console's default
bindings (`InitPadSettings`):

- cross is thrust;
- circle is fire;
- square is drop weapon;
- triangle is view;
- L2 / R2 are the air brakes (0x100 / 0x200 on the digital pad);
- Start is start.

Analog sticks feed the NegCon path (axes 0x8FF0-0x8FF2) only when the player
picks the analog controller type.

Gamepads count only while the game's window has the input focus: SDL reports them
in the background too, so a pad used in another program does nothing here. Buttons
still held when the window gets the focus back stay quiet until they are released.

## Testing the controls

`WIPEOUT_KEYS=file` plays scripted keys and a virtual gamepad into SDL's event queue (see the head of
`platform/controls.c`), so they take the path of real input:

    650 down Return          # from video field 650 on
    655 up Return
    300 pad attach xbox      # xbox, ps5, switch or generic
    700 pad down a           # a is the south button; start, dpdown, ...
    800 focus lost           # the window loses (or, "focus gained", regains) the focus

With `WIPEOUT_NO_GAMEPAD` set (the tests) real gamepads are never opened, only the script's.

`WIPEOUT_VRAM_DUMP=file.ppm` writes video memory at the end of a run (`WIPEOUT_FRAMES`).
`tools/controls/shots.sh` captures the title, main menu, controller screen, pause menu and the results
for the keyboard, an Xbox pad and a PlayStation pad.

## Saves

The console saves to a memory card. The port keeps the card format (a 128 KiB
image per slot in the user's data directory, `libcard.c`), so the game's own
save, load and card screens work unchanged and saves are portable with
emulators. The Windows release's `CONFIG.DAT`/`SAVEGAME.BIN` are not used: the
console game stores more (ghosts, the full options block).
