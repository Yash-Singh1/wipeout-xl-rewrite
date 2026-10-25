/* The PC version's race HUD and its screens over a race (hud.c, ayrace.c), drawn over the race every disc plays
   (pc.h). The PC drew them from its HUD2 pages (texture codes 32-34, PC pages 22-24) in 640x480 and in front of the
   race by their depth: text at 0.1, the speedometer's bars at 0.29-0.3 behind its panels at 0.28, the pause menu's
   box at 0.06-0.07 with its text at 0.05, the audio settings' in front of that. What they show is the PlayStation
   code's state: its ships (ShipData), its race (ConfigData) and its pause (PauseMenu); the pause menu's choices and
   input are the PlayStation's (options.c), drawn the PC's way. */
#include <stdio.h>

#include "pc.h"
#include "../../include/ayrace.h"
#include "../../include/hiscores.h"
#include "../../../platform/controls.h"

/* combatAttr: the ship has begun a lap (UpdateRaceHud) */
#define LAP_BEGUN 0x4000
/* keysDown, the pad's buttons held: its left and right */
#define HELD_RIGHT 0x20
#define HELD_LEFT 0x80

extern int ownShip;
extern int hudFrame;
extern int checkTimer;
extern int shipDamage[];
extern int savedLapTimes[];
extern int raceType;
extern int cardNum;
extern int trackNameScroll;
extern int trackNameOffset;
extern int cdMusicTrack;
extern int cdMusicCount;
extern u_char cdMusicIndex;
extern u_char perfectLapCount;
extern u_char rearLockCycle;
extern u_char raceEndState;
extern short continueAnimFrame;
extern unsigned short keysDown;
extern int ambientVoice1;
extern char msgControllerRemoved[];

/* The weapon icons in the HUD's third page, for each weapon (ship->weapon - 1): where each is drawn, in the console's
   320x240, and its place and size in the page. */
static const short weaponX[13] = {153, 153, 150, 153, 153, 153, 153, 153, 153, 153, 164, 153, 153};
static const short weaponY[13] = {25, 28, 25, 28, 28, 28, 28, 28, 28, 28, 25, 28, 28};
static const u_char weaponU[13] = {64, 64, 178, 0, 0, 0, 64, 128, 192, 0, 128, 128, 192};
static const u_char weaponV[13] = {128, 64, 128, 0, 128, 0, 0, 0, 0, 64, 128, 64, 64};
static const u_char weaponWidth[13] = {64, 64, 74, 4, 64, 64, 64, 64, 63, 64, 18, 64, 63};
static const u_char weaponHeight[13] = {70, 64, 74, 4, 64, 64, 64, 64, 64, 64, 36, 64, 64};

/* The keys the PC shows in its button prompts (DoButton), in its first HUD page: ESC, ENTER, the left and right
   arrows. */
/* The PC's keys in its HUD page (hud.c's buttonIcons), by PcKey. */
static const struct {
    u_char u, v, width, height;
} keyIcons[] = {{0, 0, 30, 30}, {158, 162, 30, 30}, {158, 194, 30, 30}, {160, 226, 32, 16}, {194, 226, 32, 16}};
/* ... and the console's icon (DoButton's: 0 circle, 1 cross, 3 triangle, 4/5 arrows) the device's glyph replaces */
static const u_char consoleIcons[] = {0, 3, 1, 4, 5};

/* The row of the lap times' heading for each class (as many laps as the class races). */
static const short lapTimeRows[4] = {180, 170, 160, 150};

static POLY_FT4 panelQuads[4]; /* the speedometer's two panels, the weapon's two */
static int hudThrust;          /* the thrust bar's length, kept while the autopilot flies the ship */
static signed char audioRow;   /* the pause's audio settings: 0 the CD track, 1 the music's volume, 2 the effects' */
static u_char volumeRepeat;    /* frames a volume key has been held since the volume last changed */

void PcInitPanelQuads(void)
{
    PcSetTextQuad(&panelQuads[0], 205, 161, 0, 0, 224, 66, 33, 0);
    PcSetTextQuad(&panelQuads[1], 205, 196, 0, 68, 224, 52, 33, 0);
    PcSetTextQuad(&panelQuads[2], 145, 20, 0, 204, 96, 48, 34, 0);
    PcSetTextQuad(&panelQuads[3], 145, 44, 128, 204, 96, 48, 34, 0);
}

