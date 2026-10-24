/* The CHD reader by itself, so its tests need neither the game nor psxio's sound and input. */
#include <stdio.h>
#include "chd.h"

int main(int argc, char **argv)
{
    char message[2048];

    if (argc != 4) {
        fprintf(stderr, "usage: chd_decode image.chd disc.bin disc.cue\n");
        return 2;
    }
    if (!PortChdDecode(argv[1], argv[2], argv[3], NULL, message, sizeof(message))) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}
