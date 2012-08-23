/*
 * vt.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#ifndef VT_H_
#define VT_H_

#include <glib.h>
#include <termio.h>
#include <SDL/SDL_events.h>
#include <fuse_lowlevel.h>

/* Struct to process the read request or poll*/
struct io_request
{
	fuse_req_t req;
	size_t size;
	off_t off;
	struct fuse_file_info *fi;

	int		revent;

};


struct console{
	gunichar	* screen_buffer_glyph;
	guint64	* screen_buffer_attr; // 24bit for color, 8 bit for attrib
	gint		current_pos;
	gint		char_pixelsize; // char size in pixel, 8x16 font is 16

	guint64	cur_attr; // current fg bg color, and attribute

	GRecMutex		lock;
	GQueue readers ;

	GQueue keycode_buffer;

	pid_t control_pid;

	pid_t session_id;

	struct termios termios;

	/*
	 * flags for DECODE ESCAPE SEQUENCE
	 */


	guint64	flags_esc:1;
	guint64	flags_csi:1;

};


void init_vt(int argc, char **argv);
void tty_ioctl(fuse_req_t req, int cmd, void *arg, struct fuse_file_info *fi,unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);

void tty_notify_read(struct io_request * req,gchar * buffer,glong count);

void vte_notify_keypress(struct console * vt,SDL_KeyboardEvent * key);

void vte_attach_reader(struct console * vt , struct io_request * request );

void vte_notify_write(struct console * vt, gunichar chars[], glong count);
void vte_get_window_size(struct console * vt ,struct winsize*);

#endif /* VT_H_ */
