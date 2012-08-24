/*
 * kmscon - VT Emulator
 *
 * Copyright (c) 2011 David Herrmann <dh.herrmann@googlemail.com>
 * Copyright (c) 2011 University of Tuebingen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Virtual Terminal Emulator
 * This is the VT implementation. It is written from scratch. It uses the
 * console subsystem as output and is tightly bound to it. It supports
 * functionality from vt100 up to vt500 series. It doesn't implement an
 * explicitly selected terminal but tries to support the most important commands
 * to be compatible with existing implementations. However, full vt102
 * compatibility is the least that is provided.
 *
 * The main parser in this file controls the parser-state and dispatches the
 * actions to the related handlers. The parser is based on the state-diagram
 * from Paul Williams: http://vt100.net/emu/
 * It is written from scratch, though.
 * This parser is fully compatible up to the vt500 series. It requires UTF-8 and
 * does not support any other input encoding. The G0 and G1 sets are therefore
 * defined as subsets of UTF-8. You may still map G0-G3 into GL, though.
 *
 * However, the CSI/DCS/etc handlers are not designed after a specific VT
 * series. We try to support all vt102 commands but implement several other
 * often used sequences, too. Feel free to add further.
 *
 * See ./doc/vte.txt for more information on this VT-emulator.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>
//#include "console.h"
#include "unicode.h"
#include "vte.h"

#define LOG_SUBSYSTEM "vte"

/* Input parser states */
enum parser_state {
	STATE_NONE,		/* placeholder */
	STATE_GROUND,		/* initial state and ground */
	STATE_ESC,		/* ESC sequence was started */
	STATE_ESC_INT,		/* intermediate escape characters */
	STATE_CSI_ENTRY,	/* starting CSI sequence */
	STATE_CSI_PARAM,	/* CSI parameters */
	STATE_CSI_INT,		/* intermediate CSI characters */
	STATE_CSI_IGNORE,	/* CSI error; ignore this CSI sequence */
	STATE_DCS_ENTRY,	/* starting DCS sequence */
	STATE_DCS_PARAM,	/* DCS parameters */
	STATE_DCS_INT,		/* intermediate DCS characters */
	STATE_DCS_PASS,		/* DCS data passthrough */
	STATE_DCS_IGNORE,	/* DCS error; ignore this DCS sequence */
	STATE_OSC_STRING,	/* parsing OCS sequence */
	STATE_ST_IGNORE,	/* unimplemented seq; ignore until ST */
	STATE_NUM
};

/* Input parser actions */
enum parser_action {
	ACTION_NONE,		/* placeholder */
	ACTION_IGNORE,		/* ignore the character entirely */
	ACTION_PRINT,		/* print the character on the console */
	ACTION_EXECUTE,		/* execute single control character (C0/C1) */
	ACTION_CLEAR,		/* clear current parameter state */
	ACTION_COLLECT,		/* collect intermediate character */
	ACTION_PARAM,		/* collect parameter character */
	ACTION_ESC_DISPATCH,	/* dispatch escape sequence */
	ACTION_CSI_DISPATCH,	/* dispatch csi sequence */
	ACTION_DCS_START,	/* start of DCS data */
	ACTION_DCS_COLLECT,	/* collect DCS data */
	ACTION_DCS_END,		/* end of DCS data */
	ACTION_OSC_START,	/* start of OSC data */
	ACTION_OSC_COLLECT,	/* collect OSC data */
	ACTION_OSC_END,		/* end of OSC data */
	ACTION_NUM
};

/* CSI flags */
#define CSI_BANG	0x0001		/* CSI: ! */
#define CSI_CASH	0x0002		/* CSI: $ */
#define CSI_WHAT	0x0004		/* CSI: ? */
#define CSI_GT		0x0008		/* CSI: > */
#define CSI_SPACE	0x0010		/* CSI:   */
#define CSI_SQUOTE	0x0020		/* CSI: ' */
#define CSI_DQUOTE	0x0040		/* CSI: " */
#define CSI_MULT	0x0080		/* CSI: * */
#define CSI_PLUS	0x0100		/* CSI: + */
#define CSI_POPEN	0x0200		/* CSI: ( */
#define CSI_PCLOSE	0x0400		/* CSI: ) */

/* max CSI arguments */
#define CSI_ARG_MAX 16

/* terminal flags */
#define FLAG_CURSOR_KEY_MODE			0x00000001 /* DEC cursor key mode */
#define FLAG_KEYPAD_APPLICATION_MODE		0x00000002 /* DEC keypad application mode; TODO: toggle on numlock? */
#define FLAG_LINE_FEED_NEW_LINE_MODE		0x00000004 /* DEC line-feed/new-line mode */
#define FLAG_8BIT_MODE				0x00000008 /* Disable UTF-8 mode and enable 8bit compatible mode */
#define FLAG_7BIT_MODE				0x00000010 /* Disable 8bit mode and use 7bit compatible mode */
#define FLAG_USE_C1				0x00000020 /* Explicitely use 8bit C1 codes; TODO: implement */
#define FLAG_KEYBOARD_ACTION_MODE		0x00000040 /* Disable keyboard; TODO: implement? */
#define FLAG_INSERT_REPLACE_MODE		0x00000080 /* Enable insert mode */
#define FLAG_SEND_RECEIVE_MODE			0x00000100 /* Disable local echo */
#define FLAG_TEXT_CURSOR_MODE			0x00000200 /* Show cursor */
#define FLAG_INVERSE_SCREEN_MODE		0x00000400 /* Inverse colors */
#define FLAG_ORIGIN_MODE			0x00000800 /* Relative origin for cursor */
#define FLAG_AUTO_WRAP_MODE			0x00001000 /* Auto line wrap mode */
#define FLAG_AUTO_REPEAT_MODE			0x00002000 /* Auto repeat key press; TODO: implement */
#define FLAG_NATIONAL_CHARSET_MODE		0x00004000 /* Send keys from nation charsets; TODO: implement */

struct vte_saved_state {
	unsigned int cursor_x;
	unsigned int cursor_y;
	struct font_char_attr cattr;
	kmscon_vte_charset *gl;
	kmscon_vte_charset *gr;
	bool wrap_mode;
	bool origin_mode;
};

struct kmscon_vte {
	unsigned long ref;
	struct kmscon_console *con;
	kmscon_vte_write_cb write_cb;
	void *data;

	struct kmscon_utf8_mach *mach;
	unsigned long parse_cnt;

	unsigned int state;
	unsigned int csi_argc;
	int csi_argv[CSI_ARG_MAX];
	unsigned int csi_flags;

	uint8_t (*palette)[3];
	struct font_char_attr def_attr;
	struct font_char_attr cattr;
	unsigned int flags;

	kmscon_vte_charset *gl;
	kmscon_vte_charset *gr;
	kmscon_vte_charset *glt;
	kmscon_vte_charset *grt;
	kmscon_vte_charset *g0;
	kmscon_vte_charset *g1;
	kmscon_vte_charset *g2;
	kmscon_vte_charset *g3;

	struct vte_saved_state saved_state;
};

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

static uint8_t (*get_palette(void))[3]
{
	if (!kmscon_conf.palette)
		return color_palette;

	if (!strcmp(kmscon_conf.palette, "solarized"))
		return color_palette_solarized;
	if (!strcmp(kmscon_conf.palette, "solarized-black"))
		return color_palette_solarized_black;
	if (!strcmp(kmscon_conf.palette, "solarized-white"))
		return color_palette_solarized_white;

	return color_palette;
}

static void set_fcolor(struct font_char_attr *attr, unsigned int color,
		       struct kmscon_vte *vte)
{
	if (color >= COLOR_NUM)
		color = COLOR_WHITE;

	attr->fr = vte->palette[color][0];
	attr->fg = vte->palette[color][1];
	attr->fb = vte->palette[color][2];
}

static void set_bcolor(struct font_char_attr *attr, unsigned int color,
		       struct kmscon_vte *vte)
{
	if (color >= COLOR_NUM)
		color = COLOR_BLACK;

	attr->br = vte->palette[color][0];
	attr->bg = vte->palette[color][1];
	attr->bb = vte->palette[color][2];
}

static void copy_fcolor(struct font_char_attr *dest,
			const struct font_char_attr *src)
{
	dest->fr = src->fr;
	dest->fg = src->fg;
	dest->fb = src->fb;
}

static void copy_bcolor(struct font_char_attr *dest,
			const struct font_char_attr *src)
{
	dest->br = src->br;
	dest->bg = src->bg;
	dest->bb = src->bb;
}

