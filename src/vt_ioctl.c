/*
 * ioctl.c
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */
#include <stdio.h>
#include <memory.h>
#include <fuse/cuse_lowlevel.h>
#include <fuse.h>
#include <termio.h>
#include <errno.h>

#include "kernel.h"
#include "utty.h"
#include <console.h>

const char * cmdname( int cmd)
{
	switch (cmd) {
		case TCGETS:
			return "TCGETS";
			break;
		case TCSETSF:
			return "TCSETSF";
		case TCSETSW:
			return "TCSETSW";
		case TIOCGSID:
			return "TIOCGSID";
		case TIOCGPGRP:
			return "TIOCGPGRP";
		default:
			break;
	}

	return "unknow";
}


void tty_ioctl(fuse_req_t req, int cmd, void *arg,
			  struct fuse_file_info *fi, unsigned flags,
			  const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	struct console * vt = (void*)fi->fh;

	if(flags & FUSE_IOCTL_COMPAT){
		fuse_reply_err(req,ENOSYS);
		return;
	}

	switch(cmd){
	case TIOCSCTTY:
		((struct console*)(fi->fh))->control_pid = fuse_req_ctx(req)->pid;
		((struct console*)(fi->fh))->session_id =  fuse_req_ctx(req)->pid;
		fuse_reply_ioctl(req,0,0,0);
	/*noop ioctls be here*/
	case TCFLSH:
	case TCXONC:
	case TCSBRK:
		fuse_reply_ioctl(req, 0, 0,0);
		break;

	case TCSETSF:
	case TCSETS:
	case TCSETSW:
		if (!in_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(struct termios) };
			fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0);
		}
		else
		{
			memcpy(&vt->termios, in_buf, in_bufsz);
			fuse_reply_ioctl(req, 0, 0, 0);
		}
		break;
	case TCGETS:
		if(out_bufsz){
			fuse_reply_ioctl(req, 0,&vt->termios ,52);
		}else{
			struct iovec iov =
			{ arg, sizeof(struct termios) };
			fuse_reply_ioctl_retry(req , NULL, 0, &iov, 1);
		}
		break;
	case TIOCGSID:
		if (!out_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(pid_t) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		}
		else
		{
			fuse_reply_ioctl(req, 0, &(((struct console*)(fi->fh))->session_id), sizeof(pid_t));
		}
		break;
	case TIOCGPGRP:
	{
		if (!out_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(pid_t) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		}
		else
		{
			fuse_reply_ioctl(req, 0, &(((struct console*)(fi->fh))->control_pid), sizeof(pid_t));
		}
	}
	break;
	case TIOCSPGRP:
	{
		if (!in_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(pid_t) };
			fuse_reply_ioctl_retry(req,  &iov, 1, NULL, 0);
		}
		else
		{
			((struct console*)(fi->fh))->control_pid = *(pid_t*)in_buf;
			fuse_reply_ioctl(req, 0, 0,0);
		}
	}
	break;

	case KDGKBMODE: // kernel keyborad mode
		if (!out_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(unsigned int) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		}
		else
		{
			unsigned int mode = K_UNICODE;
			fuse_reply_ioctl(req, 0, &mode, sizeof(unsigned int));
		}
		break;
	case TIOCGWINSZ: // get window size

		if (!out_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(struct winsize) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		}
		else
		{
			struct winsize size;
			console_vt_get_window_size(vt,&size);
			fuse_reply_ioctl(req, 0, &size, sizeof(size));
		}
		break;
	case TIOCSWINSZ:
	case TIOCMGET:
		fuse_reply_err(req,EINVAL);
		break;

	default:
		printf("handled cmd=0x%x (%s) . arg=%p as ENOSYS\n",cmd, cmdname(cmd) , arg);
		fuse_reply_err(req,ENOSYS);
		return ;
	}
}
