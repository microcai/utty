/*
 ============================================================================
 Name        : cusetty.c
 Author      : microcai
 Version     :
 Copyright   : MIT
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <SDL/SDL.h>
#include <SDL/SDL_video.h>
#include <glib.h>
#include <sys/errno.h>
#include <ft2build.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <poll.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "kernel.h"
#include "utty.h"
#include "font.h"
#include "console.h"
#include "vt.h"

static void utty_redraw()
{

	SDL_Surface * surface = SDL_GetVideoSurface();

	console_draw_vt(console_get_forground_vt(),surface);

	SDL_Flip(surface);
	//SDL_FreeSurface(surface);
}

static void utty_run()
{
	SDL_Event event;

	while (SDL_WaitEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			SDL_Quit();
			return ;
		case SDL_USEREVENT:
			utty_redraw();
			break;
		case SDL_KEYDOWN:
			// process keyborad event
			console_notify_keypress(&event.key);
		}
	}
}

static void utty_init_video()
{
	SDL_Surface * screen;

	setenv("SDL_VIDEO_CENTERED","1",1);

	SDL_Init(SDL_INIT_EVENTTHREAD|SDL_INIT_VIDEO);

	SDL_EnableUNICODE(1);

	int SDLCALL EventFilter(const SDL_Event *event)
	{
		switch (event->type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_QUIT:
		case SDL_USEREVENT:
			return 1;
		}
		return 0;
	}

	// we only want keyborad event
	SDL_SetEventFilter(EventFilter);

	char buf[128]={0};
	SDL_VideoDriverName(buf,sizeof buf);
	printf("video driver %s ， suported modes: ",buf);

	SDL_Rect ** modes = SDL_ListModes(NULL, SDL_SWSURFACE);

	if (modes && modes != (void*)-1)
	{
		SDL_Rect * m = *modes;

		screen = SDL_SetVideoMode(m->w, m->h, 32 , SDL_SWSURFACE);

		init_console(m->w,m->h,16);

		printf("pixels: %dx%d , text  %dx%d\n",m->w,m->h,m->w/8,m->h/16);

	}
	else if(!modes)
	{
		printf("no modes\n");
		exit(1);
	}else
	{
		screen = SDL_SetVideoMode(800, 600, 32, SDL_SWSURFACE);

		init_console(800, 600,16);

		printf("any, so i set %dx%d  aka text mode  %dx%d\n\n",800, 600,800/8,600/16);
	}

	SDL_Surface * bmp = SDL_LoadBMP("test.bmp");

	if(SDL_BlitSurface(bmp,0,screen,0))
		fprintf(stderr,"error SDL!!!\n");
	SDL_Flip(screen);	SDL_FreeSurface(bmp);

}

static void utty_init()
{
	// load font
	init_font();

	/*
	 * init display and keyborad.
	 *
	 * It will then call init_console() to build an console on top of it and mutiplex it as VT
	 */

	utty_init_video();

	//  create tty device , one thread per tty
	init_vt();
}

void utty_force_expose()
{
	SDL_Event event={0};

	event.type = SDL_USEREVENT;

	SDL_PushEvent(&event);
}

int main(int argc, char **argv)
{
	// init things
	utty_init();
	// UI thread main loop
	utty_run();
	return 0;
}