void PcRaceHud(ShipData *ships, int choice, ConfigData *config, int position)
{
    ShipData *player = &ships[ownShip];
    int laps = classLaps[config->shipClass], row = lapTimeRows[config->shipClass];
    int racingCount = 0, speed, lap, track, i;
    short autopilot;
    char text[96];

    /* what the PC's UpdateRaceHud updates for them (the PlayStation's does as it draws them) */
    if (perfectLapCount > 0) {
        perfectLapCount--;
    }
    if (trackNameScroll > -480) {
        trackNameOffset = trackNameScroll;
        if (trackNameScroll < 0 && trackNameScroll > -200) {
            trackNameOffset = 0;
        }
        if (trackNameOffset <= -200) {
            trackNameOffset += 200;
        }
        if (trackNameOffset < 0) {
            trackNameOffset = -trackNameOffset;
        }
        if (pauseState.selection == -1) {
            trackNameScroll -= 6;
        }
        cdMusicTrack = config->cdTrack == 1 ? cdMusicTable[cdMusicIndex] : config->cdTrack;
    }

    if (player->combatAttr & LAP_BEGUN) {
        /* the lap's time, and those of the laps before */
        lap = player->lap == 0 ? 1 : player->lap;
        PcAddTime(player->lapTimes[(player->lap < laps + 1 ? lap : laps) - 1], 16, 205, 16, 0, 1);
        PcPrint("LAP TIMES", 22, row, 0, 0);
        for (i = 1; i < laps; i++) {
            sprintf(text, "%d", i);
            PcPrint(text, 16, row + i * 10 + 2, 0, 0);
        }
        for (i = 1; i < player->lap && i < laps; i++) {
            PcAddTime(player->lapTimes[i - 1], 24, row + i * 10, 8, 1, 1);
        }
    }
    autopilot = player->autopilotCount;
    if (autopilot != 0) {
        PcAddText("AUTOPILOT", 120, 108, 8, 1);
        if (autopilot < 30) {
            PcAddText("DISENGAGING", 110, 120, 8, 1);
        } else {
            PcAddNumber(autopilot / 30, 160, 128, 8, 0);
        }
        speed = player->moveDist * 34;
    } else {
        speed = player->speed;
    }
    PcAddText("LAP", 258, 50, 8, 1);
    PcAddJustifyNumber(player->lap > laps ? laps : player->lap, 263, 60, 16, 0);
    PcAddText("OF", 263, 68, 8, 1);
    PcAddNumber(laps, 285, 60, 16, 0);
    if (player->rearTarget != -1 && rearLockCycle < 13 && player->weapon == 2 && choice == -1) {
        PcPrintCX("REAR LOCK", 168, 70, 0);
    }
    if (perfectLapCount > 0) {
        PcCentreText("PERFECT LAP", 120, 160, 8, 0);
    }
    if (trackNameScroll > -480 && cdMusicCount != 0) {
        track = cdMusicTrack != 0 ? cdMusicTrack - 1 : 0;
        snprintf(text, sizeof(text), "CD TRACK: %d   %s [%s]", cdMusicTrack, GetCdTrackName(track),
                 GetCdTrackArtist(track));
        PcPrint(text, 16, (trackNameOffset >> 2) + 227, 0, 0);
    }
    for (i = 0; i < 15; i++) {
        if (!(ships[i].combatAttr & DESTROYED)) {
            racingCount++;
        }
    }
    PcAddText("CHECK", 20, 20, 8, 1);
    PcAddText("POS", 258, 20, 8, 1);
    PcAddJustifyNumber(position, 263, 30, 16, 0);
    PcAddText("OF", 263, 38, 8, 1);
    PcAddNumber(racingCount, 285, 30, 16, 0);
    if (player->lap >= 0) {
        if (checkTimer > 0) {
            PcAddTime(checkTimer, 20, 30, 16, 0, 1);
        } else if (PcBlink()) {
            PcAddTime(checkTimer, 20, 30, 16, 1, 1);
        }
    }
    if (autopilot == 0 && shipDamage[ownShip] < 100 && !(player->attr & DIRECTION) &&
        player->nearTrkSect->junction.index == -1 && player->nearTrkSect->nextSection.ptr->junction.index == -1) {
        PcCentreText("WRONG WAY", 50, 110, 8, 1);
    }
    PcAddJustifyNumber(speed / 95, 250, 168, 16, 0);
}

