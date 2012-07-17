
#ifndef UTTY_H_
#define UTTY_H_

#include <fuse/fuse_lowlevel.h>


/* Struct to process the read request or poll*/
struct io_request
{
	fuse_req_t req;
	size_t size;
	off_t off;
	struct fuse_file_info *fi;

	int		revent;

};

void utty_force_expose();


#endif /* UTTY_H_ */
