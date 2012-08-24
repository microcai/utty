#include <stdio.h>
#include <stdlib.h>
#include <fuse/cuse_lowlevel.h>
#include <fuse/fuse_common.h>
#include <fuse/fuse.h>
#include <pthread.h>
#include <termio.h>
#include <errno.h>
#include <linux/kd.h>

#include "vte/vte.h"
#include "tty/tty.h"

#define _fi_tty(fi) (fi->fh)
#define fi_tty(fi)	((struct tty*)_fi_tty(fi))

#include "cuseioctl.c.h"

static void tty_poll(fuse_req_t req, struct fuse_file_info *fi,struct fuse_pollhandle *ph)
{
//	sleep(1);

//	fuse_reply_poll(req,POLL_);

}

static void tty_open(fuse_req_t req, struct fuse_file_info *fi)
{
	_fi_tty(fi) = (uint64_t)fuse_req_userdata(req);
	fuse_reply_open(req, fi);
}

static void tty_read(fuse_req_t req, size_t size, off_t off,struct fuse_file_info *fi)
{

}


static void tty_write(fuse_req_t req, const char *buf, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	struct chunk chunk;

	chunk.tty = fi_tty(fi);

	chunk.size = size;
	chunk.data = buf;

	fi_tty(fi)->vte->feed(fi_tty(fi)->vte,&chunk);

	fuse_reply_write(req,size);
}

static void tty_release(fuse_req_t req, struct fuse_file_info *fi)
{
//	struct console * vt = (void*)fi->fh;

	//no more references
//	vt->control_pid = -1 ; //

	fuse_reply_err(req,0);
}

static void * cuse_loop(void * threadparam)
{
	struct tty * tty = threadparam;

	struct fuse_session * fuse = tty->private;

	fuse_session_loop(fuse);

	pthread_exit(0);
}

static const struct cuse_lowlevel_ops tty_clop = {
	.open		= tty_open,
	.read		= tty_read,
	.write		= tty_write,
	.ioctl		= tty_ioctl,
	.release	= tty_release,
	.poll		= tty_poll,
};


struct tty * tty_cuse_new()
{
	static int device_index = 0;

	char devname[24]={0};
	struct fuse_args args={0};

	struct tty * tty =  calloc(1,sizeof(struct tty));
	// setup cuse

	snprintf(devname,sizeof devname,"DEVNAME=utty%d",device_index++);

	const char *dev_info_argv[] = {devname ,"MODE=0666" };
	struct cuse_info ci={0};

	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	fuse_opt_add_arg(&args, "utty");
	fuse_opt_add_arg(&args, "-f");

	int no=1;

	struct fuse_session * session = cuse_lowlevel_setup(args.argc,args.argv, &ci, &tty_clop, &no,tty);

	tty->private = session;

	pthread_create(&tty->thread,NULL,cuse_loop,tty);
	return tty;
}
