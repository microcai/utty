/*
 * font.c
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#include <ft2build.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <poll.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "font.h"


static int sdldeeps[] =
{ 32, 1, 8, 2, 4, 8, 8 };

static FT_Library ftlib;

static char * ttffile;

static void loadfont()
{
	int ret;

	FcPattern * fcpat =  FcPatternCreate();
	FcConfigSubstitute(0,fcpat,FcMatchPattern);

	FcDefaultSubstitute(fcpat);

	ret = FcPatternAddString(fcpat,FC_FAMILY,"Monospace");

	FcFontSet * fcfs = FcFontSetCreate ();

	FcResult result;
	FcFontSetAdd(fcfs,FcFontMatch(0,fcpat,&result));
	FcPatternDestroy(fcpat);

	FcFontSetPrint(fcfs);

	FcPattern * matchedpattern = FcFontSetMatch(0,&fcfs,1,fcpat,&result);

	FcChar8 * ttf;

	ret = FcPatternGetString(matchedpattern,FC_FILE,0,&ttf);

	ttffile = g_strdup(ttf);

	FcPatternDestroy(matchedpattern);

}


void init_font()
{

	FT_Init_FreeType(&ftlib);

	/*don't enable HOME sice we are runing as system instance*/
	FcConfigEnableHome(FALSE);
	FcInit();

	loadfont();
}


FT_Face matchbest(gunichar uc)
{
	static FT_Face ret;

	if(!ret){

		FT_New_Face(ftlib,ttffile,0,&ret);

	}


	// cache the last time used FT_face




	return ret;
}


SDL_Surface * font_render_unicode(gunichar uc, int pixelsize)
{
	FT_Face ftface = matchbest(uc);

	FT_Set_Pixel_Sizes(ftface,0,pixelsize);


	FT_Load_Char(ftface, uc, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);

	FT_Bitmap bmp = ftface->glyph->bitmap;

	SDL_Surface * surface = SDL_CreateRGBSurfaceFrom(bmp.buffer, bmp.width,
			bmp.rows, sdldeeps[bmp.pixel_mode], bmp.pitch, 0xff, 0xff, 0xff, 0);

	return surface;
}

