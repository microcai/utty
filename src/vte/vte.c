#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "console/console.h"
#include "font/font.h"
#include "vte/vte.h"
#include "vte/utf8.h"

enum vte_color {
	COLOR_BLACK,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_YELLOW,
	COLOR_BLUE,
	COLOR_MAGENTA,
	COLOR_CYAN,
	COLOR_LIGHT_GREY,
	COLOR_DARK_GREY,
	COLOR_LIGHT_RED,
	COLOR_LIGHT_GREEN,
	COLOR_LIGHT_YELLOW,
	COLOR_LIGHT_BLUE,
	COLOR_LIGHT_MAGENTA,
	COLOR_LIGHT_CYAN,
	COLOR_WHITE,
	COLOR_FOREGROUND,
	COLOR_BACKGROUND,
	COLOR_NUM
};

static uint8_t color_palette[COLOR_NUM][3] = {
	[COLOR_BLACK]         = {   0,   0,   0 }, /* black */
	[COLOR_RED]           = { 205,   0,   0 }, /* red */
	[COLOR_GREEN]         = {   0, 205,   0 }, /* green */
	[COLOR_YELLOW]        = { 205, 205,   0 }, /* yellow */
	[COLOR_BLUE]          = {   0,   0, 238 }, /* blue */
	[COLOR_MAGENTA]       = { 205,   0, 205 }, /* magenta */
	[COLOR_CYAN]          = {   0, 205, 205 }, /* cyan */
	[COLOR_LIGHT_GREY]    = { 229, 229, 229 }, /* light grey */
	[COLOR_DARK_GREY]     = { 127, 127, 127 }, /* dark grey */
	[COLOR_LIGHT_RED]     = { 255,   0,   0 }, /* light red */
	[COLOR_LIGHT_GREEN]   = {   0, 255,   0 }, /* light green */
	[COLOR_LIGHT_YELLOW]  = { 255, 255,   0 }, /* light yellow */
	[COLOR_LIGHT_BLUE]    = {  92,  92, 255 }, /* light blue */
	[COLOR_LIGHT_MAGENTA] = { 255,   0, 255 }, /* light magenta */
	[COLOR_LIGHT_CYAN]    = {   0, 255, 255 }, /* light cyan */
	[COLOR_WHITE]         = { 255, 255, 255 }, /* white */

	[COLOR_FOREGROUND]    = { 229, 229, 229 }, /* light grey */
	[COLOR_BACKGROUND]    = {   0,   0,   0 }, /* black */
};

static uint8_t color_palette_solarized[COLOR_NUM][3] = {
	[COLOR_BLACK]         = {   7,  54,  66 }, /* black */
	[COLOR_RED]           = { 220,  50,  47 }, /* red */
	[COLOR_GREEN]         = { 133, 153,   0 }, /* green */
	[COLOR_YELLOW]        = { 181, 137,   0 }, /* yellow */
	[COLOR_BLUE]          = {  38, 139, 210 }, /* blue */
	[COLOR_MAGENTA]       = { 211,  54, 130 }, /* magenta */
	[COLOR_CYAN]          = {  42, 161, 152 }, /* cyan */
	[COLOR_LIGHT_GREY]    = { 238, 232, 213 }, /* light grey */
	[COLOR_DARK_GREY]     = {   0,  43,  54 }, /* dark grey */
	[COLOR_LIGHT_RED]     = { 203,  75,  22 }, /* light red */
	[COLOR_LIGHT_GREEN]   = {  88, 110, 117 }, /* light green */
	[COLOR_LIGHT_YELLOW]  = { 101, 123, 131 }, /* light yellow */
	[COLOR_LIGHT_BLUE]    = { 131, 148, 150 }, /* light blue */
	[COLOR_LIGHT_MAGENTA] = { 108, 113, 196 }, /* light magenta */
	[COLOR_LIGHT_CYAN]    = { 147, 161, 161 }, /* light cyan */
	[COLOR_WHITE]         = { 253, 246, 227 }, /* white */

	[COLOR_FOREGROUND]    = { 238, 232, 213 }, /* light grey */
	[COLOR_BACKGROUND]    = {   7,  54,  66 }, /* black */
};

static uint8_t color_palette_solarized_black[COLOR_NUM][3] = {
	[COLOR_BLACK]         = {   0,   0,   0 }, /* black */
	[COLOR_RED]           = { 220,  50,  47 }, /* red */
	[COLOR_GREEN]         = { 133, 153,   0 }, /* green */
	[COLOR_YELLOW]        = { 181, 137,   0 }, /* yellow */
	[COLOR_BLUE]          = {  38, 139, 210 }, /* blue */
	[COLOR_MAGENTA]       = { 211,  54, 130 }, /* magenta */
	[COLOR_CYAN]          = {  42, 161, 152 }, /* cyan */
	[COLOR_LIGHT_GREY]    = { 238, 232, 213 }, /* light grey */
	[COLOR_DARK_GREY]     = {   0,  43,  54 }, /* dark grey */
	[COLOR_LIGHT_RED]     = { 203,  75,  22 }, /* light red */
	[COLOR_LIGHT_GREEN]   = {  88, 110, 117 }, /* light green */
	[COLOR_LIGHT_YELLOW]  = { 101, 123, 131 }, /* light yellow */
	[COLOR_LIGHT_BLUE]    = { 131, 148, 150 }, /* light blue */
	[COLOR_LIGHT_MAGENTA] = { 108, 113, 196 }, /* light magenta */
	[COLOR_LIGHT_CYAN]    = { 147, 161, 161 }, /* light cyan */
	[COLOR_WHITE]         = { 253, 246, 227 }, /* white */

	[COLOR_FOREGROUND]    = { 238, 232, 213 }, /* light grey */
	[COLOR_BACKGROUND]    = {   0,   0,   0 }, /* black */
};

