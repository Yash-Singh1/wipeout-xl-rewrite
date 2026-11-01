/* One background release check per launch; no downloads or installation. */
#ifndef PORT_UPDATES_H
#define PORT_UPDATES_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PORT_RELEASES "https://github.com/Yash-Singh1/wipeout-xl-rewrite/releases"
void PortUpdatesStart(void);
void PortUpdatesPoll(void);
void PortUpdatesShutdown(void);
const char *PortUpdateAvailable(void); /* pending notification, NULL after dismissal */
void PortUpdateDismiss(void);
void PortUpdateOpenRelease(void);
int PortUpdateNewer(const char *current, const char *available);
int PortUpdateTagFromUrl(const char *url, char *tag, size_t size);
/* Platform HTTPS transport. Returns the latest release's URL, or zero on failure. */
int PortUpdateFetch(char *url, size_t size);
#ifdef __cplusplus
}
#endif
#endif
