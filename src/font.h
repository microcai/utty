/*
 * font.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#ifndef FONT_H_
#define FONT_H_

#include <glib.h>
#include <SDL/SDL_video.h>

void init_font(int argc, char **argv);

SDL_Surface * font_render_unicode(gunichar uc,int pixelsize,guint64 attribute);

#endif /* FONT_H_ */
