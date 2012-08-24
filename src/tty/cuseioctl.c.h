static const char * cmdname( int cmd)
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
		case TIOCSPGRP:
			return "TIOCSPGRP";
		case KDGKBMODE:
			return "KDGKBMODE";
		case KDGKBLED:
			return "KDGKBLED";
		case TCSETS:
			return "TCSETS";
		case TIOCGWINSZ:
			return "TIOCGWINSZ";
		case TIOCSWINSZ:
			return "TIOCSWINSZ";
		default:
			break;
	}

	return "unknow";
}

static void tty_ioctl(fuse_req_t req, int cmd, void *arg,
			  struct fuse_file_info *fi, unsigned flags,
			  const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	switch (cmd) {
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
	case KDGKBLED:
		fuse_reply_err(req,ENOSYS);
		break;
	case TIOCGWINSZ:
		if (!out_bufsz)
		{
			struct iovec iov =
			{ arg, sizeof(struct winsize) };
			fuse_reply_ioctl_retry(req, NULL, 0, &iov, 1);
		}
		else
		{
			struct winsize size;
//			fi_vte(fi)->con->screen->getwindowsize
//			vte_get_window_size(vt,&size);
			fuse_reply_ioctl(req, 0, &size, sizeof(size));
		}
		break;
	default:
		fprintf(stderr,"unhandled ioctl cmd=0x%x (%s) . arg=%p as ENOSYS\n",cmd, cmdname(cmd) , arg);
		fuse_reply_err(req,ENOSYS);
		break;
	}
	printf("handled cmd=0x%x (%s) . arg=%p \n",cmd, cmdname(cmd),arg);
}
