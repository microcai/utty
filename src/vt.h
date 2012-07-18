/*
 * vt.h
 *
 *  Created on: 2012-7-17
 *      Author: cai
 */

#ifndef VT_H_
#define VT_H_

void init_vt(int argc, char **argv);
void tty_ioctl(fuse_req_t req, int cmd, void *arg, struct fuse_file_info *fi,unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);

void tty_notify_read(struct io_request * req,gchar * buffer,glong count);


#endif /* VT_H_ */
