

#include <SDL/SDL_events.h>
#include "vt.h"
#include "../utty.h"
#include "../console.h"

#define ESC		0x1B
#define CSI		0x9B

extern int screen_width, screen_rows;

static void screen_wrap(struct console * vt)
{
	if (vt->current_pos >= screen_width*screen_rows) // move out of screen, scroll one line
	{
		//chars
		memmove(vt->screen_buffer_glyph,vt->screen_buffer_glyph + screen_width, screen_width*(screen_rows-1)*sizeof(gunichar));
		memset(vt->screen_buffer_glyph + screen_width*(screen_rows-1) , 0 ,screen_width*sizeof(gunichar));

		//attributes
		memmove(vt->screen_buffer_attr,vt->screen_buffer_attr + screen_width, screen_width*(screen_rows-1)*sizeof(guint64));
		memset(vt->screen_buffer_attr + screen_width*(screen_rows-1) , 0 ,screen_width*sizeof(guint64));

		vt->current_pos -= screen_width;
	}
}


static void screen_nextline(struct console * vt)
{
	// move to new line, if possible, scroll the buffer
	vt->current_pos = ((vt->current_pos / screen_width) +1 )* screen_width;

	screen_wrap(vt);
}


static int	screen_can_double(struct console * vt) // enough space to write this double width char this line?
{
	return (screen_width - vt->current_pos % screen_width) > 1;
}

static void screen_write_char(struct console * vt,gunichar  c)
{
	// decode ESC escape sequence here

	if(c<8)return;

	if(vt->flags_esc ==1 ){
		if(c=='['){
			vt->flags_csi = 1;
			vt->flags_esc = 0;
			return ;
		}
	}

	if(vt->flags_csi==1)
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
		vt->screen_buffer_attr[vt->current_pos] = vt->cur_attr;
		vt->current_pos++;
		screen_wrap(vt);
	} while (i--);
}

static void screen_startline(struct console * vt)
{
	// move to line begin, if possible, scroll the buffer
	vt->current_pos = ((vt->current_pos / screen_width))* screen_width;
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
			if (vt->current_pos > 0){
				vt->screen_buffer_glyph[--vt->current_pos] = 0;
				vt->screen_buffer_attr[vt->current_pos] = 0;
			}
			continue;
		}

		screen_write_char(vt,chars[i]);
	}
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

void vte_attach_reader(struct console * vt , struct io_request * request ) //
{
	g_rec_mutex_lock(&vt->lock);

	g_queue_push_tail(&vt->readers,request);

	g_rec_mutex_unlock(&vt->lock);
}


void vte_notify_keypress(struct console * vt,SDL_KeyboardEvent * key)
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

	if(keycode == '\r')
		keycode = vt->termios.c_iflag & ICRNL ? '\n':'\r';

	if( keycode == '\b' && (lflag & (ICANON|ECHOE) ) ) // line edit, erase one
	{
		g_queue_pop_tail(& vt->keycode_buffer);

		should_queue = 0;
	}

	// enough to feed the read buffer
	if( !should_notify && (keycode!='\b' ) && request )
		if( request->size <= (g_queue_get_length(&vt->keycode_buffer) +1) )
			should_notify = 1;

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
