/* The Sega Saturn's Wipeout XL / WipEout 2097 disc in place of the PlayStation's (saturn.c). The game's side
   (its file layer's PortSaturnFileInfo and PortSaturnRead) is in game/include/port_platform.h. */
#ifndef PORT_SATURN_H
#define PORT_SATURN_H

#ifdef __cplusplus
extern "C" {
#endif

struct Disc;

/* Whether a disc image is the Saturn's Wipeout XL (Japan) or WipEout 2097 (Europe). */
int PortSaturnDiscIs(const struct Disc *disc);
/* Whether the disc in use is; its name. */
int PortSaturnDisc(void);
const char *PortSaturnTitle(void);

#ifdef __cplusplus
}
#endif

#endif