/* A bar of the speedometer from its page (row v on): `top` long along its top edge, `bottom` along its bottom one, its
   end slanted as the panel's is. */
static void SpeedoBar(int x, int y, int v, int top, int bottom, float depth)
{
    POLY_FT4 bar;
    int width = top > bottom ? top : bottom;

    PcSetTextQuad(&bar, (short)x, (short)y, 0, (u_char)v, (u_char)width, 44, 33, 0);
    bar.u1 = (u_char)top;
    bar.x1 = (short)(x * 2 + top);
    bar.u3 = (u_char)bottom;
    bar.x3 = (short)(x * 2 + bottom);
    DrawScaledTexturedQuad(depth, &bar, 0);
}

void PcFillSpeedo(ShipData *ships, ConfigData *config, int choice)
{
    static const int speedDividers[4] = {125, 167, 209, 250};
    ShipData *player = &ships[ownShip];
    int energy = 100 - shipDamage[ownShip], speed, length, bottom;

    speed = CameraUpdate == UpdateCameraAttractInternal ? player->speed * 7 : player->speed;
    /* the energy: blinking when low, but while paused */
    length = energy * 192 / 100;
    if (length >= 190) {
        length = 190;
    }
    if ((choice != -1 || energy > 40 || hudFrame % 8 <= 4) && length > 0) {
        bottom = length - 22 < 0 ? 0 : length - 22;
        SpeedoBar(208, 198, 210, bottom + 2, length + 2, 0.3f);
    }
    /* the speed, three times as long while the autopilot flies the ship */
    length = speed / speedDividers[config->shipClass];
    if (player->autopilotCount != 0) {
        length *= 3;
    }
    if (length >= 192) {
        length = 192;
    }
    if (length > 0) {
        SpeedoBar(218, 170, 122, length, length - 22 < 0 ? 0 : length - 22, 0.29f);
    }
    /* the thrust, as it was when the autopilot took over */
    if (player->autopilotCount == 0) {
        hudThrust = player->thrust * 192 / player->maxThrust;
    }
    length = hudThrust >= 192 ? 192 : hudThrust;
    if (length > 0) {
        SpeedoBar(218, 170, 166, length, length - 22 < 0 ? 0 : length - 22, 0.3f);
    }
    DrawScaledTexturedQuad(0.28f, &panelQuads[0], 0);
    DrawScaledTexturedQuad(0.28f, &panelQuads[1], 0);
}

void PcDrawWeaponIcons(ShipData *ship)
{
    POLY_FT4 icon;
    int weapon = ship->weapon - 1;

    /* the panel and the icon added to what is under them, as the PC's Direct3D does where it can */
    DrawScaledTexturedQuad(0.3f, &panelQuads[2], 1);
    DrawScaledTexturedQuad(0.3f, &panelQuads[3], 1);
    if (weapon >= 0 && weapon < 13) {
        PcSetTextQuad(&icon, weaponX[weapon], weaponY[weapon], weaponU[weapon], weaponV[weapon], weaponWidth[weapon],
                      weaponHeight[weapon], 34, 0);
        DrawScaledTexturedQuad(0.29f, &icon, 1);
    }
}

