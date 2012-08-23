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

}
