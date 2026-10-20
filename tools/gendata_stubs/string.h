/* Layout-only stand-in for the C library headers port.h includes (the 32-bit view of the data has no host libc). */
#ifndef GENDATA_STRING_H
#define GENDATA_STRING_H
#include <stddef.h>
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
char *strcat(char *, const char *);
char *strncat(char *, const char *, size_t);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
size_t strlen(const char *);
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strstr(const char *, const char *);
#endif