void PcDisplayRaceInfo(ConfigData *config, HiScoreData *score, ShipData *ships)
{
    static const char *const classNames[4] = {"VECTOR", "VENOM", "RAPIER", "PHANTOM"};
    static const char *const medalNames[3] = {" GOLD", " SILVER", " BRONZE"};
    int eliminated = config->shipClass == 3 || ownShip >= 12 ? 15 : 12, total = 0, laps, i;
    char text[40];

    for (i = 0; i < 15; i++) {
        if (!(ships[i].combatAttr & DESTROYED)) {
            eliminated--;
        }
    }
    if (eliminated < 0) {
        eliminated = 0;
    }
    if (raceEndState == 2) {
        /* out of time, or destroyed */
        if (ships[ownShip].combatAttr & 0x2D) {
            PcCentreText("BAD LUCK", 15, 24, 8, 1);
            PcCentreText("YOU RAN OUT OF TIME", 15, 60, 8, 1);
        } else if (shipTargetIndex[ownShip] < 8) {
            PcCentreText("BAD LUCK", 15, 24, 8, 1);
            PcCentreText("YOUR SHIP WAS DESTROYED", 45, 60, 8, 1);
        }
        laps = ships[ownShip].lap - 1;
        score->totalTime = 0;
        if (laps > 0) {
            PcAddText("RACE STATISTICS", 15, 100, 8, 1);
        }
    } else {
        if (raceType != 1) {
            if (finishPosition >= 1 && finishPosition <= 3) {
                PcCentreText("CONGRATULATIONS", 15, 24, 8, 1);
                sprintf(text, "%s%s AWARDED", classNames[config->shipClass & 3], medalNames[finishPosition - 1]);
                PcCentreText(text, 15, 48, 8, 0);
            } else {
                PcCentreText("BAD LUCK", 15, 24, 8, 1);
                PcCentreText("NO MEDAL AWARDED", 15, 48, 8, 0);
            }
            PcAddText("RACE POSITION", 15, 74, 8, 1);
            PcAddNumber(finishPosition, 155, 74, 8, 0);
            if (eliminated != 0) {
                PcAddText("ELIMINATIONS", 15, 196, 8, 1);
                PcAddNumber(eliminated, 144, 196, 8, 0);
            }
        }
        laps = classLaps[config->shipClass];
        PcAddText("RACE STATISTICS", 15, 100, 8, 1);
        score->totalTime = 0;
    }
    for (i = 0; i < laps; i++) {
        PcAddText("LAP", 25, i * 12 + 115, 8, 1);
        PcAddNumber(i + 1, 65, i * 12 + 115, 8, 1);
        PcAddTime(savedLapTimes[i], 85, i * 12 + 115, 8, 0, 1);
        score->totalTime += savedLapTimes[i];
        total += savedLapTimes[i];
    }
    if (raceEndState != 2) {
        if ((raceType == 0 || raceType == 4) && finishPosition <= 3) {
            /* Port: where the console's projection shows it (the PC's own, 80, 0, 150, is for its projection) */
            DrawTeamShips(70, 0, 180, cardNum, 0, teamShip[0]);
        }
        PcAddText("RACE TIME", 15, 180, 8, 1);
        PcAddTime(raceType == 0 || raceType == 4 || raceType == 1 ? score->totalTime : total, 128, 180, 8, 0, 1);
    }
    PcDoButton(127, 210, PC_KEY_ENTER, "CONTINUE");
}

void PcDisplayContinues(int x, int y)
{
    POLY_FT4 pilot;
    int count = continuesLeft, size, left, top, i;

    /* the pilot's head for each continue left, the last shrinking away while one is used */
    x = x * 2 - ((count * 64 + (count - 1) * 20) >> 1);
    y *= 2;
    for (i = 0; i < count; i++, x += 84) {
        size = i == count - 1 ? (32 - continueAnimFrame) * 2 : 64;
        left = x - size / 2;
        top = y - size / 2;
        PcSetTextQuad(&pilot, 0, 0, 190, 160, 64, 64, 32, 0);
        pilot.x0 = pilot.x2 = (short)left;
        pilot.x1 = pilot.x3 = (short)(left + size);
        pilot.y0 = pilot.y1 = (short)top;
        pilot.y2 = pilot.y3 = (short)(top + size);
        DrawScaledTexturedQuad(0.3f, &pilot, 0);
    }
}

int PcKeyOfIcon(int icon)
{
    return icon == 3 ? PC_KEY_ESCAPE : icon == 4 ? PC_KEY_LEFT : icon == 5 ? PC_KEY_RIGHT : PC_KEY_ENTER;
}

void PcDoButton(int x, int y, int key, const char *text)
{
    POLY_FT4 button;
    int shift;

    /* Port: a gamepad's button where the PC draws its key (with the keyboard, or before the player has touched
       anything, the PC's own key) */
    SetPolyFT4(&button);
    button.r0 = button.g0 = button.b0 = 128;
    button.tpage = 0;
    shift = PortButtonGlyph(&button, consoleIcons[key], x, y);
    if (button.tpage != 0 && !PortShowsKeys()) {
        PcDrawPacket(0.1f, &button, sizeof(button));
    } else {
        shift = 0;
        PcSetTextQuad(&button, (short)x, (short)y, keyIcons[key].u, keyIcons[key].v, keyIcons[key].width,
                      keyIcons[key].height, 32, 0);
        DrawScaledTexturedQuad(0.1f, &button, 0);
    }
    PcPrint(text, key == PC_KEY_ESCAPE ? x - 25 : x + 18 + shift, y + 6, 0, 0);
}

