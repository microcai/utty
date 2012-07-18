/*
 * vt.c
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#include <glib.h>

#include <fuse/cuse_lowlevel.h>
#include <fuse/fuse.h>
#include <SDL/SDL_thread.h>

#include "utty.h"
#include "console.h"
#include "vt.h"


static void tty_poll(fuse_req_t req, struct fuse_file_info *fi,struct fuse_pollhandle *ph)
{
//	sleep(1);

//	fuse_reply_poll(req,POLL_);

}

static void tty_open(fuse_req_t req, struct fuse_file_info *fi)
{
	fi->fh = (uint64_t)fuse_req_userdata(req);
	fuse_reply_open(req, fi);
}

void tty_notify_read(struct io_request * req,gchar * buffer,glong count)
{
	fuse_reply_buf(req->req,buffer,count);
	g_free(req);
}

static void tty_read(fuse_req_t req, size_t size, off_t off,
			 struct fuse_file_info *fi)
{
//	fuse_reply_buf(req,"hello",5);
	//make request
	struct io_request * request = g_new0(struct io_request,1);

	request->fi = fi;
	request->off = off;
	request->req = req;
	request->revent = POLL_IN;
	request->size = size;

	// attach to the readclient line
	console_vt_attach_reader((struct console *)(fi->fh),request);
}


static void tty_write(fuse_req_t req, const char *buf, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	glong written;

	fuse_reply_write(req,size);

	// convert to ucs char

	gunichar * chars = g_utf8_to_ucs4_fast(buf,size,&written);

	// put it on-screen buffer
	console_vt_notify_write((struct console*)fi->fh, chars,written);
}

static void tty_release(fuse_req_t req, struct fuse_file_info *fi)
{
	struct console * vt = (void*)fi->fh;

	//no more references
	vt->control_pid = -1 ; //

	fuse_reply_err(req,0);
}

static const struct cuse_lowlevel_ops tty_clop = {
	.open		= tty_open,
	.read		= tty_read,
	.write		= tty_write,
	.ioctl		= tty_ioctl,
	.release	= tty_release,
	.poll		= tty_poll,
};



struct fuse_session * device_init(int vt_index)
{
	struct fuse_args args={0};

	const char *dev_info_argv[] = { "DEVNAME=utty","MODE=0666" };
	struct cuse_info ci={0};

	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	fuse_opt_add_arg(&args, "utty");
	fuse_opt_add_arg(&args, "-f");



	int no=0;

	return  cuse_lowlevel_setup(args.argc,args.argv, &ci, &tty_clop, &no ,console_direct_get_vt(vt_index));

}

int cuse_loop( void * threadparam)
{
	struct fuse_session * fuse = threadparam;

	fuse_session_loop(fuse);

	exit(0);
}


void init_vt(int argc, char **argv)
{
	SDL_CreateThread(cuse_loop, device_init(0));
}