int kmscon_vte_new(struct kmscon_vte **out, struct kmscon_console *con,
		   kmscon_vte_write_cb write_cb, void *data)
{
	struct kmscon_vte *vte;
	int ret;

	if (!out || !con || !write_cb)
		return -EINVAL;

	vte = malloc(sizeof(*vte));
	if (!vte)
		return -ENOMEM;

	memset(vte, 0, sizeof(*vte));
	vte->ref = 1;
	vte->con = con;
	vte->write_cb = write_cb;
	vte->data = data;
	vte->palette = get_palette();
	set_fcolor(&vte->def_attr, COLOR_FOREGROUND, vte);
	set_bcolor(&vte->def_attr, COLOR_BACKGROUND, vte);

	ret = kmscon_utf8_mach_new(&vte->mach);
	if (ret)
		goto err_free;

	kmscon_vte_reset(vte);
	kmscon_console_set_def_attr(vte->con, &vte->def_attr);
	kmscon_console_erase_screen(vte->con, false);

	log_debug("new vte object");
	kmscon_console_ref(vte->con);
	*out = vte;
	return 0;

err_free:
	free(vte);
	return ret;
}

void kmscon_vte_ref(struct kmscon_vte *vte)
{
	if (!vte)
		return;

	vte->ref++;
}

void kmscon_vte_unref(struct kmscon_vte *vte)
{
	if (!vte || !vte->ref)
		return;

	if (--vte->ref)
		return;

	log_debug("destroying vte object");
	kmscon_console_unref(vte->con);
	kmscon_utf8_mach_free(vte->mach);
	free(vte);
}

/*
 * Write raw byte-stream to pty.
 * When writing data to the client we must make sure that we send the correct
 * encoding. For backwards-compatibility reasons we should always send 7bit
 * characters exclusively. However, when FLAG_7BIT_MODE is not set, then we can
 * also send raw 8bit characters. For instance, in FLAG_8BIT_MODE we can use the
 * GR characters as keyboard input and send them directly or even use the C1
 * escape characters. In unicode mode (default) we can send multi-byte utf-8
 * characters which are also 8bit. When sending these characters, set the \raw
 * flag to true so this function does not perform debug checks on data we send.
 * If debugging is disabled, these checks are also disabled and won't affect
 * performance.
 * For better debugging, we also use the __LINE__ and __FILE__ macros. Use the
 * vte_write() and vte_write_raw() macros below for more convenient use.
 *
 * As a rule of thumb do never send 8bit characters in escape sequences and also
 * avoid all 8bit escape codes including the C1 codes. This will guarantee that
 * all kind of clients are always compatible to us.
 *
 * If SEND_RECEIVE_MODE is off (that is, local echo is on) we have to send all
 * data directly to ourself again. However, we must avoid recursion when
 * kmscon_vte_input() itself calls vte_write*(), therefore, we increase the
 * PARSER counter when entering kmscon_vte_input() and reset it when leaving it
 * so we never echo data that origins from kmscon_vte_input().
 * But note that SEND_RECEIVE_MODE is inherently broken for escape sequences
 * that request answers. That is, if we send a request to the client that awaits
 * a response and parse that request via local echo ourself, then we will also
 * send a response to the client even though he didn't request one. This
 * recursion fix does not avoid this but only prevents us from endless loops
 * here. Anyway, only few applications rely on local echo so we can safely
 * ignore this.
 */
static void vte_write_debug(struct kmscon_vte *vte, const char *u8, size_t len,
			    bool raw, const char *file, int line)
{
#ifdef KMSCON_ENABLE_DEBUG
	/* in debug mode we check that escape sequences are always <0x7f so they
	 * are correctly parsed by non-unicode and non-8bit-mode clients. */
	size_t i;

	if (!raw) {
		for (i = 0; i < len; ++i) {
			if (u8[i] & 0x80)
				log_warning("sending 8bit character inline to client in %s:%d",
					    file, line);
		}
	}
#endif

	/* in local echo mode, directly parse the data again */
	if (!vte->parse_cnt && !(vte->flags & FLAG_SEND_RECEIVE_MODE))
		kmscon_vte_input(vte, u8, len);

	vte->write_cb(vte, u8, len, vte->data);
}

#define vte_write(_vte, _u8, _len) \
	vte_write_debug((_vte), (_u8), (_len), false, __FILE__, __LINE__)
#define vte_write_raw(_vte, _u8, _len) \
	vte_write_debug((_vte), (_u8), (_len), true, __FILE__, __LINE__)

/* write to console */
static void write_console(struct kmscon_vte *vte, kmscon_symbol_t sym)
{
	kmscon_console_write(vte->con, sym, &vte->cattr);
}

static void reset_state(struct kmscon_vte *vte)
{
	vte->saved_state.cursor_x = 0;
	vte->saved_state.cursor_y = 0;
	vte->saved_state.origin_mode = false;
	vte->saved_state.wrap_mode = true;
	vte->saved_state.gl = &kmscon_vte_unicode_lower;
	vte->saved_state.gr = &kmscon_vte_unicode_upper;

	copy_fcolor(&vte->saved_state.cattr, &vte->def_attr);
	copy_bcolor(&vte->saved_state.cattr, &vte->def_attr);
	vte->saved_state.cattr.bold = 0;
	vte->saved_state.cattr.underline = 0;
	vte->saved_state.cattr.inverse = 0;
	vte->saved_state.cattr.protect = 0;
}

static void save_state(struct kmscon_vte *vte)
{
	vte->saved_state.cursor_x = kmscon_console_get_cursor_x(vte->con);
	vte->saved_state.cursor_y = kmscon_console_get_cursor_y(vte->con);
	vte->saved_state.cattr = vte->cattr;
	vte->saved_state.gl = vte->gl;
	vte->saved_state.gr = vte->gr;
	vte->saved_state.wrap_mode = vte->flags & FLAG_AUTO_WRAP_MODE;
	vte->saved_state.origin_mode = vte->flags & FLAG_ORIGIN_MODE;
}

static void restore_state(struct kmscon_vte *vte)
{
	kmscon_console_move_to(vte->con, vte->saved_state.cursor_x,
			       vte->saved_state.cursor_y);
	vte->cattr = vte->saved_state.cattr;
	vte->gl = vte->saved_state.gl;
	vte->gr = vte->saved_state.gr;

	if (vte->saved_state.wrap_mode) {
		vte->flags |= FLAG_AUTO_WRAP_MODE;
		kmscon_console_set_flags(vte->con,
					 KMSCON_CONSOLE_AUTO_WRAP);
	} else {
		vte->flags &= ~FLAG_AUTO_WRAP_MODE;
		kmscon_console_reset_flags(vte->con,
					   KMSCON_CONSOLE_AUTO_WRAP);
	}

	if (vte->saved_state.origin_mode) {
		vte->flags |= FLAG_ORIGIN_MODE;
		kmscon_console_set_flags(vte->con,
					 KMSCON_CONSOLE_REL_ORIGIN);
	} else {
		vte->flags &= ~FLAG_ORIGIN_MODE;
		kmscon_console_reset_flags(vte->con,
					   KMSCON_CONSOLE_REL_ORIGIN);
	}
}

/*
 * Reset VTE state
 * This performs a soft reset of the VTE. That is, everything is reset to the
 * same state as when the VTE was created. This does not affect the console,
 * though.
 */
void kmscon_vte_reset(struct kmscon_vte *vte)
{
	if (!vte)
		return;

	vte->flags = 0;
	vte->flags |= FLAG_TEXT_CURSOR_MODE;
	vte->flags |= FLAG_AUTO_REPEAT_MODE;
	vte->flags |= FLAG_SEND_RECEIVE_MODE;
	vte->flags |= FLAG_AUTO_WRAP_MODE;
	kmscon_console_reset(vte->con);
	kmscon_console_set_flags(vte->con, KMSCON_CONSOLE_AUTO_WRAP);

	kmscon_utf8_mach_reset(vte->mach);
	vte->state = STATE_GROUND;
	vte->gl = &kmscon_vte_unicode_lower;
	vte->gr = &kmscon_vte_unicode_upper;
	vte->glt = NULL;
	vte->grt = NULL;
	vte->g0 = &kmscon_vte_unicode_lower;
	vte->g1 = &kmscon_vte_unicode_upper;
	vte->g2 = &kmscon_vte_unicode_lower;
	vte->g3 = &kmscon_vte_unicode_upper;

	vte->cattr.fr = 255;
	vte->cattr.fg = 255;
	vte->cattr.fb = 255;
	vte->cattr.br = 0;
	vte->cattr.bg = 0;
	vte->cattr.bb = 0;
	vte->cattr.bold = 0;
	vte->cattr.underline = 0;
	vte->cattr.inverse = 0;

	reset_state(vte);
}

static void hard_reset(struct kmscon_vte *vte)
{
	kmscon_console_erase_screen(vte->con, false);
	kmscon_console_clear_sb(vte->con);
	kmscon_console_move_to(vte->con, 0, 0);
	kmscon_vte_reset(vte);
}

static void send_primary_da(struct kmscon_vte *vte)
{
	vte_write(vte, "\e[?60;1;6;9;15c", 17);
}

