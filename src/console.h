/*
 * console.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <glib.h>
#include <SDL/SDL_events.h>
#include "utty.h"

struct console{
	gunichar	* screen_buffer_glyph;
	guint32	* screen_buffer_color;
	int		current_pos;
	gint		char_pixelsize; // char size in pixel, 8x16 font is 16
};

void init_console(int width,int height, int char_widh_pixel);
struct console * console_direct_get_vt(int index);
struct console  * console_get_forground_vt();

/*Called by UI code, not by CUSE code*/
void console_draw_vt( struct console * vt ,SDL_Surface * screen);
void console_notify_keypress(SDL_KeyboardEvent * key);

/**Called by CUSE code, not by UI code*/
void console_notify_write(struct console * vt, gunichar chars[], glong count);
void console_attach_reader(struct console * vt , struct io_request * request );
void console_notify_redraw();


#endif /* CONSOLE_H_ */