/* A box of the pause menu, its corners in 640x480 (DrawMenuBox): its fill and its border, each a texel of the HUD's
   page. */
void PcDrawMenuBox(int x0, int y0, int x1, int y1, float fillDepth, float borderDepth)
{
    POLY_FT4 box;

    PcSetTextQuad(&box, 60, 80, 109, 56, 1, 1, 32, 0);
    box.x0 = box.x2 = (short)(x0 + 2);
    box.x1 = box.x3 = (short)(x1 - 2);
    box.y0 = box.y1 = (short)(y0 + 2);
    box.y2 = box.y3 = (short)(y1 - 2);
    DrawScaledTexturedQuad(fillDepth, &box, 0);
    PcSetTextQuad(&box, 60, 80, 109, 62, 1, 1, 32, 0);
    box.x0 = box.x2 = (short)x0;
    box.x1 = box.x3 = (short)x1;
    box.y0 = box.y1 = (short)y0;
    box.y2 = box.y3 = (short)y1;
    DrawScaledTexturedQuad(borderDepth, &box, 0);
}

/* A volume's bar in its frame (UpdateMusicBar, UpdateSfxBar): its colours from u in the page's last row. */
static void VolumeBar(int volume, int y, int u)
{
    POLY_FT4 bar;
    int level = volume * 65 / 100;

    if (level > 0) {
        PcSetTextQuad(&bar, 140, (short)y, (u_char)u, 240, (u_char)level, 8, 32, 0);
        bar.x1 = bar.x3 = (short)(280 + level * 2);
        bar.y2 = bar.y3 = (short)(y * 2 + 16);
        DrawScaledTexturedQuad(0.01f, &bar, 0);
    }
    PcSetTextQuad(&bar, 138, (short)(y - 2), 0, 74, 138, 24, 32, 0);
    DrawScaledTexturedQuad(0.02f, &bar, 0);
}

/* The menu (DrawPauseMenu, DrawPauseOptions): its options in a box, and the audio settings in another over it. */
static void DrawPauseMenu(struct PauseMenu *pause, ConfigData *game, int audio)
{
    static const char *const options[4] = {"CONTINUE", "RESTART", "RETURN TO MENU", "AUDIO CONFIG"};
    int i;

    PcCentred(1);
    pcTextDepth = 0.05f;
    PcDrawMenuBox(140, 120, 500, 320, 0.06f, 0.07f);
    if (audio) {
        PcDrawMenuBox(150, 300, 490, 440, 0.03f, 0.04f);
        pcTextDepth = 0.01f;
        if (audioRow != 0 || PcBlink()) {
            PcCentreText("CD TRACK", 0, 155, 8, 0);
        }
        PcPrintCX(GetCdTrackName(game->cdTrack ? game->cdTrack - 1 : 0), 160, 167, 0);
        if (audioRow != 1 || PcBlink()) {
            PcAddText("MUSIC", 85, 185, 8, 0);
        }
        if (audioRow != 2 || PcBlink()) {
            PcAddText("SFX", 85, 200, 8, 0);
        }
        VolumeBar(game->otherMusicVolume, 185, 0);
        VolumeBar(game->otherEffectVolume, 200, 82);
        pcTextDepth = 0.05f;
    }
    if (PcBlink()) {
        /* Port: the console's word for a pad pulled out, in place of PAUSED */
        PcCentreText(padType == 0 ? msgControllerRemoved : "PAUSED", 107, 70, 8, 1);
    }
    if (pause->stage == 2 || pause->stage == 0) {
        for (i = 0; i < 4; i++) {
            PcCentreText(options[i], 0, 95 + i * 12, 8, pause->selection == i);
        }
    }
    pcTextDepth = 0.1f;
    PcCentred(0);
}

/* A volume as the left and right keys held change it: by 5 every fifth frame. */
static short HeldVolume(short volume)
{
    if (keysDown & (HELD_LEFT | HELD_RIGHT)) {
        if (++volumeRepeat == 5) {
            volume += keysDown & HELD_LEFT ? -5 : 5;
            volumeRepeat = 0;
        }
    }
    return volume < 0 ? 0 : volume > 100 ? 100 : volume;
}

