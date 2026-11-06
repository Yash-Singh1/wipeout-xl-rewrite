/* Card browser prefixes and names share a real buffer, including under glibc fortification. */
#include "functions.h"
#include "card.h"
#include <assert.h>
#include <stddef.h>
int main(int argc, char **argv)
{
    const char *prefixes[] = {"WOXL ", "WO2097 "};
    const char *name = argc > 1 ? argv[1] : "TESTSAVE";
    assert(strlen(name) <= 8);
    for (int i=0;i<2;i++) {
        CardEntry entry = {0};
        entry.type = 4;
        memcpy(entry.label, prefixes[i], strlen(prefixes[i]) + 1);
        strcat(entry.label, name);
        assert(strlen(entry.label) == strlen(prefixes[i]) + strlen(name));
        assert(entry.type == 4);
    }
    /* Preserve the layout used by the game's browser, icons and directory loading. */
    assert(offsetof(CardEntry,type) == offsetof(CardEntry,label) + 32);
    puts("Card browser labels hold EU/US prefixes and eight-character save names: OK");
}
