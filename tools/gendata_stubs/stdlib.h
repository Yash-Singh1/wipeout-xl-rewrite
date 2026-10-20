#ifndef GENDATA_STDLIB_H
#define GENDATA_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void free(void *);
int abs(int);
int rand(void);
void srand(unsigned int);
int atoi(const char *);
void exit(int);
void abort(void);
#endif
