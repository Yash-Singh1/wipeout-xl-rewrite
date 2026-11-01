/* N64 structure motion in full track units, independent of drawing distance. */
#include "../../game/src/portorig.c"
#include <assert.h>
#include <string.h>
int portPal;
static int guest64 = 1;
int PortOriginalTrack(void) { return 1; }
int PortOriginalTrackIs64(void) { return guest64; }
static int rotorAngle, animationTick, animationCalls;
void PortOriginalAnimate(int tick) { animationTick = tick; animationCalls++; }
int rsin(int angle) { (void)angle; return 0; }
void SetSkeletonDirection(Skeleton *s, short x, short y, short z)
{ (void)s; assert(x == 0 && z == 0); rotorAngle = y; }
void SetSkeletonPosition(Skeleton *s, int x, int y, int z)
{ s->relative.t[0] = x; s->relative.t[1] = y; s->relative.t[2] = z; }
int main(void)
{
    Skeleton cartSkeleton = {0}, clawSkeleton = {0}, rotorSkeleton = {0};
    Object cart = {0}, claw = {0}, rotor = {0};
    cart.skeleton = &cartSkeleton; claw.skeleton = &clawSkeleton; rotor.skeleton = &rotorSkeleton;
    carts[0].object = &cart; carts[0].origin.vx = 100;
    claws[0].object = &claw; claws[0].random = 32767;
    rotors[0] = &rotor; rotorCount = 1;
    sceneryRandom = 0x12345678;
    MoveGuestScenery(1);
    assert(rotorAngle == 0);
    assert(cartSkeleton.relative.t[0] == 6100 && cartSkeleton.relative.t[2] == -3000);
    assert(clawSkeleton.relative.t[0] == -6000 && clawSkeleton.relative.t[1] == 480);
    MoveGuestScenery(46);
    assert(cartSkeleton.relative.t[0] == -900 && cartSkeleton.relative.t[2] == 600);
    assert(rotorAngle == 1440);
    MoveGuestScenery(91);
    assert(cartSkeleton.relative.t[0] == -7900 && cartSkeleton.relative.t[2] == 4200);
    MoveGuestScenery(150);
    assert(cartSkeleton.relative.t[0] == -7900);
    MoveGuestScenery(151);
    assert(cartSkeleton.relative.t[0] == 6100);
    /* The claw travels out and back in 120 updates at its longest excursion, then rests for 30. */
    claws[0].phase = 60;
    MoveGuestScenery(1);
    assert(clawSkeleton.relative.t[0] == 4000 && clawSkeleton.relative.t[1] == 540);
    assert(clawSkeleton.relative.t[2] == 0);
    claws[0].phase = 120;
    MoveGuestScenery(1);
    assert(clawSkeleton.relative.t[0] == -6000 && clawSkeleton.relative.t[2] == -160);
    for (int i = 0; i < 1000; i++) {
        int random = CraneRandom();
        assert(random >= 8192 && random <= 32767);
    }
    memset(carts, 0, sizeof(carts)); memset(claws, 0, sizeof(claws));
    sceneryFrame = animationCalls = 0; portPal = 1;
    for (int i = 0; i < 25; i++) PortOriginalSceneUpdate(0);
    assert(animationTick == 30 && animationCalls == 30);
    PortOriginalSceneUpdate(1);
    assert(animationCalls == 30 && sceneryFrame == 25);
    sceneryFrame = animationCalls = 0; portPal = 0;
    for (int i = 0; i < 30; i++) PortOriginalSceneUpdate(0);
    assert(animationTick == 30 && animationCalls == 30);
    puts("guest structure motion, coordinate scale, PAL cadence and pause: OK");
    return 0;
}
