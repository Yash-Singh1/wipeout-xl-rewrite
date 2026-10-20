#!/usr/bin/env python3
"""Write recorder scenarios (wipeout-xl-port/tools/trace/record.py) for an
autopilot arcade race on each track of the race menu."""
import json
from pathlib import Path

# race-menu track number: (name, class row, position in the row)
TRACKS = {1: ("Talon's Reach", 0, 0), 8: ('Sagarmatha', 0, 1), 13: ('Valparaiso', 1, 0),
          20: ('Phenitia Park', 1, 1), 2: ("Gare d'Europa", 2, 0), 17: ('Odessa Keys', 2, 1),
          6: ('Vostok Island', 3, 0), 7: ('Spilskinanke', 3, 1)}


def race(track):
    name, row, column = TRACKS[track]
    script, frame = [[0, 0], [120, 'START'], [125, 0]], 240

    def press(button):
        nonlocal frame
        script.extend([[frame, button], [frame + 5, 0]])
        frame += 120

    if row == 3:
        for button in ['TRIANGLE'] * 3 + ['CIRCLE'] * 3:   # the game's own Phantom cheat
            press('L1+R1+SELECT+' + button)
    press('UP')
    press('RIGHT'); press('RIGHT'); press('CROSS')
    for _ in range(row):
        press('DOWN')
    if column:
        press('RIGHT')
    press('CROSS')
    press('DOWN'); press('CROSS')
    script.append([frame, 'UP'])   # held: keeps the idle timer at zero; the drone code ignores it
    slug = name.lower().replace("'", '').replace(' ', '-')
    return dict(name=f'race-{track:02d}-{slug}', frames=16000, autopilot=True, skip_intro=True,
                script=script, coverage=[f'Arcade race on {name} under autopilot: start, laps, AI, weapons'])


if __name__ == '__main__':
    here = Path(__file__).parent
    for track in (8, 13, 20, 2, 17, 6):
        s = race(track)
        (here / f"{s['name']}.json").write_text(json.dumps(s, indent=1) + '\n')
        print(s['name'])
