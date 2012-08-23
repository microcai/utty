/*
 * console.c
 *
 *
 * Mainly the code to manipulate framebuffer and deal with keyborad with SDL
 */

#include <termio.h>
#include <SDL/SDL.h>
#include "utty.h"
#include "font/font.h"
#include "console.h"
#include "vte/vt.h"

#define MAX_VT	12




static int fg_vt;


/* ALL THE VT HAVE THE SAME SCREEN SIZE*/
int screen_width, screen_rows;

struct console vts[MAX_VT] =
{ 0 };

void console_draw_vt(struct console * vt, SDL_Surface * screen)
{
	/*
	 * redraw current screen
	 */
	int i;
	SDL_Rect pos;

	SDL_FillRect(screen, 0, 0);

	for (i = 0; i < screen_width * screen_rows; i++)
	{
		if (g_unichar_isprint(vt->screen_buffer_glyph[i]))
		{
			pos.x = (i % screen_width) * vt->char_pixelsize / 2;
			pos.y = (i / screen_width) * vt->char_pixelsize;

			SDL_Surface * source = font_render_unicode(
					vt->screen_buffer_glyph[i],vt->char_pixelsize,vt->screen_buffer_attr[i]);

			SDL_BlitSurface(source, 0, screen, &pos);
			SDL_FreeSurface(source);
		}
		i += g_unichar_iswide(vt->screen_buffer_glyph[i]);
	}
}


static void init_default_termios(struct termios * tp)
{
	ioctl(0,TCGETS,tp);
	tp->c_lflag |= ECHOCTL|ECHO;
}

void init_console(int width,int height,int char_widh_pixel)
{
	screen_width = width / (char_widh_pixel /2) ;
	screen_rows = height/char_widh_pixel;

	union attribute attr;

	/*
	 *  initial color is white
	 */
	attr.qword = 0;

	attr.s.fg.red =30;
	attr.s.fg.green = 255;
	attr.s.fg.blue = 30;

	for(int i=0;i<MAX_VT;i++)
	{
		vts[i].screen_buffer_glyph = g_new0(gunichar,screen_width*screen_rows);
		vts[i].screen_buffer_attr = g_new0(guint64,screen_width*screen_rows);
		vts[i].char_pixelsize = char_widh_pixel;

		g_queue_init(&vts[i].readers);
		g_queue_init(&vts[i].keycode_buffer);

		init_default_termios(&vts[i].termios);

		vts[i].cur_attr = attr.qword;
	}
}


struct console  * console_get_forground_vt()
{
	return & vts[fg_vt];
}

struct console * console_direct_get_vt(int index)
{
	if(index < MAX_VT)
		return & vts[index];
	return NULL;
}

void vte_get_window_size(struct console * vt ,struct winsize * size)
{
	size->ws_col = screen_width;
	size->ws_row = screen_rows;
	size->ws_xpixel = vt->char_pixelsize/2;
	size->ws_ypixel = vt->char_pixelsize;
}

void console_get_window_size(glong * width, glong * height)
{
	*width = screen_width;
	*height = screen_rows;
}
/*
void console_vt_drain(struct console * vt)
{
	vt->current_pos = 0;

	memset(vt->screen_buffer_glyph,0,screen_rows*screen_width*sizeof(gunichar));
	memset(vt->screen_buffer_attr,0,screen_rows*screen_width*sizeof(guint64));

	if(vt == console_get_forground_vt()){
		utty_force_expose();
	}
}*/

void	console_notify_keypress(SDL_KeyboardEvent * key)
{
	int notify = 0;
	struct console * vt = console_get_forground_vt();

	switch (key->keysym.sym) {
		case 8 ... SDLK_DELETE: // ascii keys
			notify = 1;
			break;
		default:
			break;
	}
	if(notify)
		vte_notify_keypress(vt,key);
}

#ifdef DEBUG

void DBG_console_vt_printbuffer(struct console * vt)
{
	int x,y;
	gunichar code ;

	printf("************begin screen dump***************\n");


	// print buffers
	for (y = 0; y < screen_rows; y++)
	{
		for(x=0;x<screen_width;x++)
		{
			code = vt->screen_buffer_glyph[y*screen_width + x];

			//print the code
			printf("0x%04x",code);
			if(g_unichar_isprint(code))
				printf("(%c) ",code);
			else
				printf("(*) ");
		}
		printf("\n",code);
	}
	printf("*************end screen dump****************\n");

}
#endif
