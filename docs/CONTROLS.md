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

The glyph set follows the device the player used last:

- **Keyboard**: the Windows release's key caps from `PICS/HI-RES/HUD2.TGA`
  (768x256, A1R5G5B5, three 256x256 pages; caps on the first page):
  ESC (158,162,30,30) for triangle/BACK, ENTER (158,194,30,30) for cross,
  arrows (160,226,32,16) and (194,226,32,16). Glyphs with no PC art (circle,
  square, the password symbols) are drawn as key caps showing the name of the
  bound key, in the same cap style. Button names in text ("L1 BUTTON") become
  the bound key's name.
- **PlayStation controller**: the console's own icons, unchanged.
- **Other gamepads**: the console's icons by position (south = cross, east =
  circle, west = square, north = triangle), until per-family art exists.

When the Windows release's files are missing, the keyboard glyphs fall back to
drawn key caps with the key name.

## Keyboard (the Windows release's defaults)

| Action | Key |
|---|---|
| Thrust | S |
| Fire | Space |
| Drop weapon | Left Alt |
| Left / right air brake | A / D |
| Steer | Left / Right |
| Nose down / up | Up / Down |
| View | F1 |
| Start / pause | Enter (race: Esc also pauses, as on the PC) |
| Menu accept / back | Enter / Esc |
| Select | F9 |

Keys are rebindable in the controller screen. The console's `JoyPadConfig`
screen stays for gamepads. For the keyboard, the binding rows show key names.

## Gamepads

Gamepads are SDL gamepads mapped to the console pad by position, with the
console's default bindings (`InitPadSettings`):

- cross is thrust;
- circle is fire;
- square is drop weapon;
- triangle is view;
- L2 / R2 are the air brakes (0x100 / 0x200 on the digital pad);
- Start is start.

Analog sticks feed the NegCon path (axes 0x8FF0-0x8FF2) only when the player
picks the analog controller type.

## Saves

The console saves to a memory card. The port keeps the card format (a 128 KiB
image per slot in the user's data directory, `libcard.c`), so the game's own
save, load and card screens work unchanged and saves are portable with
emulators. The Windows release's `CONFIG.DAT`/`SAVEGAME.BIN` are not used: the
console game stores more (ghosts, the full options block).
