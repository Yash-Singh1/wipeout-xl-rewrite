/* The disc's region (region.c): Wipeout XL (USA or Japan) or WipEout 2097 (Europe), whose regional pieces (title screen,
   menus' logo, movies...) the US program is given. The game's side is in game/include/port_platform.h. */
#ifndef PORT_REGION_H
#define PORT_REGION_H

#ifdef __cplusplus
extern "C" {
#endif

int PortRegionEurope(void); /* the disc is WipEout 2097 (Europe, SLES-00327) */
int PortRegionJapan(void);  /* the disc is Wipeout XL (Japan, SIPS-60010) */
/* Decides portPal, portEuropeDisc and portJapanDisc (game/include/version.h), once the disc is open and before the
   game starts. */
void PortRegionStart(void);
extern int portPal, portEuropeDisc, portJapanDisc, portEuropeCard;

#ifdef __cplusplus
}
#endif

#endif