/* Another CD track chosen: track 1 is the tracks in turn, the next of them. */
static void ChangeMusic(struct PauseMenu *pause, ConfigData *game)
{
    PlayNote(8, 255, 15, 15, 256);
    trackNameScroll = 480;
    if (game->cdTrack != 1) {
        CdMusicPlay(game->cdTrack, 0);
    } else {
        cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1);
        cdMusicTrack = cdMusicTable[cdMusicIndex];
        CdMusicPlay(cdMusicTrack, 0);
    }
    pause->musicPaused = 1;
}

/* The audio settings' rows: the CD track, the music's volume, the effects' (heard as the shield's hum meanwhile). */
static void AudioSettings(struct PauseMenu *pause, ConfigData *game)
{
    if (GetMenuKey(0)) {
        if (--audioRow < 0) {
            audioRow = 0;
        }
    } else if (GetMenuKey(1)) {
        if (++audioRow > 2) {
            audioRow = 2;
        }
    }
    if (audioRow == 0) {
        if (GetMenuKey(2)) {
            if (--game->cdTrack <= 0) {
                game->cdTrack = cdMusicCount;
            }
            ChangeMusic(pause, game);
        } else if (GetMenuKey(3)) {
            if (++game->cdTrack > cdMusicCount) {
                game->cdTrack = 1;
            }
            ChangeMusic(pause, game);
        }
    } else if (audioRow == 1) {
        game->otherMusicVolume = HeldVolume(game->otherMusicVolume);
        game->musicVolume = game->otherMusicVolume;
        SetCdVolume(game->musicVolume * 255 / 100);
    } else {
        game->otherEffectVolume = HeldVolume(game->otherEffectVolume);
        game->effectVolume = game->otherEffectVolume;
        SetSoundVolume(game->effectVolume * 255 / 100);
        SetVolume(ambientVoice1, 255);
        SfxEffectLRPan(ambientVoice1, 15);
        SfxEffectFBPan(ambientVoice1, 15);
    }
    UpdateNotes(1);
}

void PcInGameReset(struct PauseMenu *pause, ConfigData *game)
{
    int audio;

    SfxSetReverbDepth(0);
    MuteControlledSound();
    if ((pause->stage == 2 || pause->stage == 0) && pause->selection == 3) {
        /* the audio settings: opened and closed by the accept button, the music playing while they are open */
        if (!game->paused) {
            if (GetMenuKey(5)) {
                audioRow = 0;
                CdMusicPlay(cdMusicTrack, 0);
                pause->musicPaused = 1;
                game->paused = 1;
            }
        } else if (GetMenuKey(5)) {
            pause->musicPaused = 0;
            CdMusicStop();
            SetSoundVolume(game->effectVolume * 255 / 100);
            game->paused = 0;
        } else {
            AudioSettings(pause, game);
        }
    }
    audio = pause->selection == 3 && game->paused;
    if (!audio || audioRow != 2) {
        SetVolume(ambientVoice1, 0);
    }
    DrawPauseMenu(pause, game, audio);
    pause->frameCount++;
}

void PcInGameConfirm(u_char *confirmed, int mode, u_char *selection, int *frameCount)
{
    if (GetMenuKey(2) || GetMenuKey(3)) {
        *selection ^= 1;
        PlayNote(8, 255, 15, 15, 256);
    }
    PcCentred(1);
    pcTextDepth = 0.05f;
    PcDrawMenuBox(120, 170, 520, 280, 0.06f, 0.07f);
    PcCentreText("ARE YOU SURE YOU", 77, 95, 8, 0);
    PcCentreText(mode == 1 ? "WANT TO RESTART ?" : "WANT TO QUIT ?", mode == 1 ? 90 : 105, 105, 8, 0);
    PcAddText("YES", 75, 120, 8, *selection != 0 && PcBlink());
    PcAddText("NO", 220, 120, 8, *selection == 0 && PcBlink());
    pcTextDepth = 0.1f;
    PcCentred(0);
    if (GetMenuKey(5) || GetMenuKey(6)) {
        *confirmed = *selection;
    }
    (*frameCount)++;
}
