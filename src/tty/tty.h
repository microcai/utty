/*
 * tty.h
 *
 *  Created on: 2012-8-24
 *      Author: cai
 */

#pragma once

#include <stdlib.h>
#include <pthread.h>

struct tty;
struct vte;

struct chunk {
	struct tty* tty;
	size_t	size;
	const void	*data;
};


struct tty{
	struct vte * vte;
	/* the thread that do IO operate CUSE or PTY*/
	pthread_t thread;
	void * private;
};


struct tty * tty_cuse_new();
