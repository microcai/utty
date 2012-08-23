
#include "console/console.h"
#include "font/font.h"
#include "vte/vte.h"

static void console_redraw(struct console* con)
{
	vte_lock(con->vte);

	for(int i = 0 ; i < con->vte->rows; i++)
		con->font->draw_line(con->font,con->screen,con->vte,i);
	vte_unlock(con->vte);

	con->screen->flip(con->screen);
}

static void con_redraw(struct console* con)
{
	con->need_redraw = 1;
	con->kbd->interrupt(con->kbd);
}

static struct console con = {
		.con_redraw = con_redraw,
};


void console_run()
{
	struct input_event event;

	for (;;)
	{
		switch(con.kbd->wait(con.kbd, &event))
		{
		case input_event_interrupt:
			// do some thing
			break;
		case input_event_expose:
			con.need_redraw = 1;
			//redraw;
			break;
		case input_event_key:
			//forward key to VTE
			con.vte->process_key(con.vte,&event);
			break;
		case input_event_terminate:
			return ;
		}
		if(con.need_redraw){
			con.need_redraw =0;

			console_redraw(&con);
		}
	}
	return;
}


int console_attatch_vte(struct vte * vte)
{
	int w,h,font_w,font_h;

	vte->con = &con, con.vte = vte;

	con.screen->getwindowsize(con.screen,&w,&h);

	font_h = con.font->getfontsize(con.font);
	font_w = font_h/2;

	vte->resize(vte,w/=font_w,  h/=font_h);

	vte->clear(vte);
	return 0;
}
int console_init(struct screenop * screen, struct kbdop * kbd,struct fontop* font)
{
	con.screen = screen;
	con.kbd = kbd;
	con.font = font;

	if(con.screen->init)
		con.screen->init(con.screen);
	if(con.kbd->init)
		con.kbd->init(con.kbd);
}
