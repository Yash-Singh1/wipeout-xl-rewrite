/* Port: what the game is doing, for the frame recorder's info.json (platform/psx/framecap.c): the track and, for
   a guest, the game it is from; the class, the race type, the player's team, lap and section. */
#include <stdio.h>

#include "functions.h"
#include "portorig.h"

extern TrackData *track;
extern ShipData *shipIndex;
extern int ownShip;
extern int raceType;
extern ConfigData *gameData;

static uint64_t raceFrame; /* the game frame the race was last drawn in, + 1 (0: not since the start) */

void PortInfoRaceDrawn(void)
{
    raceFrame = PortGameFrames() + 1;
}

static const char *RaceTypeName(void)
{
    switch (raceType) {
    case 0: return "arcade";
    case 1: return "time trial";
    case 2: return "arcade link";
    case 3: return "one on one";
    case 4: return "challenge";
    default: return "other";
    }
}

static const char *TeamName(int ship)
{
    static const char *const teams[] = {"AG Systems", "Auricom", "Qirex", "Feisar", "Piranha"};

    return ship >= 0 && ship < 15 ? teams[ship / 3] : "?";
}

/* The description, as the members of a JSON object (no braces); its length. */
int PortGameInfo(char *out, int size)
{
    static const char *const classes[] = {"Vector", "Venom", "Rapier", "Phantom"};
    int guest = PortOriginalTrack(), racing = raceFrame && PortGameFrames() + 1 - raceFrame < 4, n;
    const char *name = gameData ? PortRaceTrackName(gameData->actTrackNum) : NULL;

    n = snprintf(out, (size_t)size,
                 "\"game frame\": %llu,\n  \"data\": \"%s\",\n  \"screen\": \"%s\",\n  \"race type\": \"%s\",\n",
                 (unsigned long long)PortGameFrames(), PortPcData() ? "PC" : "PlayStation",
                 racing ? "race" : "menus", RaceTypeName());
    if (gameData && n < size) {
        n += snprintf(out + n, (size_t)(size - n),
                      "  \"track\": \"%s\",\n  \"track number\": %d,\n  \"from\": \"%s\",\n  \"class\": \"%s\",\n",
                      name ? name : "?", gameData->actTrackNum,
                      guest < 0 ? "Wipeout XL" : PortOriginalTrackIs64() ? "Wipeout 64" : "Wipeout",
                      gameData->shipClass >= 0 && gameData->shipClass < 4 ? classes[gameData->shipClass] : "?");
    }
    if (racing && shipIndex && ownShip >= 0 && ownShip < 15 && track && n < size) {
        const ShipData *ship = &shipIndex[ownShip];

        n += snprintf(out + n, (size_t)(size - n),
                      "  \"team\": \"%s\",\n  \"ship\": %d,\n  \"lap\": %d,\n  \"section\": %d,\n"
                      "  \"section in the track file\": %d,\n  \"speed\": %d,\n",
                      TeamName(ownShip), ownShip, ship->lap, ship->nearSecNo,
                      ship->nearTrkSect ? (int)(ship->nearTrkSect - track->sections) : -1, ship->speed);
    }
    return n < size ? n : size - 1;
}
