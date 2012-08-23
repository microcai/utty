/*
 * sdlscreen.c
 *
 *  Created on: 2012-8-20
 *      Author: cai
 */

#include "common.h"

#include <SDL/SDL.h>
#include <SDL/SDL_video.h>

#include "console.h"
#include "font/font.h"

static int screen_sdl_init(struct screenop * op)
{
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	op->private = SDL_SetVideoMode(640, 400, 32, SDL_SWSURFACE);
	return op->private==0;
}

static void getwindowsize(struct screenop * op ,int *width,int *height)
{
	*width=640;*height=400;
}

static 	void bitblt(struct screenop * screen,int x ,int y , struct fontbmp*fontbmp)
{
	SDL_Rect drect = {x,y,fontbmp->width,fontbmp->height };
	SDL_Surface * src = SDL_CreateRGBSurfaceFrom(fontbmp->data,fontbmp->width,fontbmp->height,32,
			fontbmp->stride,0xFF0000,0xFF00,0xFF,0);

	SDL_BlitSurface(src,NULL,(SDL_Surface*)(screen->private),&drect);

	SDL_FreeSurface(src);
}

static 	void flip(struct screenop * screen)
{
	SDL_Flip(screen->private);
}

static struct screenop sdl={
		.init = screen_sdl_init,
		.getwindowsize = getwindowsize,
		.bitblt = bitblt,
		.flip = flip,
};

struct screenop * screen_sdl_new()
{
	return &sdl;
}