/* execute control character (C0 or C1) */
static void do_execute(struct kmscon_vte *vte, uint32_t ctrl)
{
	switch (ctrl) {
	case 0x00: /* NUL */
		/* Ignore on input */
		break;
	case 0x05: /* ENQ */
		/* Transmit answerback message */
		/* TODO: is there a better answer than ACK?  */
		vte_write(vte, "\x06", 1);
		break;
	case 0x07: /* BEL */
		/* Sound bell tone */
		/* TODO: I always considered this annying, however, we
		 * should at least provide some way to enable it if the
		 * user *really* wants it.
		 */
		break;
	case 0x08: /* BS */
		/* Move cursor one position left */
		kmscon_console_move_left(vte->con, 1);
		break;
	case 0x09: /* HT */
		/* Move to next tab stop or end of line */
		kmscon_console_tab_right(vte->con, 1);
		break;
	case 0x0a: /* LF */
	case 0x0b: /* VT */
	case 0x0c: /* FF */
		/* Line feed or newline (CR/NL mode) */
		if (vte->flags & FLAG_LINE_FEED_NEW_LINE_MODE)
			kmscon_console_newline(vte->con);
		else
			kmscon_console_move_down(vte->con, 1, true);
		break;
	case 0x0d: /* CR */
		/* Move cursor to left margin */
		kmscon_console_move_line_home(vte->con);
		break;
	case 0x0e: /* SO */
		/* Map G1 character set into GL */
		vte->gl = vte->g1;
		break;
	case 0x0f: /* SI */
		/* Map G0 character set into GL */
		vte->gl = vte->g0;
		break;
	case 0x11: /* XON */
		/* Resume transmission */
		/* TODO */
		break;
	case 0x13: /* XOFF */
		/* Stop transmission */
		/* TODO */
		break;
	case 0x18: /* CAN */
		/* Cancel escape sequence */
		/* nothing to do here */
		break;
	case 0x1a: /* SUB */
		/* Discard current escape sequence and show err-sym */
		write_console(vte, 0xbf);
		break;
	case 0x1b: /* ESC */
		/* Invokes an escape sequence */
		/* nothing to do here */
		break;
	case 0x1f: /* DEL */
		/* Ignored */
		break;
	case 0x84: /* IND */
		/* Move down one row, perform scroll-up if needed */
		kmscon_console_move_down(vte->con, 1, true);
		break;
	case 0x85: /* NEL */
		/* CR/NL with scroll-up if needed */
		kmscon_console_newline(vte->con);
		break;
	case 0x88: /* HTS */
		/* Set tab stop at current position */
		kmscon_console_set_tabstop(vte->con);
		break;
	case 0x8d: /* RI */
		/* Move up one row, perform scroll-down if needed */
		kmscon_console_move_up(vte->con, 1, true);
		break;
	case 0x8e: /* SS2 */
		/* Temporarily map G2 into GL for next char only */
		vte->glt = vte->g2;
		break;
	case 0x8f: /* SS3 */
		/* Temporarily map G3 into GL for next char only */
		vte->glt = vte->g3;
		break;
	case 0x9a: /* DECID */
		/* Send device attributes response like ANSI DA */
		send_primary_da(vte);
		break;
	case 0x9c: /* ST */
		/* End control string */
		/* nothing to do here */
		break;
	default:
		log_warn("unhandled control char %u", ctrl);
	}
}

static void do_clear(struct kmscon_vte *vte)
{
	int i;

	vte->csi_argc = 0;
	for (i = 0; i < CSI_ARG_MAX; ++i)
		vte->csi_argv[i] = -1;
	vte->csi_flags = 0;
}

static void do_collect(struct kmscon_vte *vte, uint32_t data)
{
	switch (data) {
	case '!':
		vte->csi_flags |= CSI_BANG;
		break;
	case '$':
		vte->csi_flags |= CSI_CASH;
		break;
	case '?':
		vte->csi_flags |= CSI_WHAT;
		break;
	case '>':
		vte->csi_flags |= CSI_GT;
		break;
	case ' ':
		vte->csi_flags |= CSI_SPACE;
		break;
	case '\'':
		vte->csi_flags |= CSI_SQUOTE;
		break;
	case '"':
		vte->csi_flags |= CSI_DQUOTE;
		break;
	case '*':
		vte->csi_flags |= CSI_MULT;
		break;
	case '+':
		vte->csi_flags |= CSI_PLUS;
		break;
	case '(':
		vte->csi_flags |= CSI_POPEN;
		break;
	case ')':
		vte->csi_flags |= CSI_PCLOSE;
		break;
	}
}

static void do_param(struct kmscon_vte *vte, uint32_t data)
{
	int new;

	if (data == ';') {
		if (vte->csi_argc < CSI_ARG_MAX)
			vte->csi_argc++;
		return;
	}

	if (vte->csi_argc >= CSI_ARG_MAX)
		return;

	/* avoid integer overflows; max allowed value is 16384 anyway */
	if (vte->csi_argv[vte->csi_argc] > 0xffff)
		return;

	if (data >= '0' && data <= '9') {
		new = vte->csi_argv[vte->csi_argc];
		if (new <= 0)
			new = data - '0';
		else
			new = new * 10 + data - '0';
		vte->csi_argv[vte->csi_argc] = new;
	}
}

static bool set_charset(struct kmscon_vte *vte, kmscon_vte_charset *set)
{
	if (vte->csi_flags & CSI_POPEN)
		vte->g0 = set;
	else if (vte->csi_flags & CSI_PCLOSE)
		vte->g1 = set;
	else if (vte->csi_flags & CSI_MULT)
		vte->g2 = set;
	else if (vte->csi_flags & CSI_PLUS)
		vte->g3 = set;
	else
		return false;

	return true;
}

static void do_esc(struct kmscon_vte *vte, uint32_t data)
{
	switch (data) {
	case 'B': /* map ASCII into G0-G3 */
		if (set_charset(vte, &kmscon_vte_unicode_lower))
			return;
		break;
	case '<': /* map DEC supplemental into G0-G3 */
		if (set_charset(vte, &kmscon_vte_dec_supplemental_graphics))
			return;
		break;
	case '0': /* map DEC special into G0-G3 */
		if (set_charset(vte, &kmscon_vte_dec_special_graphics))
			return;
		break;
	case 'A': /* map British into G0-G3 */
		/* TODO: create British charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case '4': /* map Dutch into G0-G3 */
		/* TODO: create Dutch charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'C':
	case '5': /* map Finnish into G0-G3 */
		/* TODO: create Finnish charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'R': /* map French into G0-G3 */
		/* TODO: create French charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'Q': /* map French-Canadian into G0-G3 */
		/* TODO: create French-Canadian charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'K': /* map German into G0-G3 */
		/* TODO: create German charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'Y': /* map Italian into G0-G3 */
		/* TODO: create Italian charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'E':
	case '6': /* map Norwegian/Danish into G0-G3 */
		/* TODO: create Norwegian/Danish charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'Z': /* map Spanish into G0-G3 */
		/* TODO: create Spanish charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'H':
	case '7': /* map Swedish into G0-G3 */
		/* TODO: create Swedish charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case '=': /* map Swiss into G0-G3 */
		/* TODO: create Swiss charset from DEC */
		if (set_charset(vte, &kmscon_vte_unicode_upper))
			return;
		break;
	case 'F':
		if (vte->csi_flags & CSI_SPACE) {
			/* S7C1T */
			/* Disable 8bit C1 mode */
			vte->flags &= ~FLAG_USE_C1;
			return;
		}
		break;
	case 'G':
		if (vte->csi_flags & CSI_SPACE) {
			/* S8C1T */
			/* Enable 8bit C1 mode */
			vte->flags |= FLAG_USE_C1;
			return;
		}
		break;
	}

	/* everything below is only valid without CSI flags */
	if (vte->csi_flags) {
		log_debug("unhandled escape seq %u", data);
		return;
	}

	switch (data) {
	case 'D': /* IND */
		/* Move down one row, perform scroll-up if needed */
		kmscon_console_move_down(vte->con, 1, true);
		break;
	case 'E': /* NEL */
		/* CR/NL with scroll-up if needed */
		kmscon_console_newline(vte->con);
		break;
	case 'H': /* HTS */
		/* Set tab stop at current position */
		kmscon_console_set_tabstop(vte->con);
		break;
	case 'M': /* RI */
		/* Move up one row, perform scroll-down if needed */
		kmscon_console_move_up(vte->con, 1, true);
		break;
	case 'N': /* SS2 */
		/* Temporarily map G2 into GL for next char only */
		vte->glt = vte->g2;
		break;
	case 'O': /* SS3 */
		/* Temporarily map G3 into GL for next char only */
		vte->glt = vte->g3;
		break;
	case 'Z': /* DECID */
		/* Send device attributes response like ANSI DA */
		send_primary_da(vte);
		break;
	case '\\': /* ST */
		/* End control string */
		/* nothing to do here */
		break;
	case '~': /* LS1R */
		/* Invoke G1 into GR */
		vte->gr = vte->g1;
		break;
	case 'n': /* LS2 */
		/* Invoke G2 into GL */
		vte->gl = vte->g2;
		break;
	case '}': /* LS2R */
		/* Invoke G2 into GR */
		vte->gr = vte->g2;
		break;
	case 'o': /* LS3 */
		/* Invoke G3 into GL */
		vte->gl = vte->g3;
		break;
	case '|': /* LS3R */
		/* Invoke G3 into GR */
		vte->gr = vte->g3;
		break;
	case '=': /* DECKPAM */
		/* Set application keypad mode */
		vte->flags |= FLAG_KEYPAD_APPLICATION_MODE;
		break;
	case '>': /* DECKPNM */
		/* Set numeric keypad mode */
		vte->flags &= ~FLAG_KEYPAD_APPLICATION_MODE;
		break;
	case 'c': /* RIS */
		/* hard reset */
		hard_reset(vte);
		break;
	case '7': /* DECSC */
		/* save console state */
		save_state(vte);
		break;
	case '8': /* DECRC */
		/* restore console state */
		restore_state(vte);
		break;
	default:
		log_debug("unhandled escape seq %u", data);
	}
}

