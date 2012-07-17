/*
 * console.c
 *
 *
 * Mainly the code to manipulate framebuffer and deal with keyborad with SDL
 */


#include <SDL/SDL.h>
#include "utty.h"
#include "font.h"
#include "console.h"

#define MAX_VT	12


struct console{
	gunichar	* screen_buffer_glyph;
	guint32	* screen_buffer_color;
	int		current_pos;
	gint		char_pixelsize; // char size in pixel, 8x16 font is 16

	struct		termios termios;

};


static int fg_vt;


/* ALL THE VT HAVE THE SAME SCREEN SIZE*/
int screen_width, screen_rows;

struct console  vts[MAX_VT]={0};

void console_draw_vt( struct console * vt ,SDL_Surface * screen )
{
	/*
	 * redraw current screen
	 */
		int i;
		SDL_Rect		pos;

		SDL_FillRect(screen,0,0);

	for (i = 0; i < screen_width * screen_rows; i++)
	{
		if (g_unichar_isprint(vt->screen_buffer_glyph[i]))
		{
			pos.x = (i % screen_width) * vt->char_pixelsize / 2;
			pos.y = (i / screen_width) * vt->char_pixelsize;

			SDL_Surface * source = font_render_unicode(
					vt->screen_buffer_glyph[i], vt->char_pixelsize);

			SDL_BlitSurface(source, 0, screen, &pos);
			SDL_FreeSurface(source);
		}
		i += g_unichar_iswide(vt->screen_buffer_glyph[i]);
	}
}

/*
 * allocate VTs
 */
static void console_allocate()
{
	for(int i=0;i<MAX_VT;i++)
	{
		vts[i].screen_buffer_glyph = g_new0(gunichar,screen_width*screen_rows);
		vts[i].screen_buffer_color = g_new0(guint32,screen_width*screen_rows);
		vts[i].char_pixelsize = 16;
	}
}

static void screen_wrap(struct console * vt)
{
	if(vt->current_pos >= screen_width*screen_rows) // move out of screen, scroll one line
	{
		memmove(vt->screen_buffer_glyph,vt->screen_buffer_glyph + screen_width, screen_width*(screen_rows-1)*sizeof(gunichar));
		memset(vt->screen_buffer_glyph + screen_width*(screen_rows-1) , 0 ,screen_width*sizeof(gunichar));


		vt->current_pos -= screen_width;
	}
}

static void screen_nextline(struct console * vt)
{
	// move to new line, if possible, scroll the buffer
	vt->current_pos = ((vt->current_pos / screen_width) +1 )* screen_width;

	screen_wrap(vt);
}

static void screen_write_char(struct console * vt,gunichar  c)
{
	vt->screen_buffer_glyph[vt->current_pos] = c;
	vt->current_pos++;

	screen_wrap(vt);
}

static void screen_write(struct console * vt,gunichar * chars, glong written)
{
	for(int i=0; i< written; i++)
	{
		switch(chars[i])
		{
		case '\n':
			screen_nextline(vt);
			continue;
		}

		screen_write_char(vt,chars[i]);
		if(	g_unichar_iswide(chars[i]))
			screen_write_char(vt,chars[i]);
	}
}


void init_console(int width,int height,int char_widh_pixel)
{
	screen_width = width / (char_widh_pixel /2) ;
	screen_rows = height/char_widh_pixel;

	console_allocate();
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


void	console_notify_write(struct console * vt,gunichar chars[],glong count)
{
	screen_write(vt,chars,count);
	if(vt == console_get_forground_vt()){
		utty_force_expose();
	}
}

void	console_notify_keypress(SDL_KeyboardEvent * key)
{
	glong written;
	glong readed;

	struct console * vt = console_get_forground_vt();

	gunichar * chars = g_utf16_to_ucs4(&key->keysym.unicode,1,&readed,&written,0);

	console_notify_write(vt,chars,written);

	g_free(chars);


#if 0
	struct io_request * request = g_queue_pop_head(&readers);

	if(!request)
	{
		return ;
	}

	glong written;
	glong readed;


	char * keystok = g_utf16_to_utf8(&event->key.keysym.unicode,1,&readed,&written,0);

	if(request->size < written){
		fuse_reply_err(request->req,EAGAIN);
	}else{
		fuse_reply_buf(request->req,keystok,written);
	}
	g_free(keystok);
	g_free(request);
#endif
}


void console_attach_reader(struct console * vt , struct io_request * request ) //
{

}
