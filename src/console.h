/*
 * console.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <termios.h>
#include <glib.h>
#include <SDL/SDL_events.h>

struct console;

void init_console(int width,int height, int char_widh_pixel);
struct console * console_direct_get_vt(int index);
struct console  * console_get_forground_vt();
void	 console_vt_get_termios(struct console * vt,struct termios *);
void	 console_vt_set_termios(struct console * vt,struct termios *);

/*Called by UI code, not by CUSE code*/
void console_draw_vt( struct console * vt ,SDL_Surface * screen);
void console_notify_keypress(SDL_KeyboardEvent * key);

/**Called by CUSE code, not by UI code*/
void console_vt_notify_write(struct console * vt, gunichar chars[], glong count);
void console_vt_attach_reader(struct console * vt , struct io_request * request );


#endif /* CONSOLE_H_ */