static void csi_attribute(struct kmscon_vte *vte)
{
	static const uint8_t bval[6] = { 0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff };
	unsigned int i, code;

	if (vte->csi_argc <= 1 && vte->csi_argv[0] == -1) {
		vte->csi_argc = 1;
		vte->csi_argv[0] = 0;
	}

	for (i = 0; i < vte->csi_argc; ++i) {
		switch (vte->csi_argv[i]) {
		case -1:
			break;
		case 0:
			copy_fcolor(&vte->cattr, &vte->def_attr);
			copy_bcolor(&vte->cattr, &vte->def_attr);
			vte->cattr.bold = 0;
			vte->cattr.underline = 0;
			vte->cattr.inverse = 0;
			break;
		case 1:
			vte->cattr.bold = 1;
			break;
		case 4:
			vte->cattr.underline = 1;
			break;
		case 7:
			vte->cattr.inverse = 1;
			break;
		case 22:
			vte->cattr.bold = 0;
			break;
		case 24:
			vte->cattr.underline = 0;
			break;
		case 27:
			vte->cattr.inverse = 0;
			break;
		case 30:
			set_fcolor(&vte->cattr, COLOR_BLACK, vte);
			break;
		case 31:
			set_fcolor(&vte->cattr, COLOR_RED, vte);
			break;
		case 32:
			set_fcolor(&vte->cattr, COLOR_GREEN, vte);
			break;
		case 33:
			set_fcolor(&vte->cattr, COLOR_YELLOW, vte);
			break;
		case 34:
			set_fcolor(&vte->cattr, COLOR_BLUE, vte);
			break;
		case 35:
			set_fcolor(&vte->cattr, COLOR_MAGENTA, vte);
			break;
		case 36:
			set_fcolor(&vte->cattr, COLOR_CYAN, vte);
			break;
		case 37:
			set_fcolor(&vte->cattr, COLOR_LIGHT_GREY, vte);
			break;
		case 39:
			copy_fcolor(&vte->cattr, &vte->def_attr);
			break;
		case 40:
			set_bcolor(&vte->cattr, COLOR_BLACK, vte);
			break;
		case 41:
			set_bcolor(&vte->cattr, COLOR_RED, vte);
			break;
		case 42:
			set_bcolor(&vte->cattr, COLOR_GREEN, vte);
			break;
		case 43:
			set_bcolor(&vte->cattr, COLOR_YELLOW, vte);
			break;
		case 44:
			set_bcolor(&vte->cattr, COLOR_BLUE, vte);
			break;
		case 45:
			set_bcolor(&vte->cattr, COLOR_MAGENTA, vte);
			break;
		case 46:
			set_bcolor(&vte->cattr, COLOR_CYAN, vte);
			break;
		case 47:
			set_bcolor(&vte->cattr, COLOR_LIGHT_GREY, vte);
			break;
		case 49:
			copy_bcolor(&vte->cattr, &vte->def_attr);
			break;
		case 90:
			set_fcolor(&vte->cattr, COLOR_DARK_GREY, vte);
			break;
		case 91:
			set_fcolor(&vte->cattr, COLOR_LIGHT_RED, vte);
			break;
		case 92:
			set_fcolor(&vte->cattr, COLOR_LIGHT_GREEN, vte);
			break;
		case 93:
			set_fcolor(&vte->cattr, COLOR_LIGHT_YELLOW, vte);
			break;
		case 94:
			set_fcolor(&vte->cattr, COLOR_LIGHT_BLUE, vte);
			break;
		case 95:
			set_fcolor(&vte->cattr, COLOR_LIGHT_MAGENTA, vte);
			break;
		case 96:
			set_fcolor(&vte->cattr, COLOR_LIGHT_CYAN, vte);
			break;
		case 97:
			set_fcolor(&vte->cattr, COLOR_WHITE, vte);
			break;
		case 100:
			set_bcolor(&vte->cattr, COLOR_DARK_GREY, vte);
			break;
		case 101:
			set_bcolor(&vte->cattr, COLOR_LIGHT_RED, vte);
			break;
		case 102:
			set_bcolor(&vte->cattr, COLOR_LIGHT_GREEN, vte);
			break;
		case 103:
			set_bcolor(&vte->cattr, COLOR_LIGHT_YELLOW, vte);
			break;
		case 104:
			set_bcolor(&vte->cattr, COLOR_LIGHT_BLUE, vte);
			break;
		case 105:
			set_bcolor(&vte->cattr, COLOR_LIGHT_MAGENTA, vte);
			break;
		case 106:
			set_bcolor(&vte->cattr, COLOR_LIGHT_CYAN, vte);
			break;
		case 107:
			set_bcolor(&vte->cattr, COLOR_WHITE, vte);
			break;
		case 38:
			/* fallthrough */
		case 48:
			if (i + 2 >= vte->csi_argc ||
			    vte->csi_argv[i + 1] != 5 ||
			    vte->csi_argv[i + 2] < 0) {
				log_debug("invalid 256color SGR");
				break;
			}

			code = vte->csi_argv[i + 2];
			if (vte->csi_argv[i] == 38) {
				if (code < 16) {
					set_fcolor(&vte->cattr, code, vte);
				} else if (code < 232) {
					code -= 16;
					vte->cattr.fb = bval[code % 6];
					code /= 6;
					vte->cattr.fg = bval[code % 6];
					code /= 6;
					vte->cattr.fr = bval[code % 6];
				} else {
					code = (code - 232) * 10 + 8;
					vte->cattr.fr = code;
					vte->cattr.fg = code;
					vte->cattr.fb = code;
				}
			} else {
				if (code < 16) {
					set_bcolor(&vte->cattr, code, vte);
				} else if (code < 232) {
					code -= 16;
					vte->cattr.bb = bval[code % 6];
					code /= 6;
					vte->cattr.bg = bval[code % 6];
					code /= 6;
					vte->cattr.br = bval[code % 6];
				} else {
					code = (code - 232) * 10 + 8;
					vte->cattr.br = code;
					vte->cattr.bg = code;
					vte->cattr.bb = code;
				}
			}

			i += 2;
			break;
		default:
			log_debug("unhandled SGR attr %i",
				  vte->csi_argv[i]);
		}
	}
}

static void csi_soft_reset(struct kmscon_vte *vte)
{
	kmscon_vte_reset(vte);
}

static void csi_compat_mode(struct kmscon_vte *vte)
{
	/* always perform soft reset */
	csi_soft_reset(vte);

	if (vte->csi_argv[0] == 61) {
		/* Switching to VT100 compatibility mode. We do
		 * not support this mode, so ignore it. In fact,
		 * we are almost compatible to it, anyway, so
		 * there is no need to explicitely select it.
		 * However, we enable 7bit mode to avoid
		 * character-table problems */
		vte->flags |= FLAG_7BIT_MODE;
		vte->gl = &kmscon_vte_unicode_lower;
		vte->gr = &kmscon_vte_dec_supplemental_graphics;
	} else if (vte->csi_argv[0] == 62 ||
		   vte->csi_argv[0] == 63 ||
		   vte->csi_argv[0] == 64) {
		/* Switching to VT2/3/4 compatibility mode. We
		 * are always compatible with this so ignore it.
		 * We always send 7bit controls so we also do
		 * not care for the parameter value here that
		 * select the control-mode.
		 * VT220 defines argument 2 as 7bit mode but
		 * VT3xx up to VT5xx use it as 8bit mode. We
		 * choose to conform with the latter here.
		 * We also enable 8bit mode when VT220
		 * compatibility is requested explicitely. */
		if (vte->csi_argv[1] == 1 ||
		    vte->csi_argv[1] == 2)
			vte->flags |= FLAG_USE_C1;

		vte->flags |= FLAG_8BIT_MODE;
		vte->gl = &kmscon_vte_unicode_lower;
		vte->gr = &kmscon_vte_dec_supplemental_graphics;
	} else {
		log_debug("unhandled DECSCL 'p' CSI %i, switching to utf-8 mode again",
			  vte->csi_argv[0]);
	}
}

