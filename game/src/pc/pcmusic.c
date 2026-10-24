/* The music of the PC CD-ROM version: the audio tracks after its data track (the PlayStation disc's are other songs),
   as the PC's menus and HUD name them (ayrace.c's cdTrackNames and cdTrackArtists; index 0 is the random play, the
   tracks from the second on the CD are the rest, as the console's list is). */
#include "pc.h"

static const char *const trackNames[13] = {
    "RANDOM", "BODY IN MOTION", "CANADA", "HAKAPIK MURDER", "PLASTICITY", "MESSIJ RECEIVED", "SURGEON",
    "TENATION", "MESSIJ XTND", "KINKONG", "KINKONG", "KINKONG", "KINKONG",
};

static const char *const trackArtists[13] = {
    "RANDOM", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE",
    "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "COLD STORAGE", "",
};

const char *PcCdTrackName(int track)
{
    return trackNames[track];
}

const char *PcCdTrackArtist(int track)
{
    return trackArtists[track];
}
