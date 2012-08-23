#include <stdio.h>

#include "common.h"
#include "console/console.h"
#include "vte/vte.h"
#include "font.h"

static const unsigned char fontdata[]={
#include "font_cjk.h"
};

static inline int fontdataoffset(unsigned long glyph,int isright)
{
	if(glyph==0)
		glyph = ' ';

	if(glyph <256)
		return glyph * 16;
	else if(glyph < 65535)
		return glyph * 32 + 16 * isright;
	else{
		printf("bad glyph %d\n",(int)glyph);
		return 0;
	}
}

static void draw_one(struct fontop* fontop, struct screenop * screenop,int x, int y,struct font_char_attr *attr)
{
	uint32_t data[8*16];

	struct fontbmp fontbmp = {8,16,32};

	fontbmp.data = data;

	// 1 bit BMP
	const unsigned char * fontbits = & fontdata[ fontdataoffset(attr->uchar,attr->righthalf) ];

	//convert to 32bit, one font 16byte
	for(int i=0;i<16;i++){
		unsigned char c = fontbits[i];

		// blend the underline , 第 13 行
		if(attr->underline && i ==13)
			c = 0xFF;

		// 根據 c 配置咯 , 8 個字節

		for( int j=0; j <8;j++){
			data [i*8+j ] = (c & (1<<(7-j)))?attr->f.u:attr->b.u;
		}
	}
	screenop->bitblt(screenop,x,y,&fontbmp);
}

static int draw_line(struct fontop* fontop, struct screenop * screenop, struct vte * vte,int row)
{
	int x,y;
	y = 16 * row;

	for(int i=0; i < vte->cols ; i++)
	{
		x = i * 8;
		draw_one(fontop,screenop,x,y,& vte->screenbuf[ vte->cols * row  + i ]);
	}
	return 0;
}

static int getfontsize( struct fontop * op  ){return 16;}
static int setfontsize(struct fontop *op,int height){return height==16?0:-1;}

static struct fontop staticfontop={
		.getfontsize = getfontsize,
		.setfontsize = setfontsize,
		.draw_line = draw_line,
};

struct fontop *font_static_new()
{
	return &staticfontop;
}