static inline void set_reset_flag(struct kmscon_vte *vte, bool set,
				  unsigned int flag)
{
	if (set)
		vte->flags |= flag;
	else
		vte->flags &= ~flag;
}

static void csi_mode(struct kmscon_vte *vte, bool set)
{
	unsigned int i;

	for (i = 0; i < vte->csi_argc; ++i) {
		if (!(vte->csi_flags & CSI_WHAT)) {
			switch (vte->csi_argv[i]) {
			case -1:
				continue;
			case 2: /* KAM */
				set_reset_flag(vte, set,
					       FLAG_KEYBOARD_ACTION_MODE);
				continue;
			case 4: /* IRM */
				set_reset_flag(vte, set,
					       FLAG_INSERT_REPLACE_MODE);
				if (set)
					kmscon_console_set_flags(vte->con,
						KMSCON_CONSOLE_INSERT_MODE);
				else
					kmscon_console_reset_flags(vte->con,
						KMSCON_CONSOLE_INSERT_MODE);
				continue;
			case 12: /* SRM */
				set_reset_flag(vte, set,
					       FLAG_SEND_RECEIVE_MODE);
				continue;
			case 20: /* LNM */
				set_reset_flag(vte, set,
					       FLAG_LINE_FEED_NEW_LINE_MODE);
				continue;
			default:
				log_debug("unknown non-DEC (Re)Set-Mode %d",
					  vte->csi_argv[i]);
				continue;
			}
		}

		switch (vte->csi_argv[i]) {
		case -1:
			continue;
		case 1: /* DECCKM */
			set_reset_flag(vte, set, FLAG_CURSOR_KEY_MODE);
			continue;
		case 2: /* DECANM */
			/* Select VT52 mode */
			/* We do not support VT52 mode. Is there any reason why
			 * we should support it? We ignore it here and do not
			 * mark it as to-do item unless someone has strong
			 * arguments to support it. */
			continue;
		case 3: /* DECCOLM */
			/* If set, select 132 column mode, otherwise use 80
			 * column mode. If neither is selected explicitely, we
			 * use dynamic mode, that is, we send SIGWCH when the
			 * size changes and we allow arbitrary buffer
			 * dimensions. On soft-reset, we automatically fall back
			 * to the default, that is, dynamic mode.
			 * Dynamic-mode can be forced to a static mode in the
			 * config. That is, everytime dynamic-mode becomes
			 * active, the terminal will be set to the dimensions
			 * that were selected in the config. This allows setting
			 * a fixed size for the terminal regardless of the
			 * display size.
			 * TODO: Implement this */
			continue;
		case 4: /* DECSCLM */
			/* Select smooth scrolling. We do not support the
			 * classic smooth scrolling because we have a scrollback
			 * buffer. There is no need to implement smooth
			 * scrolling so ignore this here. */
			continue;
		case 5: /* DECSCNM */
			set_reset_flag(vte, set, FLAG_INVERSE_SCREEN_MODE);
			if (set)
				kmscon_console_set_flags(vte->con,
						KMSCON_CONSOLE_INVERSE);
			else
				kmscon_console_reset_flags(vte->con,
						KMSCON_CONSOLE_INVERSE);
			continue;
		case 6: /* DECOM */
			set_reset_flag(vte, set, FLAG_ORIGIN_MODE);
			if (set)
				kmscon_console_set_flags(vte->con,
						KMSCON_CONSOLE_REL_ORIGIN);
			else
				kmscon_console_reset_flags(vte->con,
						KMSCON_CONSOLE_REL_ORIGIN);
			continue;
		case 7: /* DECAWN */
			set_reset_flag(vte, set, FLAG_AUTO_WRAP_MODE);
			if (set)
				kmscon_console_set_flags(vte->con,
						KMSCON_CONSOLE_AUTO_WRAP);
			else
				kmscon_console_reset_flags(vte->con,
						KMSCON_CONSOLE_AUTO_WRAP);
			continue;
		case 8: /* DECARM */
			set_reset_flag(vte, set, FLAG_AUTO_REPEAT_MODE);
			continue;
		case 18: /* DECPFF */
			/* If set, a form feed (FF) is sent to the printer after
			 * every screen that is printed. We don't have printers
			 * these days directly attached to terminals so we
			 * ignore this here. */
			continue;
		case 19: /* DECPEX */
			/* If set, the full screen is printed instead of
			 * scrolling region only. We have no printer so ignore
			 * this mode. */
			continue;
		case 25: /* DECTCEM */
			set_reset_flag(vte, set, FLAG_TEXT_CURSOR_MODE);
			if (set)
				kmscon_console_reset_flags(vte->con,
						KMSCON_CONSOLE_HIDE_CURSOR);
			else
				kmscon_console_set_flags(vte->con,
						KMSCON_CONSOLE_HIDE_CURSOR);
			continue;
		case 42: /* DECNRCM */
			set_reset_flag(vte, set, FLAG_NATIONAL_CHARSET_MODE);
			continue;
		default:
			log_debug("unknown DEC %set-Mode %d",
				  set?"S":"Res", vte->csi_argv[i]);
			continue;
		}
	}
}

static void csi_dev_attr(struct kmscon_vte *vte)
{
	if (vte->csi_argc <= 1 && vte->csi_argv[0] <= 0) {
		if (vte->csi_flags == 0) {
			send_primary_da(vte);
			return;
		} else if (vte->csi_flags & CSI_GT) {
			vte_write(vte, "\e[>1;1;0c", 9);
			return;
		}
	}

	log_debug("unhandled DA: %x %d %d %d...", vte->csi_flags,
		  vte->csi_argv[0], vte->csi_argv[1], vte->csi_argv[2]);
}

static void csi_dsr(struct kmscon_vte *vte)
{
	char buf[64];
	unsigned int x, y, len;

	if (vte->csi_argv[0] == 5) {
		vte_write(vte, "\e[0n", 4);
	} else if (vte->csi_argv[0] == 6) {
		x = kmscon_console_get_cursor_x(vte->con);
		y = kmscon_console_get_cursor_y(vte->con);
		len = snprintf(buf, sizeof(buf), "\e[%u;%uR", x, y);
		if (len >= sizeof(buf))
			vte_write(vte, "\e[0;0R", 6);
		else
			vte_write(vte, buf, len);
	}
}

