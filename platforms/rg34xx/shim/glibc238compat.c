/* glibc238compat.c - the prebuilt Dawn archive was compiled against glibc
 * 2.38+, whose headers redirect strtol & co. to __isoc23_* (C23 adds 0b
 * prefixes). The firmware has glibc 2.35, so map them back. */
#include <stdlib.h>

long __isoc23_strtol(const char* s, char** e, int b) { return strtol(s, e, b); }
long long __isoc23_strtoll(const char* s, char** e, int b) { return strtoll(s, e, b); }
unsigned long __isoc23_strtoul(const char* s, char** e, int b) { return strtoul(s, e, b); }
unsigned long long __isoc23_strtoull(const char* s, char** e, int b) { return strtoull(s, e, b); }
