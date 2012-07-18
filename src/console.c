/*
 * console.c
 *
 *
 * Mainly the code to manipulate framebuffer and deal with keyborad with SDL
 */

#include <termio.h>
#include <SDL/SDL.h>
#include "utty.h"
#include "font.h"
#include "console.h"
#include "vt.h"

#define MAX_VT	12

#define ESC		0x1B
#define CSI		0x9B


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
					vt->screen_buffer_glyph[i], vt->char_pixelsize);

			SDL_BlitSurface(source, 0, screen, &pos);
			SDL_FreeSurface(source);
		}
		i += g_unichar_iswide(vt->screen_buffer_glyph[i]);
	}
}

static void screen_wrap(struct console * vt)
{
	if (vt->current_pos >= screen_width*screen_rows) // move out of screen, scroll one line
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

static void screen_startline(struct console * vt)
{
	// move to line begin, if possible, scroll the buffer
	vt->current_pos = ((vt->current_pos / screen_width))* screen_width;
}


static int	screen_can_double(struct console * vt) // enough space to write this double width char this line?
{
	return (screen_width - vt->current_pos % screen_width) > 1;
}

static void screen_write_char(struct console * vt,gunichar  c)
{
	// decode ESC escape sequence here

	if(vt->flags_esc ){
		if(c=='['){
			vt->flags_csi = 1;
			vt->flags_esc = 0;
			return ;
		}
	}

	if(vt->flags_csi)
	{
		if(c == 'm')
		{
			vt->flags_csi = 0;
			return;
		}
		return ;
	}

	switch(c)
	{
	case ESC:
		vt->flags_esc = 1;
		return ;
	case CSI:
		vt->flags_csi = 1;
		return ;
	}

	int i = g_unichar_iswide(c);

	if( i && (!screen_can_double(vt)) )
	{
		screen_nextline(vt);
	}




	do
	{
		vt->screen_buffer_glyph[vt->current_pos] = c;
		vt->current_pos++;
		screen_wrap(vt);
	} while (i--);
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
		case	'\r':
			screen_startline(vt);
			continue;
		case '\b':
			if (vt->current_pos > 0)
				vt->screen_buffer_glyph[--vt->current_pos] = 0;
			continue;
		}

		screen_write_char(vt,chars[i]);
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

	for(int i=0;i<MAX_VT;i++)
	{
		vts[i].screen_buffer_glyph = g_new0(gunichar,screen_width*screen_rows);
		vts[i].screen_buffer_color = g_new0(guint32,screen_width*screen_rows);
		vts[i].char_pixelsize = char_widh_pixel;

		g_queue_init(&vts[i].readers);
		g_queue_init(&vts[i].keycode_buffer);

		init_default_termios(&vts[i].termios);

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


void console_vt_attach_reader(struct console * vt , struct io_request * request ) //
{
	g_rec_mutex_lock(&vt->lock);

	g_queue_push_tail(&vt->readers,request);

	g_rec_mutex_unlock(&vt->lock);
}


void	console_vt_notify_write(struct console * vt,gunichar chars[],glong count)
{
	g_rec_mutex_lock(&vt->lock);

	screen_write(vt,chars,count);

	g_rec_mutex_unlock(&vt->lock);

	if(vt == console_get_forground_vt()){
		utty_force_expose();
	}
}

void	console_vt_notify_keypress(struct console * vt,SDL_KeyboardEvent * key)
//;; gunichar chars[],glong count)
{
	gunichar keycode = key->keysym.unicode;

	g_rec_mutex_lock(&vt->lock);


	struct io_request * request = g_queue_peek_head(&vt->readers);

	cc_t lflag = vt->termios.c_lflag;

	int should_notify = 0;
	int should_queue = 1;

	if( ! (lflag & ICANON)  ) // not line buffered
	{
		should_notify = 1;
	}

	if( (lflag & ICANON) && (keycode == '\r') ) // line buffered and enter pressed
	{
		should_notify = 1;
	}

	// enough to feed the read buffer
	if( !should_notify && request )
		if( request->size <= (g_queue_get_length(&vt->keycode_buffer) +1) )
			should_notify = 1;

	if(keycode == '\r')
		keycode = vt->termios.c_iflag & ICRNL ? '\n':'\r';

	if( keycode == '\b' && (lflag & ICANON) ) // line edit, erase one
	{
		g_queue_pop_tail(& vt->keycode_buffer);

		should_queue = 0;
	}

	// is line buffered ?
	if( lflag & ECHO ){ // echo
		// write to screen buffer
		console_vt_notify_write(vt,&keycode,1);
	}


	if(should_notify)
	{
		should_queue = 0;
		// notify the reader
		request = g_queue_pop_head(&vt->readers);

		if(request)
		{
			glong		written;
			gchar *   for_client;

			glong		len = g_queue_get_length(&vt->keycode_buffer) +1;
			//  line up keycodes
			gunichar * keycodes = g_new0( gunichar, len);
			gunichar * _keycodes = keycodes;

			void g_cb_lineup(gpointer data, gpointer user_data)
			{
				gunichar ** pkeycodes = user_data;
				gunichar * keycodes = *pkeycodes;

				*keycodes = GPOINTER_TO_UINT(data);

				pkeycodes[0]++;
			}

			g_queue_foreach(&vt->keycode_buffer,g_cb_lineup,&_keycodes);

			*_keycodes = keycode ;//

			for_client = g_ucs4_to_utf8(keycodes,len,NULL,&written,NULL);

			g_free(keycodes);

			// to see if it realy feed your stomach
			tty_notify_read(request,for_client,written);

			g_free(for_client);
		}
		g_queue_clear(&vt->keycode_buffer);
	}

	if(should_queue)
		g_queue_push_tail(&vt->keycode_buffer, GUINT_TO_POINTER(key->keysym.unicode));

	g_rec_mutex_unlock(&vt->lock);
}


void	console_notify_keypress(SDL_KeyboardEvent * key)
{
	struct console * vt = console_get_forground_vt();
	console_vt_notify_keypress(vt,key);
}


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
