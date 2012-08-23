/*
 * font.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */
#pragma once
#include <stdint.h>
#include <common.h>

struct screenop;
struct vte;

struct font_char_attr {

	uint32_t	uchar; // unicode char

	union color f;
	union color b;

	unsigned int bold : 1;		/* bold character */
	unsigned int underline : 1;	/* underlined character */
	unsigned int inverse : 1;	/* inverse colors */
	unsigned int protect : 1;	/* cannot be erased */
	unsigned int righthalf : 1;	/* for doublewidth */
};

/* font bmp must be  32-bit RGBA format */
struct fontbmp{
	int width;
	int height;
	int stride;
	void * data;
};

/*
 *	op for font
 */
struct fontop{
	int (*init)();
	/* set the name if the font	 */
	int (*setfontname)(const char * fontname);

	int (*setfontsize)(struct fontop *,int height);
	int (*getfontsize)(struct fontop *); // height

	/* draw one line, because we need to do text layout */
	int	(*draw_line)(struct fontop*,struct screenop * screenop, struct vte * vte,int row);

	/* cursor is based on font, so here*/
	void (*draw_cursor)(struct fontop*,struct screenop * screenop, struct vte * vte);

	void	*private_data;
};

struct fontop *font_static_new();