static void do_csi(struct kmscon_vte *vte, uint32_t data)
{
	int num, x, y, upper, lower;
	bool protect;

	if (vte->csi_argc < CSI_ARG_MAX)
		vte->csi_argc++;

	switch (data) {
	case 'A': /* CUU */
		/* move cursor up */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_move_up(vte->con, num, false);
		break;
	case 'B': /* CUD */
		/* move cursor down */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_move_down(vte->con, num, false);
		break;
	case 'C': /* CUF */
		/* move cursor forward */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_move_right(vte->con, num);
		break;
	case 'D': /* CUB */
		/* move cursor backward */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_move_left(vte->con, num);
		break;
	case 'd': /* VPA */
		/* Vertical Line Position Absolute */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		x = kmscon_console_get_cursor_x(vte->con);
		kmscon_console_move_to(vte->con, x, num - 1);
		break;
	case 'e': /* VPR */
		/* Vertical Line Position Relative */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		x = kmscon_console_get_cursor_x(vte->con);
		y = kmscon_console_get_cursor_y(vte->con);
		kmscon_console_move_to(vte->con, x, y + num);
		break;
	case 'H': /* CUP */
	case 'f': /* HVP */
		/* position cursor */
		x = vte->csi_argv[0];
		if (x <= 0)
			x = 1;
		y = vte->csi_argv[1];
		if (y <= 0)
			y = 1;
		kmscon_console_move_to(vte->con, y - 1, x - 1);
		break;
	case 'G': /* CHA */
		/* Cursor Character Absolute */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		y = kmscon_console_get_cursor_y(vte->con);
		kmscon_console_move_to(vte->con, num - 1, y);
		break;
	case 'J':
		if (vte->csi_flags & CSI_WHAT)
			protect = true;
		else
			protect = false;

		if (vte->csi_argv[0] <= 0)
			kmscon_console_erase_cursor_to_screen(vte->con,
							      protect);
		else if (vte->csi_argv[0] == 1)
			kmscon_console_erase_screen_to_cursor(vte->con,
							      protect);
		else if (vte->csi_argv[0] == 2)
			kmscon_console_erase_screen(vte->con, protect);
		else
			log_debug("unknown parameter to CSI-J: %d",
				  vte->csi_argv[0]);
		break;
	case 'K':
		if (vte->csi_flags & CSI_WHAT)
			protect = true;
		else
			protect = false;

		if (vte->csi_argv[0] <= 0)
			kmscon_console_erase_cursor_to_end(vte->con, protect);
		else if (vte->csi_argv[0] == 1)
			kmscon_console_erase_home_to_cursor(vte->con, protect);
		else if (vte->csi_argv[0] == 2)
			kmscon_console_erase_current_line(vte->con, protect);
		else
			log_debug("unknown parameter to CSI-K: %d",
				  vte->csi_argv[0]);
		break;
	case 'X': /* ECH */
		/* erase characters */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_erase_chars(vte->con, num);
		break;
	case 'm':
		csi_attribute(vte);
		break;
	case 'p':
		if (vte->csi_flags & CSI_GT) {
			/* xterm: select X11 visual cursor mode */
			csi_soft_reset(vte);
		} else if (vte->csi_flags & CSI_BANG) {
			/* DECSTR: Soft Reset */
			csi_soft_reset(vte);
		} else if (vte->csi_flags & CSI_CASH) {
			/* DECRQM: Request DEC Private Mode */
			/* If CSI_WHAT is set, then enable,
			 * otherwise disable */
			csi_soft_reset(vte);
		} else {
			/* DECSCL: Compatibility Level */
			/* Sometimes CSI_DQUOTE is set here, too */
			csi_compat_mode(vte);
		}
		break;
	case 'h': /* SM: Set Mode */
		csi_mode(vte, true);
		break;
	case 'l': /* RM: Reset Mode */
		csi_mode(vte, false);
		break;
	case 'r': /* DECSTBM */
		/* set margin size */
		upper = vte->csi_argv[0];
		if (upper < 0)
			upper = 0;
		lower = vte->csi_argv[1];
		if (lower < 0)
			lower = 0;
		kmscon_console_set_margins(vte->con, upper, lower);
		break;
	case 'c': /* DA */
		/* device attributes */
		csi_dev_attr(vte);
		break;
	case 'L': /* IL */
		/* insert lines */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_insert_lines(vte->con, num);
		break;
	case 'M': /* DL */
		/* delete lines */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_delete_lines(vte->con, num);
		break;
	case 'g': /* TBC */
		/* tabulation clear */
		num = vte->csi_argv[0];
		if (num <= 0)
			kmscon_console_reset_tabstop(vte->con);
		else if (num == 3)
			kmscon_console_reset_all_tabstops(vte->con);
		else
			log_debug("invalid parameter %d to TBC CSI", num);
		break;
	case '@': /* ICH */
		/* insert characters */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_insert_chars(vte->con, num);
		break;
	case 'P': /* DCH */
		/* delete characters */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_delete_chars(vte->con, num);
		break;
	case 'Z': /* CBT */
		/* cursor horizontal backwards tab */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_tab_left(vte->con, num);
		break;
	case 'I': /* CHT */
		/* cursor horizontal forward tab */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_tab_right(vte->con, num);
		break;
	case 'n': /* DSR */
		/* device status reports */
		csi_dsr(vte);
		break;
	case 'S': /* SU */
		/* scroll up */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_scroll_up(vte->con, num);
		break;
	case 'T': /* SD */
		/* scroll down */
		num = vte->csi_argv[0];
		if (num <= 0)
			num = 1;
		kmscon_console_scroll_down(vte->con, num);
		break;
	default:
		log_debug("unhandled CSI sequence %c", data);
	}
}

/* map a character according to current GL and GR maps */
static uint32_t vte_map(struct kmscon_vte *vte, uint32_t val)
{
	/* 32, 127, 160 and 255 map to identity like all values >255 */
	switch (val) {
	case 33 ... 126:
		if (vte->glt) {
			val = (*vte->glt)[val - 32];
			vte->glt = NULL;
		} else {
			val = (*vte->gl)[val - 32];
		}
		break;
	case 161 ... 254:
		if (vte->grt) {
			val = (*vte->grt)[val - 160];
			vte->grt = NULL;
		} else {
			val = (*vte->gr)[val - 160];
		}
		break;
	}

	return val;
}

/* perform parser action */
static void do_action(struct kmscon_vte *vte, uint32_t data, int action)
{
	kmscon_symbol_t sym;

	switch (action) {
		case ACTION_NONE:
			/* do nothing */
			return;
		case ACTION_IGNORE:
			/* ignore character */
			break;
		case ACTION_PRINT:
			sym = kmscon_symbol_make(vte_map(vte, data));
			write_console(vte, sym);
			break;
		case ACTION_EXECUTE:
			do_execute(vte, data);
			break;
		case ACTION_CLEAR:
			do_clear(vte);
			break;
		case ACTION_COLLECT:
			do_collect(vte, data);
			break;
		case ACTION_PARAM:
			do_param(vte, data);
			break;
		case ACTION_ESC_DISPATCH:
			do_esc(vte, data);
			break;
		case ACTION_CSI_DISPATCH:
			do_csi(vte, data);
			break;
		case ACTION_DCS_START:
			break;
		case ACTION_DCS_COLLECT:
			break;
		case ACTION_DCS_END:
			break;
		case ACTION_OSC_START:
			break;
		case ACTION_OSC_COLLECT:
			break;
		case ACTION_OSC_END:
			break;
		default:
			log_warn("invalid action %d", action);
	}
}

/* entry actions to be performed when entering the selected state */
static const int entry_action[] = {
	[STATE_CSI_ENTRY] = ACTION_CLEAR,
	[STATE_DCS_ENTRY] = ACTION_CLEAR,
	[STATE_DCS_PASS] = ACTION_DCS_START,
	[STATE_ESC] = ACTION_CLEAR,
	[STATE_OSC_STRING] = ACTION_OSC_START,
	[STATE_NUM] = ACTION_NONE,
};

/* exit actions to be performed when leaving the selected state */
static const int exit_action[] = {
	[STATE_DCS_PASS] = ACTION_DCS_END,
	[STATE_OSC_STRING] = ACTION_OSC_END,
	[STATE_NUM] = ACTION_NONE,
};

/* perform state transision and dispatch related actions */
static void do_trans(struct kmscon_vte *vte, uint32_t data, int state, int act)
{
	if (state != STATE_NONE) {
		/* A state transition occurs. Perform exit-action,
		 * transition-action and entry-action. Even when performing a
		 * transition to the same state as the current state we do this.
		 * Use STATE_NONE if this is not the desired behavior.
		 */
		do_action(vte, data, exit_action[vte->state]);
		do_action(vte, data, act);
		do_action(vte, data, entry_action[state]);
		vte->state = state;
	} else {
		do_action(vte, data, act);
	}
}

/*
 * Escape sequence parser
 * This parses the new input character \data. It performs state transition and
 * calls the right callbacks for each action.
 */
