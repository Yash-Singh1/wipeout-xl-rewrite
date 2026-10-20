/* Shared by the I/O tests: locating the user's disc, starting the I/O layer in fixed-step mode
   (no audio device, no clock but the game's fields), and the platform hooks the library calls. */
#ifndef IOTEST_TESTENV_H
#define IOTEST_TESTENV_H

#include <stdint.h>

/* The .cue of the disc image (WIPEOUT_XL_DISC, else ../wipeout-xl next to the repository), or
   NULL if there is none; tests that need the disc skip themselves. */
const char *test_disc_path(void);
/* The extracted file tree of the same disc (WIPEOUT_XL_ROOT), or NULL. */
const char *test_root_dir(void);
/* Starts PsxIo on the disc with a fresh temporary save directory. */
int test_io_start(double cdSpeed, const char *padScript);
void test_io_stop(void);
const char *test_save_dir(void);
/* Video fields passed so far. */
uint64_t test_fields(void);

#endif
