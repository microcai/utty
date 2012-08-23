#pragma once

#include <stdint.h>

#define lambda(return_type, body_and_args) \
  ({ return_type __fn__ body_and_args  __fn__; })

struct RGBA{
	uint8_t b;			/* blue */
	uint8_t g;			/* green */
	uint8_t r;			/* red */
	uint8_t a;			/* blue */
};

union color{
	struct RGBA s;
	uint32_t	u;
};

static union color __inline__ make_color3(uint8_t rgb[3]){
	return (union color){ .s.r = rgb[0] , .s.g = rgb[1], .s.b = rgb[2], };
}
