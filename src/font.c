/*
 * font.c
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#include <cairo/cairo.h>
#include <SDL/SDL_video.h>

#include "console.h"
#include "font.h"

static int sdldeeps[] =
{ 32, 1, 8, 2, 4, 8, 8 };

void init_font(int argc, char **argv)
{
	glong width,height;

	console_get_window_size(&width,&height);

}

SDL_Surface * font_render_unicode(gunichar uc,int pixelsize,guint64 attribute)
{
	char buf[6]={0};

	guchar attr;

	union attribute a ;

	a.qword = attribute;
	attr = a.s.attr;

	g_unichar_to_utf8(uc,buf);

	cairo_surface_t *cairo_surface =  cairo_image_surface_create(CAIRO_FORMAT_RGB24,pixelsize,pixelsize);

	cairo_t *cairo = cairo_create(cairo_surface);

	cairo_set_source_rgb(cairo,a.s.bg.red,a.s.bg.green,a.s.bg.blue);

	cairo_fill(cairo);

	cairo_set_source_rgb(cairo,a.s.fg.red,a.s.fg.green,a.s.fg.blue);

	cairo_select_font_face(cairo,"Monospace",attr & ATTR_ALANT, attr & ATTR_BOLD);

	cairo_set_font_size(cairo,pixelsize);

	cairo_move_to(cairo,0,pixelsize*0.8);

	cairo_show_text(cairo,buf);

	cairo_surface_flush(cairo_surface);

	void * buffer = cairo_image_surface_get_data(cairo_surface);
	int		pixwidth = cairo_image_surface_get_width(cairo_surface);
	int		pixrows = cairo_image_surface_get_height(cairo_surface);
	int		pitch = cairo_image_surface_get_stride(cairo_surface);

	SDL_Surface * surface = SDL_CreateRGBSurfaceFrom(buffer, pixwidth,pixrows,32, pitch, 0xff0000, 0xff00, 0xff, 0);

	return surface;
}