static void parse_data(struct kmscon_vte *vte, uint32_t raw)
{
	/* events that may occur in any state */
	switch (raw) {
		case 0x18:
		case 0x1a:
		case 0x80 ... 0x8f:
		case 0x91 ... 0x97:
		case 0x99:
		case 0x9a:
		case 0x9c:
			do_trans(vte, raw, STATE_GROUND, ACTION_EXECUTE);
			return;
		case 0x1b:
			do_trans(vte, raw, STATE_ESC, ACTION_NONE);
			return;
		case 0x98:
		case 0x9e:
		case 0x9f:
			do_trans(vte, raw, STATE_ST_IGNORE, ACTION_NONE);
			return;
		case 0x90:
			do_trans(vte, raw, STATE_DCS_ENTRY, ACTION_NONE);
			return;
		case 0x9d:
			do_trans(vte, raw, STATE_OSC_STRING, ACTION_NONE);
			return;
		case 0x9b:
			do_trans(vte, raw, STATE_CSI_ENTRY, ACTION_NONE);
			return;
	}

	/* events that depend on the current state */
	switch (vte->state) {
	case STATE_GROUND:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x80 ... 0x8f:
		case 0x91 ... 0x9a:
		case 0x9c:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x20 ... 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_PRINT);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_PRINT);
		return;
	case STATE_ESC:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_ESC_INT, ACTION_COLLECT);
			return;
		case 0x30 ... 0x4f:
		case 0x51 ... 0x57:
		case 0x59:
		case 0x5a:
		case 0x5c:
		case 0x60 ... 0x7e:
			do_trans(vte, raw, STATE_GROUND, ACTION_ESC_DISPATCH);
			return;
		case 0x5b:
			do_trans(vte, raw, STATE_CSI_ENTRY, ACTION_NONE);
			return;
		case 0x5d:
			do_trans(vte, raw, STATE_OSC_STRING, ACTION_NONE);
			return;
		case 0x50:
			do_trans(vte, raw, STATE_DCS_ENTRY, ACTION_NONE);
			return;
		case 0x58:
		case 0x5e:
		case 0x5f:
			do_trans(vte, raw, STATE_ST_IGNORE, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_ESC_INT, ACTION_COLLECT);
		return;
	case STATE_ESC_INT:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_NONE, ACTION_COLLECT);
			return;
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x30 ... 0x7e:
			do_trans(vte, raw, STATE_GROUND, ACTION_ESC_DISPATCH);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_COLLECT);
		return;
	case STATE_CSI_ENTRY:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_CSI_INT, ACTION_COLLECT);
			return;
		case 0x3a:
			do_trans(vte, raw, STATE_CSI_IGNORE, ACTION_NONE);
			return;
		case 0x30 ... 0x39:
		case 0x3b:
			do_trans(vte, raw, STATE_CSI_PARAM, ACTION_PARAM);
			return;
		case 0x3c ... 0x3f:
			do_trans(vte, raw, STATE_CSI_PARAM, ACTION_COLLECT);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_GROUND, ACTION_CSI_DISPATCH);
			return;
		}
		do_trans(vte, raw, STATE_CSI_IGNORE, ACTION_NONE);
		return;
	case STATE_CSI_PARAM:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x30 ... 0x39:
		case 0x3b:
			do_trans(vte, raw, STATE_NONE, ACTION_PARAM);
			return;
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x3a:
		case 0x3c ... 0x3f:
			do_trans(vte, raw, STATE_CSI_IGNORE, ACTION_NONE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_CSI_INT, ACTION_COLLECT);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_GROUND, ACTION_CSI_DISPATCH);
			return;
		}
		do_trans(vte, raw, STATE_CSI_IGNORE, ACTION_NONE);
		return;
	case STATE_CSI_INT:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_NONE, ACTION_COLLECT);
			return;
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x30 ... 0x3f:
			do_trans(vte, raw, STATE_CSI_IGNORE, ACTION_NONE);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_GROUND, ACTION_CSI_DISPATCH);
			return;
		}
		do_trans(vte, raw, STATE_CSI_IGNORE, ACTION_NONE);
		return;
	case STATE_CSI_IGNORE:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_EXECUTE);
			return;
		case 0x20 ... 0x3f:
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_GROUND, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
		return;
	case STATE_DCS_ENTRY:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x3a:
			do_trans(vte, raw, STATE_DCS_IGNORE, ACTION_NONE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_DCS_INT, ACTION_COLLECT);
			return;
		case 0x30 ... 0x39:
		case 0x3b:
			do_trans(vte, raw, STATE_DCS_PARAM, ACTION_PARAM);
			return;
		case 0x3c ... 0x3f:
			do_trans(vte, raw, STATE_DCS_PARAM, ACTION_COLLECT);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_DCS_PASS, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_DCS_PASS, ACTION_NONE);
		return;
	case STATE_DCS_PARAM:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x30 ... 0x39:
		case 0x3b:
			do_trans(vte, raw, STATE_NONE, ACTION_PARAM);
			return;
		case 0x3a:
		case 0x3c ... 0x3f:
			do_trans(vte, raw, STATE_DCS_IGNORE, ACTION_NONE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_DCS_INT, ACTION_COLLECT);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_DCS_PASS, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_DCS_PASS, ACTION_NONE);
		return;
	case STATE_DCS_INT:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x20 ... 0x2f:
			do_trans(vte, raw, STATE_NONE, ACTION_COLLECT);
			return;
		case 0x30 ... 0x3f:
			do_trans(vte, raw, STATE_DCS_IGNORE, ACTION_NONE);
			return;
		case 0x40 ... 0x7e:
			do_trans(vte, raw, STATE_DCS_PASS, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_DCS_PASS, ACTION_NONE);
		return;
	case STATE_DCS_PASS:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x20 ... 0x7e:
			do_trans(vte, raw, STATE_NONE, ACTION_DCS_COLLECT);
			return;
		case 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x9c:
			do_trans(vte, raw, STATE_GROUND, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_DCS_COLLECT);
		return;
	case STATE_DCS_IGNORE:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x20 ... 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x9c:
			do_trans(vte, raw, STATE_GROUND, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
		return;
	case STATE_OSC_STRING:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x20 ... 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_OSC_COLLECT);
			return;
		case 0x9c:
			do_trans(vte, raw, STATE_GROUND, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_OSC_COLLECT);
		return;
	case STATE_ST_IGNORE:
		switch (raw) {
		case 0x00 ... 0x17:
		case 0x19:
		case 0x1c ... 0x1f:
		case 0x20 ... 0x7f:
			do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
			return;
		case 0x9c:
			do_trans(vte, raw, STATE_GROUND, ACTION_NONE);
			return;
		}
		do_trans(vte, raw, STATE_NONE, ACTION_IGNORE);
		return;
	}

	log_warn("unhandled input %u in state %d", raw, vte->state);
}

void kmscon_vte_input(struct kmscon_vte *vte, const char *u8, size_t len)
{
	int state;
	uint32_t ucs4;
	size_t i;

	if (!vte || !vte->con)
		return;

	++vte->parse_cnt;
	for (i = 0; i < len; ++i) {
		if (vte->flags & FLAG_7BIT_MODE) {
			if (u8[i] & 0x80)
				log_debug("receiving 8bit character U+%d from pty while in 7bit mode",
					  (int)u8[i]);
			parse_data(vte, u8[i] & 0x7f);
		} else if (vte->flags & FLAG_8BIT_MODE) {
			parse_data(vte, u8[i]);
		} else {
			state = kmscon_utf8_mach_feed(vte->mach, u8[i]);
			if (state == KMSCON_UTF8_ACCEPT ||
			    state == KMSCON_UTF8_REJECT) {
				ucs4 = kmscon_utf8_mach_get(vte->mach);
				parse_data(vte, ucs4);
			}
		}
	}
	--vte->parse_cnt;
}

