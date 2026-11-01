/* A fast ship must pitch down towards continuous guest road above the crest. */
#include "../../game/src/dynam.c"
#include <assert.h>
#include <string.h>

int portPal, raceType, droidFlyin;
static int guest;
int PortOriginalLooks(void) { return guest; }
TrackData data;
TrackData *track = &data;
GhostReplay recordBuffer[1];
int PlaneLineCollision(VECTOR p, VECTOR s, SVector n)
{
    int dot = (p.vx-s.vx)*n.vx + (p.vy-s.vy)*n.vy + (p.vz-s.vz)*n.vz;
    return dot / (((-n.vx*n.vx)>>12) + ((-n.vy*n.vy)>>12) + ((-n.vz*n.vz)>>12));
}
void Ship2Track(ShipData *s, Face *f, Object **o) { (void)s; (void)f; (void)o; }
void Ship2TrackBase(ShipData *s, Face *f, int a) { (void)s; (void)f; assert(a > 75); }
int CheckPolygonCollision(Face *f, VECTOR p, int a, int count)
{ (void)f; (void)p; (void)a; (void)count; return 1; }
void UpdateRescueDroid(ShipData *s, Object **o, int i, WeaponData *w, ControlInput *c)
{ (void)s; (void)o; (void)i; (void)w; (void)c; assert(0); }

static ShipData Run(int original, int normalZ, int height, int flags, int attr, int disabled)
{
    static TrackSection sections[3];
    static Face faces[2];
    static VECTOR vertices[1];
    ShipData ship = {0};
    memset(sections, 0, sizeof(sections));
    memset(faces, 0, sizeof(faces));
    for (int i = 0; i < 3; i++) {
        sections[i].prevSection.ptr = &sections[(i+2)%3];
        sections[i].nextSection.ptr = &sections[(i+1)%3];
        sections[i].centre.vz = i*1000;
        sections[i].flags = flags;
    }
    faces[0].flags = faces[1].flags = TRACK_BASE;
    faces[0].normal = faces[1].normal = (SVector){0,normalZ ? -2048 : -4096,normalZ};
    data.faces = faces; data.vertices = vertices;
    ship.nearTrkSect = sections;
    ship.attr = DIRECTION | LSIDE | attr;
    ship.ppivot.vy = -height;
    ship.unitVecNose.vz = 4096;
    ship.vpivot.vz = 30000;
    ship.thrust = ship.maxThrust = 1400;
    ship.mass = 110; ship.resistance = 155; ship.skid = 2;
    ship.rotationCorrectionDisabled = disabled;
    guest = original;
    ShipTrkReaction(&ship, NULL);
    return ship;
}

int main(void)
{
    for (portPal = 0; portPal <= 1; portPal++) {
        ShipData xl = Run(0,3547,2000,0,0,0);
        ShipData original = Run(1,3547,2000,0,0,0);
        assert(original.vpitch < xl.vpitch);
        /* Only pitch differs. Thrust, translation, and speed retain XL physics. */
        assert(!memcmp(&original.ppivot,&xl.ppivot,sizeof(VECTOR)));
        assert(!memcmp(&original.vpivot,&xl.vpivot,sizeof(VECTOR)));
        assert(original.thrust == xl.thrust && original.maxThrust == xl.maxThrust && original.mass == xl.mass);
        const int cases[][5] = {
            {0,200,0,0,0},       /* Already close to the road. */
            {0,2000,0,0,0},         /* Flat road must still lower the nose. */
            {-3547,2000,0,0,0},     /* Uphill road. */
            {3547,2000,TRACK_JUMP,0,0},
            {3547,2000,0,FLYING,0},
            {3547,2000,0,0,1},      /* Weapons can disable pitch correction. */
        };
        for (unsigned i = 0; i < sizeof(cases)/sizeof(*cases); i++) {
            const int *c = cases[i];
            xl = Run(0,c[0],c[1],c[2],c[3],c[4]);
            original = Run(1,c[0],c[1],c[2],c[3],c[4]);
            assert(original.pitch == xl.pitch && original.vpitch == xl.vpitch);
            assert(!memcmp(&original.ppivot,&xl.ppivot,sizeof(VECTOR)));
        }
    }
    puts("guest descent pitch, unchanged thrust, flat road, jumps and airborne handling: OK");
    return 0;
}
