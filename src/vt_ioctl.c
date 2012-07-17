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


const char * cmdname( int cmd)
{
	switch (cmd) {
		case TCGETS:
			return "TCGETS";
			break;
		case TCSETSW:
			return "TCSETSW";
		default:
			break;
	}

	return "unknow";
}


void tty_ioctl(fuse_req_t req, int cmd, void *arg,
			  struct fuse_file_info *fi, unsigned flags,
			  const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	static pid_t control_pid = 0;
	static struct termios termios;

	if(flags & FUSE_IOCTL_COMPAT){
		fuse_reply_err(req,ENOSYS);
		return;
	}

	printf("handled cmd=0x%x (%s) . arg=%p\n",cmd, cmdname(cmd) , arg);


	switch(cmd){
	/*noop ioctls be here*/
	case TCFLSH:
	case TCXONC:
	case TCSBRK:
		fuse_reply_ioctl(req, 0, 0,0);
		break;

	case TCSBRK:
		fuse_reply_err(req, EINVAL);
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
			memcpy(&termios, in_buf, in_bufsz);
			fuse_reply_ioctl(req, 0, 0, 0);
		}
		break;
	case TCGETS:
		fuse_reply_ioctl(req,0,0,0);
		break;

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
			fuse_reply_ioctl(req, 0, &control_pid, sizeof(pid_t));
		}
		break;

	case TIOCSCTTY:
	{
		control_pid = fuse_req_ctx(req)->pid;
		fuse_reply_ioctl(req,0,0,0);
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
			fuse_reply_ioctl(req, 0, &control_pid, sizeof(pid_t));
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
			control_pid = *(pid_t*)in_buf;
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


	case TIOCMGET:
		fuse_reply_err(req,EINVAL);
		break;

	default:
		fuse_reply_err(req,ENOSYS);
		return ;
	}
}
