/* Team labels must identify the pilot group used to draw and select the ship. */
#include "../../game/src/portlobby.c"
#include <assert.h>
#include <string.h>

int main(void)
{
    int team, pilot;
    const char *names[] = {"AG SYSTEMS", "AURICOM", "QIREX", "FEISAR", "PIRANHA"};
    const int ships[] = {9, 0, 3, 6, 12};

    for (team = 0; team < 5; team++) {
        assert(TeamShip(team) == ships[team]);
        assert(!strcmp(TeamName(team), names[ships[team] / PILOTS_PER_TEAM]));
        for (pilot = 0; pilot < PILOTS_PER_TEAM; pilot++) {
            assert(TeamOfShip(ships[team] + pilot, 5) == team);
        }
    }
    assert(!strcmp(BackName(1), "ESC"));
    assert(!strcmp(BackName(2), "TRIANGLE"));
    assert(!strcmp(BackName(3), "Y"));
    assert(!strcmp(BackName(4), "X"));
    puts("lobby labels match all five pilot groups; back prompts match the north button: OK");
    return 0;
}
