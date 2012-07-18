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

struct color
{
	guchar red, green, blue;
};

struct _attribute
{
	struct color fg;
	struct color bg;
	guint16 attr;
} attribute;

union attribute
{
	struct _attribute s;
	guint64 qword;
};

#define ATTR_BOLD		1
#define	ATTR_ALANT	2

struct console{
	gunichar	* screen_buffer_glyph;
	guint64	* screen_buffer_attr; // 24bit for color, 8 bit for attrib
	gint		current_pos;
	gint		char_pixelsize; // char size in pixel, 8x16 font is 16

	guint64	cur_attr; // current fg bg color, and attribute

	GRecMutex		lock;
	GQueue readers ;

	GQueue keycode_buffer;

	pid_t control_pid;

	pid_t session_id;

	struct termios termios;

	/*
	 * flags for DECODE ESCAPE SEQUENCE
	 */


	guint64	flags_esc:1;
	guint64	flags_csi:1;

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

/**Called by CUSE code, not by UI code*/
void console_vt_notify_write(struct console * vt, gunichar chars[], glong count);
void console_vt_attach_reader(struct console * vt , struct io_request * request );
void console_vt_get_window_size(struct console * vt ,struct winsize*);

#endif /* CONSOLE_H_ */
