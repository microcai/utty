/*
 * console.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <termio.h>
#include <glib.h>
#include <SDL/SDL_events.h>

#include "utty.h"


/*
 * struct for console screen write
 *
 * consw is a device for output
 */
struct consw{
	void (*sw_redraw)();
	void (*sw_setfgcolor)();
	void (*sw_setbgcolor)();
	void (*sw_putc)();

};


void init_console(int width,int height, int char_widh_pixel);
struct console * console_direct_get_vt(int index);
struct console  * console_get_forground_vt();
void	 console_vt_get_termios(struct console * vt,struct termios *);
void	 console_vt_set_termios(struct console * vt,struct termios *);
void	 console_get_window_size(glong * width, glong * height);
void	 console_vt_drain(struct console * vt);

/*Called by UI code, not by CUSE code*/
void console_draw_vt( struct console * vt ,SDL_Surface * screen);
void console_notify_keypress(SDL_KeyboardEvent * key);

/*debug only*/
#ifdef DEBUG
void DBG_console_vt_printbuffer(struct console * vt);
#else
#define DBG_console_vt_printbuffer(x)  do{;}while(0)
#endif

#endif /* CONSOLE_H_ */
