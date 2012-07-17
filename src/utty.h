
#ifndef UTTY_H_
#define UTTY_H_

void tty_ioctl(fuse_req_t req, int cmd, void *arg, struct fuse_file_info *fi,
		unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);

#endif /* UTTY_H_ */