static uint8_t color_palette_solarized_white[COLOR_NUM][3] = {
	[COLOR_BLACK]         = {   7,  54,  66 }, /* black */
	[COLOR_RED]           = { 220,  50,  47 }, /* red */
	[COLOR_GREEN]         = { 133, 153,   0 }, /* green */
	[COLOR_YELLOW]        = { 181, 137,   0 }, /* yellow */
	[COLOR_BLUE]          = {  38, 139, 210 }, /* blue */
	[COLOR_MAGENTA]       = { 211,  54, 130 }, /* magenta */
	[COLOR_CYAN]          = {  42, 161, 152 }, /* cyan */
	[COLOR_LIGHT_GREY]    = { 238, 232, 213 }, /* light grey */
	[COLOR_DARK_GREY]     = {   0,  43,  54 }, /* dark grey */
	[COLOR_LIGHT_RED]     = { 203,  75,  22 }, /* light red */
	[COLOR_LIGHT_GREEN]   = {  88, 110, 117 }, /* light green */
	[COLOR_LIGHT_YELLOW]  = { 101, 123, 131 }, /* light yellow */
	[COLOR_LIGHT_BLUE]    = { 131, 148, 150 }, /* light blue */
	[COLOR_LIGHT_MAGENTA] = { 108, 113, 196 }, /* light magenta */
	[COLOR_LIGHT_CYAN]    = { 147, 161, 161 }, /* light cyan */
	[COLOR_WHITE]         = { 253, 246, 227 }, /* white */

	[COLOR_FOREGROUND]    = {   7,  54,  66 }, /* black */
	[COLOR_BACKGROUND]    = { 238, 232, 213 }, /* light grey */
};

#define make_font_char_attr(x)	(struct font_char_attr){.uchar = x , .f = vte->f , .b = vte->b}

static void process_key(struct vte* vte,struct input_event* event)
{

}

static void vte_scroll_up(struct vte* vte,int lines)
{
	// rewind the pos
	vte->pos -= vte->cols * lines;
	// move
	memmove(vte->screenbuf,vte->screenbuf + vte->cols*lines, vte->sizepreline * (vte->rows -  lines));

	// clear last line
	for(int i=0;i < vte->cols * lines;i++){
		vte->screenbuf[vte->pos +i] = make_font_char_attr(' ');
	}
}

static void vte_check_scroll_up(struct vte* vte)
{
	if(vte->pos >= vte->cols * vte->rows)
		vte_scroll_up(vte,1);
}

static void vte_next_return(struct vte* vte)
{
	vte->pos = ((vte->pos / vte->cols)) * vte->cols;
}

static void vte_next_line(struct vte* vte)
{
	vte->pos = ((vte->pos / vte->cols)+1) * vte->cols;
	vte_check_scroll_up(vte);
}

static void vte_append(struct vte* vte,struct font_char_attr * attr)
{
	vte->screenbuf[vte->pos++] = *attr;

	if(iswide(attr->uchar)){
		attr->righthalf = 1;
		vte->screenbuf[vte->pos++] = *attr;
	}
	vte_check_scroll_up(vte);
}

static void vte_feed(struct vte* vte, struct chunk * chunk)
{
	const unsigned char * data = chunk->data;

	vte_lock(vte);

	uint32_t	ucs[chunk->size];

	memset(ucs,0,chunk->size);

	int size = utf8_ucs(ucs,chunk->size,chunk->data);

	for(int i=0;i < size ; i++){
		if( isprintable(ucs[i]))
			vte_append(vte, &  make_font_char_attr(ucs[i]));
		else{
			switch(ucs[i]){
			case '\n':
				vte_next_line(vte);
				break;
			case '\r':
				vte_next_return(vte);
				break;
			// case ECO
			}
		}
	}

	if( vte == vte->con->vte ){
		// redraw
		vte->con->con_redraw(vte->con);
	}

	vte_unlock(vte);
}

static void vte_resize(struct vte*vte,int cols,int rows)
{
	vte_lock(vte);

	vte->cols = cols;
	vte->rows = rows;
	vte->sizepreline = cols * sizeof(struct font_char_attr);
	vte->screenbuf = realloc(vte->screenbuf, vte->sizepreline * rows );
	vte_unlock(vte);
}

static void vte_clear(struct vte * vte)
{
	memset(vte->screenbuf,0,vte->sizepreline * vte->rows);
	vte->pos=0;
}

struct vte * vte_new()
{
	struct vte * vte = calloc(1,sizeof( struct vte));
	if(vte){
		vte->process_key = process_key;
		pthread_mutex_init(&vte->lock,NULL);
		vte->resize = vte_resize;
		vte->feed = vte_feed;
		vte->f =make_color3(color_palette[COLOR_GREEN]);

		vte->clear = vte_clear;
	}
	return vte;
}
