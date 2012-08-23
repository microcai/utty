#pragma once

#include <stdint.h>

int iswide(uint32_t c);
int utf8_ucs(uint32_t * out, int size,const char * utf8in);
int isprintable(uint32_t c);
