#ifndef _LIBC_STDLIB_H_
#define _LIBC_STDLIB_H_

#include <stddef.h>
#include <stdint.h>

int rand(void);

void srand(unsigned int seed);

int64_t satoi(char *str);

#endif