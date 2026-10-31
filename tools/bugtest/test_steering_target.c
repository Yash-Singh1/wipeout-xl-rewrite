/* The actual target used by UpdatePlayerShip, including the existing NeGcon calibration path. */
#include "../../game/src/control.c"
#include <assert.h>
BrakeModes brakeModes[1] = {{{0, 6, 12, 18}}};
unsigned char turnDivisor = 255, twist = 128;
int currScreen = 1;
int main(void)
{
    ShipData ship = {0};
    ControlInput control = {0};
    short small, medium, large;
    ship.maxHeading = 2560;
    control.head = 1;
    control.steering = 128; assert(SteeringTarget(&ship, &control) == 0);
    control.steering = 140; small = SteeringTarget(&ship, &control);
    control.steering = 170; medium = SteeringTarget(&ship, &control);
    control.steering = 220; large = SteeringTarget(&ship, &control);
    assert(small < 0 && medium < small && large < medium);
    control.steering = 0; assert(SteeringTarget(&ship, &control) > ship.maxHeading);
    control.steering = 255; assert(SteeringTarget(&ship, &control) < -ship.maxHeading);
    /* Ordinary gamepad steering ignores a previously saved NeGcon centre. */
    twist = 20; turnDivisor = 30;
    control.steering = 140; assert(SteeringTarget(&ship, &control) == small);
    control.head = 3; /* modern stick ghosts use the same target */
    control.steering = 140; assert(SteeringTarget(&ship, &control) == small);
    twist = 128; turnDivisor = 255;
    control.head = 2;
    control.steering = 128; assert(SteeringTarget(&ship, &control) == 0);
    control.steering = 160;
    assert(SteeringTarget(&ship, &control) == (-26 * (2560 + 400) * 2) / 255);
    puts("steering targets are proportional; NeGcon calibration remains active: OK");
    return 0;
}
