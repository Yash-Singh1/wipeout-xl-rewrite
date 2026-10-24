# Sound, disc, controller and memory card

`platform/psx` implements the PsyQ libraries the game calls for these: libspu, libcd, libpad
(the BIOS pad driver), libcard (and the BIOS file calls for "bu00:"), and the event and
critical-section calls of libapi. The SPU and the CD drive are models of the hardware, not
shortcuts: the game's own voice management, key-on/off, envelopes, reverb and music sequencing run
against them exactly as against the console.

## Wiring (platform side)

    PsxIoInit(&config, error, size)   once, after SDL_Init(AUDIO | GAMEPAD | EVENTS), before PsxMain
    PsxIoField(field)                 at the start of every PsxVBlank(), before the VSync callback
    PsxInputEvent(&event)             for every SDL event from the event pump
    PsxIoShutdown()                   on exit
    PsxVBlank()                       provided by libetc.c; the libraries call it while waiting

`PsxIoConfig`: `discPath` (.cue), `savePath` (card images), `fixedStep`, `muted` (SDL's dummy
driver is chosen by the caller, as now), `noDevice`, `cdSpeed` (1 = the console's drive speed:
loading a level takes as long as on the console; the game runs with 1000, WIPEOUT_CD_SPEED, so data
arrives as fast as the image is read and only the game's own fades remain), `padScript`
(scripted input), `wavPath` (fixed-step: write the mix to a WAV).

The BIOS file calls are `PsyqOpen/Close/Read/Write/Rename` (the names psyq.h maps `open`... to),
`format`, `delete`, `firstfile`, `nextfile`. `Exec(block, argc, argv)` resets the drive (the
movie program takes over the machine) and calls the hook set with `PsxSetExecHook`, if any.

## Time

One clock for the SPU and the drive: the count of output samples at 44100 Hz.
- With an audio device (SDL3, 44.1 kHz stereo, 512-frame buffers) the device's thread pulls
  samples, 256 frames at a time, and advances the clock. The game thread and the audio thread
  share one lock, held briefly; a game call lands at the next block.
- Fixed-step (and when no device opens): `PsxIoField(n)` renders up to exactly
  `n * 44100 * 1001 / 60000` samples. No wall clock, no device: two runs give identical audio.
  Waits shorter than a field (a command's reply) advance the clock directly.
- Queued audio (`queueAudio`; the web, or `WIPEOUT_QUEUE_AUDIO=1` on the desktop): the clock
  advances as in fixed-step mode, from the video fields, and the samples rendered are queued for
  the device instead of dropped (about 46 ms in hand when it runs dry, at most 186 ms queued). In
  a browser the device's callback runs on the game's own thread, only while the game waits, so
  the clock cannot follow the device there: every reply from the drive would wait for the
  browser's next audio callback.
- While the game waits for the drive (`WaitEvent`, `CdReadSync`, `CdControlB`, `CdSync`),
  video fields pass through `PsxVBlank()` (the loading screen animates from its VSync callback).
  `TestEvent` polled 256 times in a row without result also lets a field pass.
- Events from the drive (command complete, error) are queued by the audio thread and delivered
  on the game thread by `PsxIoService`; handlers installed with `EvMdINTR` run there, never on the
  audio thread, and wait for the end of a critical section.

## SPU (spu.c, libspu.c)

24 voices, 512 KiB RAM, 4-bit ADPCM with the exact predictor rounding, 4-point Gaussian
interpolation with the 12-bit fractional position, ADSR and volume sweeps by the psx-spx counter
rules, noise, pitch modulation, loop/repeat flags and ENDX, the capture buffers, and the reverb
unit (the 39-tap resampling filters and comb/all-pass network over the work area in RAM).
`test_spu_oracle` replays a console emulator's recording of the retail game (337,375 register
writes, 251 s) and matches all 24 voices, the reverb return and the master mix sample for sample.

libspu calls the game makes: SpuInit, SpuStart, SpuQuit, SpuInitMalloc, SpuMalloc, SpuFree,
SpuSetTransferMode, SpuSetTransferStartAddr, SpuWrite, SpuIsTransferCompleted, SpuSetVoiceAttr
(masks 3, 0x10, 0x90; every other mask is implemented too), SpuGetVoiceAttr (mask 3),
SpuSetKey, SpuGetKeyStatus, SpuSetCommonAttr, SpuSetReverb, SpuSetReverbVoice,
SpuSetReverbModeParam (preset Studio C, depths), SpuReserveReverbWorkArea,
SpuClearReverbWorkArea. Call sites: game/src/sfx.c, sound.c, animover.c.
`test_libspu` shows that the register writes of the game's sound initialisation equal the retail
game's, write for write.

Decisions, each from the retail recording or the hardware description:
- The game never writes ADSR (all 24 voices keep 0/0 from SpuInit). With those values the
  hardware attacks in 3 samples, dips to the sustain level 0x800 in a few samples, climbs back
  at the fastest sustain rate and releases in 2 samples: that is the retail sound, reproduced.
- Voice volumes are 15-bit fixed values (a negative value inverts the phase); the register is
  `volume & 0x7FFF`. The hardware doubles it: 0x3FC0 is 0x7F80.
- `SpuSetCommonAttr` with mask 0 means "everything" in libspu. The game leaves the sweep modes
  and the external-input fields uninitialised on its stack; the retail console applied whatever
  was there (the first three calls gave the master left volume a never-ticking sweep, silent until
  the fourth call 5 s later; the external input is not connected). Here the volumes are fixed
  volumes and the external input is not touched: the sound is on from the first call.
- `SpuGetKeyStatus` is libspu's: the last key command and whether the envelope is above zero
  (1 on, 2 off but sounding, 3 on but silent: a sample that ran to its end, 0 off).
- Key commands accumulate: every voice named in a write is keyed. On the console the SPU sees a
  call's write a fraction of a sample later than the next call's; here a whole frame of game
  code runs between two samples, so a loop of single-voice SpuSetKey calls (SfxKillAllVoices)
  must not lose all but the last.
- `SpuInit` plays the silent block for 51 samples and releases it 50 samples later, as the
  console does, so the voices end up off with a zero envelope.
- Transfers are immediate, `SpuMalloc` is first-fit from 0x1010 with 8-byte alignment (the
  addresses retail's allocator gave the first samples are reproduced), the reverb work area
  (0x79020 up for Studio C) is kept out of the allocator once reserved. Only the presets Off and
  Studio C exist (the only ones used); `SpuSetReverbModeParam` refuses others.

## CD (disc.c, cdrom.c, libcd.c)

The disc is the user's .cue/.bin image (mapped read-only). The drive models Setloc, Play,
ReadN/ReadS, SeekL/P, Pause, Stop, Standby, Init, Mute/Demute, Setmode, Getparam, GetlocL/P,
GetTN/GetTD, GetID, ReadTOC with the first response after 1.5 ms, second responses (INT2)
delayed by the motion involved, and seeks of 4 ms to about 0.6 s by distance. Data reads deliver a
sector every 1/150 s (double speed) or 1/75 s; CD-DA plays at 75 sectors a second regardless.
`CdControl` with a seek or read command and a position sets the position first, as libcd does.

- The tracks keep the cue's INDEX 00/01: each audio track's 150-sector pregap belongs to it. The
  table of contents holds whole seconds with the lead-out in entry 0 (libcd's layout, which is
  why the game's `InitCD` shifts entries 1..11 by 0x4A and leaves track 12 alone).
- `GetlocP` reports the track by its INDEX 00, counts a pregap down to 00:00:00, and changes
  track on the sector boundary: a song that runs on into the next file's pregap shows the new track
  number on exactly the sample the drive reads it, and the game starts the next song, as retail.
  `test_music` plays the game's own `CdMusicPlay` sequence to that boundary.
- A data read preempts CD-DA (the head goes to track 1; the game's polling then starts music).
- CD audio goes to the SPU's CD input; the drive's volume matrix (CdMix) is applied before it,
  the SPU's CD volume and the master volume after (checked sample for sample in `test_music`).
- XA audio is not read by the game (only mode 5 and 0x80 are used) and is not implemented in the
  drive; the movie programs are replaced by the platform's player (platform/movie.c), which reads
  the movie files' sectors, video and XA sound, from the image itself.
- Not modelled: the drive's report mode, CdReadyCallback/CdSyncCallback (unused by the game),
  mechanical transients (GetlocP during a seek reports the old position until the seek ends).
  The retail recording shows GetlocP leading the audio by up to one sector; here it is exact.

## Pad (pad.c, pad_sdl.c)

The game reads two 8-byte buffers filled once per field: digital pad (0x4100) and NeGcon
(0x2300), buttons active low, bit layout as in `pad.h` (same values as the game's own pad.c).
Port 0 is always plugged in (the game pauses a race when no pad answers); port 1 only with a
second gamepad. Keyboard (by key position): in the menus Enter accepts (Cross and Start), Escape
goes back (Triangle), the arrows are the d-pad, Space is Circle, Left Alt Square, Q/E L1/R1,
Backspace Select, P Start; in a steered race S is thrust, Space fire, Left Alt drop weapon, A/D the
air brakes, F1 or V the view, the arrows steer, Escape, Enter or P pause. The race keys press the
buttons the player bound in the game's controller screen (`PsxPadRaceFrame` is told them every
race frame, and a few fields after the last call the keyboard is back in the menus; see
docs/CONTROLS.md). Gamepads: the standard layout (south/east/west/north =
Cross/Circle/Square/Triangle), triggers L2/R2, left stick = d-pad; hot-plug takes the first free
port. `PsxPadLastDevice` says whether the player last used the keyboard or which family of
gamepad (PlayStation, Xbox, Nintendo, other), for the glyphs.
`WIPEOUT_PAD_TYPE=negcon` selects the NeGcon (stick or arrows twist, triggers analogue).
Scripts (`padScript`): lines `field port BUTTONS` (names joined by `+`, `none`, or a hex mask;
`twist=`, `i=`, `ii=`, `l=`), applied from that video field on.

The pads' motors (the gamepads' vibration) shake as the picture does, and growl with the launch thrust
(`PortPadRumble` and `PortPadRumbleLevel` in platform/main.c, `PsxPadRumble` and `PsxPadRumbleLevel` in
pad_sdl.c). Both are in the game's own units, 2 a knock that is barely felt to 20 the full strength.

A jolt is a hit. It comes from the game's impact feedback: `SetShake`'s moments (a weapon hit, the quake
weapon, a menu landing, the knock of a ship that is losing control) and the hits that only sound —
`DamageCollide` on the player's ship, so a wall or another ship taps the pad (not the pit lane's repair,
which is no hit). A jolt that is still running keeps its strength and its time: a knock on a wall does
not end a rocket's.

A level is held: the game says it again every frame it wants the motors at it, and they stop a few fields
after it stops saying. The one held now is the start's launch thrust (`UpdatePlayerShipStart`), and it
buzzes **only inside the window the start boosts in** — 5/8 of the ship's top thrust to 6/8, the amounts
`UpdatePlayerShip` judges at the moment the race starts, which is the band between the third and second
last indicators of the HUD's thrust marker — rising across it as it nears the edge where the start stalls,
and silent everywhere else. That window is what an analogue throttle could be held in and a digital face
button cannot: the buzz is the feedback the analogue one gave. `WIPEOUT_RUMBLE_LOG=1` writes what the
motors are asked, one line a request, which is how this is checked without a pad (see
docs/port-notes/controller-vibration.md). When a jolt and a level both want the motors, the stronger runs.

The pads that plugged into the console had no motors; these are the port's own. The debug console's
"Controller vibration" turns them off and its "Strength" scales them (`vibration` and `vibration_strength`
of wipeoutxl.ini), and the tests' hidden runs never shake.

## Memory card (libcard.c)

A real 128 KiB card image per port (`memcard0.mcr`, `memcard1.mcr` in the save directory: the
usual raw format): header frame, 15 directory frames with allocation states and checksums, 15 8 KiB
blocks. Port 0 is created formatted on first use; port 1 exists only if its file does. The file
calls (create with a block count, read, write, close, rename, delete, format, firstfile/nextfile)
work on that structure and write the file when the game closes it. `_card_info`, `_card_load`,
`_card_clear`, `_card_write` post the BIOS events the game waits on (done, error, no card, new or
unformatted card) before returning. `_bu_init` marks cards as already seen, so the first
`_card_info` of a session reports a card that is there, not a new one.
The serial link port ("sio:") opens and answers nothing: the link handshake times out as with no cable.

## Not covered

Real gamepad hot-plug and audio device output were not exercised in this environment (no
speakers, tests are muted); the code paths use SDL3's documented calls. The audio device mode
was exercised with SDL's dummy driver (the clock follows it in real time).
