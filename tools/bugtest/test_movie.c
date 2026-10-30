/* Executable names used by each disc region and the four endings. */
#include "../../platform/movie.c"
#include <assert.h>
int main(void)
{
    assert(MovieIndex("C:\\ntscanim.exe") == 0);
    assert(MovieIndex("C:\\palanim.exe") == 0);
    assert(MovieIndex("PALANIM.EXE") == 0);
    assert(MovieIndex("c:\\XTRO1.EXE") == 1);
    assert(MovieIndex("xtro2.exe") == 2);
    assert(MovieIndex("xtro3.exe") == 3);
    assert(MovieIndex("xtro4.exe") == 4);
    assert(MovieIndex("missing.exe") == -1);
    puts("movie programs: OK");
    return 0;
}
