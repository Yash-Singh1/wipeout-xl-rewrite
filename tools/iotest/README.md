# I/O layer tests

Unit tests for the sound, disc, controller and memory card layers in `platform/psx`.

    cmake -S tools/iotest -B build-iotest && cmake --build build-iotest
    WIPEOUT_NO_GAMEPAD=1 ctest --test-dir build-iotest --output-on-failure

They run headless and muted (no audio device, no window). Tests that need the PS1 disc find it at
`../wipeout-xl` next to the repository or at `$WIPEOUT_XL_DISC` (the .cue) and
`$WIPEOUT_XL_ROOT` (the extracted file tree); without it they skip. `PSX_SPU_ORACLE` (a directory
holding a console emulator's recording of the retail game, see test_spu_oracle.c) enables the
two comparisons against the real console.

- `test_adpcm`: decodes every block of every sample in the disc's `SAMPLES.WAD` and compares with an independent floating-point decoder, plus a hand-computed block.
- `test_adsr`: envelope curves for linear and exponential increase and decrease at 25 rates against the psx-spx formulas, the rate that never ticks, and the attack/decay/sustain/release phases of a voice.
- `test_voice`: through the libspu calls: pitch moves the sample position by pitch/4096 a sample, loops follow the ADPCM flags and set ENDX, one-shots stop and report key status 3, release, 24 simultaneous voices, one-at-a-time key commands (SfxKillAllVoices), 300 reuses of a voice, and the exact output level of a constant sample.
- `test_reverb`: Studio C impulse response: first echo, smooth decay to silence, stays in its work area, silent when off or at zero depth.
- `test_spu_oracle`: replays a console emulator's recording of the retail game (337,375 register writes, 251 s) and compares all 24 voices, the reverb return and the master mix sample for sample; needs `PSX_SPU_ORACLE`.
- `test_libspu`: the register writes libspu makes for the game's sound initialisation equal the retail game's (209 writes; needs `PSX_SPU_ORACLE`), sound RAM allocation addresses, voice attribute masks and read-back, common attributes with a garbage-filled struct, key registers.
- `test_cdda`: CD-DA samples equal the `.bin` files sector for sector (read with stdio, not the module's mapping), start sector, GetlocP values (pregap countdown, track change at INDEX 00), pause and resume, the volume matrix, the lead-out.
- `test_music`: the game's own music sequence (InitCD with its 0x4A-sector shift, CdMusicPlay, GetlocP polling, CdMusicStop) through libcd, the SPU CD input and master volume to the output sample; the track change is seen within a frame; a data read interrupts the music.
- `test_cd`: every non-empty file of the extracted disc (436 files, 74 MB) read the way iso9660.c reads them (SeekL, WaitEvent, CdRead, spin on CdReadSync) and compared byte for byte; command responses; a stale completion event; the drive's read timing.
- `test_pad`: the pad buffers the game reads (digital 0x4100, NeGcon 0x2300, active-low buttons), scripted input by field, the keyboard layout through SDL key events, focus loss, NeGcon on the keyboard.
- `test_card`: memory card events and BIOS file calls over a real 128 KiB card image: create, write, read, firstfile/nextfile, rename, delete, multi-block files, a full card, format, a second session reading the file back, directory checksums.
- `test_api`: BIOS events (polled and handler, critical sections), the drive's error event, exact field-to-sample timing, identical audio from two runs, the real-time mode with an audio device (SDL dummy driver) and a lock stress with a rendering thread.
- `test_soak`: 3000 video fields of random but valid calls (voices, key commands, pitch and volume changes, reverb depth, CD music, data reads while music plays) with the clock and mix checked; run it under the sanitizers.