bool kmscon_vte_handle_keyboard(struct kmscon_vte *vte,
				const struct uterm_input_event *ev)
{
	kmscon_symbol_t sym;
	char val;
	size_t len;
	const char *u8;

	if (UTERM_INPUT_HAS_MODS(ev, UTERM_CONTROL_MASK)) {
		switch (ev->keysym) {
		case XK_2:
		case XK_space:
			vte_write(vte, "\x00", 1);
			return true;
		case XK_a:
		case XK_A:
			vte_write(vte, "\x01", 1);
			return true;
		case XK_b:
		case XK_B:
			vte_write(vte, "\x02", 1);
			return true;
		case XK_c:
		case XK_C:
			vte_write(vte, "\x03", 1);
			return true;
		case XK_d:
		case XK_D:
			vte_write(vte, "\x04", 1);
			return true;
		case XK_e:
		case XK_E:
			vte_write(vte, "\x05", 1);
			return true;
		case XK_f:
		case XK_F:
			vte_write(vte, "\x06", 1);
			return true;
		case XK_g:
		case XK_G:
			vte_write(vte, "\x07", 1);
			return true;
		case XK_h:
		case XK_H:
			vte_write(vte, "\x08", 1);
			return true;
		case XK_i:
		case XK_I:
			vte_write(vte, "\x09", 1);
			return true;
		case XK_j:
		case XK_J:
			vte_write(vte, "\x0a", 1);
			return true;
		case XK_k:
		case XK_K:
			vte_write(vte, "\x0b", 1);
			return true;
		case XK_l:
		case XK_L:
			vte_write(vte, "\x0c", 1);
			return true;
		case XK_m:
		case XK_M:
			vte_write(vte, "\x0d", 1);
			return true;
		case XK_n:
		case XK_N:
			vte_write(vte, "\x0e", 1);
			return true;
		case XK_o:
		case XK_O:
			vte_write(vte, "\x0f", 1);
			return true;
		case XK_p:
		case XK_P:
			vte_write(vte, "\x10", 1);
			return true;
		case XK_q:
		case XK_Q:
			vte_write(vte, "\x11", 1);
			return true;
		case XK_r:
		case XK_R:
			vte_write(vte, "\x12", 1);
			return true;
		case XK_s:
		case XK_S:
			vte_write(vte, "\x13", 1);
			return true;
		case XK_t:
		case XK_T:
			vte_write(vte, "\x14", 1);
			return true;
		case XK_u:
		case XK_U:
			vte_write(vte, "\x15", 1);
			return true;
		case XK_v:
		case XK_V:
			vte_write(vte, "\x16", 1);
			return true;
		case XK_w:
		case XK_W:
			vte_write(vte, "\x17", 1);
			return true;
		case XK_x:
		case XK_X:
			vte_write(vte, "\x18", 1);
			return true;
		case XK_y:
		case XK_Y:
			vte_write(vte, "\x19", 1);
			return true;
		case XK_z:
		case XK_Z:
			vte_write(vte, "\x1a", 1);
			return true;
		case XK_3:
		case XK_bracketleft:
		case XK_braceleft:
			vte_write(vte, "\x1b", 1);
			return true;
		case XK_4:
		case XK_backslash:
		case XK_bar:
			vte_write(vte, "\x1c", 1);
			return true;
		case XK_5:
		case XK_bracketright:
		case XK_braceright:
			vte_write(vte, "\x1d", 1);
			return true;
		case XK_6:
		case XK_grave:
		case XK_asciitilde:
			vte_write(vte, "\x1e", 1);
			return true;
		case XK_7:
		case XK_slash:
		case XK_question:
			vte_write(vte, "\x1f", 1);
			return true;
		case XK_8:
			vte_write(vte, "\x7f", 1);
			return true;
		}
	}

	switch (ev->keysym) {
		case XK_BackSpace:
			vte_write(vte, "\x08", 1);
			return true;
		case XK_Tab:
		case XK_KP_Tab:
			vte_write(vte, "\x09", 1);
			return true;
		case XK_Linefeed:
			vte_write(vte, "\x0a", 1);
			return true;
		case XK_Clear:
			vte_write(vte, "\x0b", 1);
			return true;
		case XK_Pause:
			vte_write(vte, "\x13", 1);
			return true;
		case XK_Scroll_Lock:
			/* TODO: do we need scroll lock impl.? */
			vte_write(vte, "\x14", 1);
			return true;
		case XK_Sys_Req:
			vte_write(vte, "\x15", 1);
			return true;
		case XK_Escape:
			vte_write(vte, "\x1b", 1);
			return true;
		case XK_KP_Enter:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE) {
				vte_write(vte, "\eOM", 3);
				return true;
			}
			/* fallthrough */
		case XK_Return:
			if (vte->flags & FLAG_LINE_FEED_NEW_LINE_MODE)
				vte_write(vte, "\x0d\x0a", 2);
			else
				vte_write(vte, "\x0d", 1);
			return true;
		case XK_Find:
			vte_write(vte, "\e[1~", 4);
			return true;
		case XK_Insert:
			vte_write(vte, "\e[2~", 4);
			return true;
		case XK_Delete:
			vte_write(vte, "\e[3~", 4);
			return true;
		case XK_Select:
			vte_write(vte, "\e[4~", 4);
			return true;
		case XK_Page_Up:
			vte_write(vte, "\e[5~", 4);
			return true;
		case XK_Page_Down:
			vte_write(vte, "\e[6~", 4);
			return true;
		case XK_Up:
			if (vte->flags & FLAG_CURSOR_KEY_MODE)
				vte_write(vte, "\eOA", 3);
			else
				vte_write(vte, "\e[A", 3);
			return true;
		case XK_Down:
			if (vte->flags & FLAG_CURSOR_KEY_MODE)
				vte_write(vte, "\eOB", 3);
			else
				vte_write(vte, "\e[B", 3);
			return true;
		case XK_Right:
			if (vte->flags & FLAG_CURSOR_KEY_MODE)
				vte_write(vte, "\eOC", 3);
			else
				vte_write(vte, "\e[C", 3);
			return true;
		case XK_Left:
			if (vte->flags & FLAG_CURSOR_KEY_MODE)
				vte_write(vte, "\eOD", 3);
			else
				vte_write(vte, "\e[D", 3);
			return true;
		case XK_KP_Insert:
		case XK_KP_0:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOp", 3);
			else
				vte_write(vte, "0", 1);
			return true;
		case XK_KP_End:
		case XK_KP_1:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOq", 3);
			else
				vte_write(vte, "1", 1);
			return true;
		case XK_KP_Down:
		case XK_KP_2:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOr", 3);
			else
				vte_write(vte, "2", 1);
			return true;
		case XK_KP_Page_Down:
		case XK_KP_3:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOs", 3);
			else
				vte_write(vte, "3", 1);
			return true;
		case XK_KP_Left:
		case XK_KP_4:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOt", 3);
			else
				vte_write(vte, "4", 1);
			return true;
		case XK_KP_Begin:
		case XK_KP_5:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOu", 3);
			else
				vte_write(vte, "5", 1);
			return true;
		case XK_KP_Right:
		case XK_KP_6:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOv", 3);
			else
				vte_write(vte, "6", 1);
			return true;
		case XK_KP_Home:
		case XK_KP_7:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOw", 3);
			else
				vte_write(vte, "7", 1);
			return true;
		case XK_KP_Up:
		case XK_KP_8:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOx", 3);
			else
				vte_write(vte, "8", 1);
			return true;
		case XK_KP_Page_Up:
		case XK_KP_9:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOy", 3);
			else
				vte_write(vte, "9", 1);
			return true;
		case XK_KP_Subtract:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOm", 3);
			else
				vte_write(vte, "-", 1);
			return true;
		case XK_KP_Separator:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOl", 3);
			else
				vte_write(vte, ",", 1);
			return true;
		case XK_KP_Delete:
		case XK_KP_Decimal:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOn", 3);
			else
				vte_write(vte, ".", 1);
			return true;
		case XK_KP_Equal:
		case XK_KP_Divide:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOj", 3);
			else
				vte_write(vte, "/", 1);
			return true;
		case XK_KP_Multiply:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOo", 3);
			else
				vte_write(vte, "*", 1);
			return true;
		case XK_KP_Add:
			if (vte->flags & FLAG_KEYPAD_APPLICATION_MODE)
				vte_write(vte, "\eOk", 3);
			else
				vte_write(vte, "+", 1);
			return true;
		case XK_F1:
		case XK_KP_F1:
			vte_write(vte, "\eOP", 3);
			return true;
		case XK_F2:
		case XK_KP_F2:
			vte_write(vte, "\eOQ", 3);
			return true;
		case XK_F3:
		case XK_KP_F3:
			vte_write(vte, "\eOR", 3);
			return true;
		case XK_F4:
		case XK_KP_F4:
			vte_write(vte, "\eOS", 3);
			return true;
		case XK_KP_Space:
			vte_write(vte, " ", 1);
			return true;
		case XK_Home:
			if (vte->flags & FLAG_CURSOR_KEY_MODE)
				vte_write(vte, "\eOH", 3);
			else
				vte_write(vte, "\e[H", 3);
			return true;
		case XK_End:
			if (vte->flags & FLAG_CURSOR_KEY_MODE)
				vte_write(vte, "\eOF", 3);
			else
				vte_write(vte, "\e[F", 3);
			return true;
		case XK_F5:
			vte_write(vte, "\e[15~", 5);
			return true;
		case XK_F6:
			vte_write(vte, "\e[17~", 5);
			return true;
		case XK_F7:
			vte_write(vte, "\e[18~", 5);
			return true;
		case XK_F8:
			vte_write(vte, "\e[19~", 5);
			return true;
		case XK_F9:
			vte_write(vte, "\e[20~", 5);
			return true;
		case XK_F10:
			vte_write(vte, "\e[21~", 5);
			return true;
		case XK_F11:
			vte_write(vte, "\e[23~", 5);
			return true;
		case XK_F12:
			vte_write(vte, "\e[24~", 5);
			return true;
		case XK_F13:
			vte_write(vte, "\e[25~", 5);
			return true;
		case XK_F14:
			vte_write(vte, "\e[26~", 5);
			return true;
		case XK_F15:
			vte_write(vte, "\e[28~", 5);
			return true;
		case XK_F16:
			vte_write(vte, "\e[29~", 5);
			return true;
		case XK_F17:
			vte_write(vte, "\e[31~", 5);
			return true;
		case XK_F18:
			vte_write(vte, "\e[32~", 5);
			return true;
		case XK_F19:
			vte_write(vte, "\e[33~", 5);
			return true;
		case XK_F20:
			vte_write(vte, "\e[34~", 5);
			return true;
	}

	if (ev->unicode != UTERM_INPUT_INVALID) {
		if (vte->flags & FLAG_7BIT_MODE) {
			val = ev->unicode;
			if (ev->unicode & 0x80) {
				log_debug("invalid keyboard input in 7bit mode U+%x; mapping to '?'", ev->unicode);
				val = '?';
			}
			vte_write(vte, &val, 1);
		} else if (vte->flags & FLAG_8BIT_MODE) {
			val = ev->unicode;
			if (ev->unicode > 0xff) {
				log_debug("invalid keyboard input in 8bit mode U+%x; mapping to '?'", ev->unicode);
				val = '?';
			}
			vte_write_raw(vte, &val, 1);
		} else {
			sym = kmscon_symbol_make(ev->unicode);
			u8 = kmscon_symbol_get_u8(sym, &len);
			vte_write_raw(vte, u8, len);
			kmscon_symbol_free_u8(u8);
		}
		return true;
	}

	return false;
}
