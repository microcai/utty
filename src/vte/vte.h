#pragma  once

#include <pthread.h>

#include <common.h>

struct input_event;
struct console;

struct chunk {
	struct vte* vte;
	size_t	size;
	const void	*data;
};

struct vte{
	pthread_mutex_t	 lock;

	struct vte * next ; // next VTE

	struct console * con; // upper layer console

	/* called by console layer, notify the VTE that user press a key*/
	void (*process_key)(struct vte*,struct input_event*);

	/* called when there is data writted to the program
	 * example, in line edit mode, if user press enter, then current
	 * line will be emit to the program*/
	void (*emit)(struct vte*, struct chunk * chunk );

	/* called when program write some data to VTE*/
	void (*feed)(struct vte*, struct chunk * chunk);

	/* screen buffers  */
	struct font_char_attr	*screenbuf;
	unsigned	int			sizepreline;
	unsigned	int			rows,cols;
	unsigned	int			pos; // current pos

	union color f;	// current fg color
	union color b;	// current bg color

	/* called to resize, called at least once on attach*/
	void (*resize)(struct vte*,int cols,int rows);

	/* clear screen */
	void (*clear)(struct vte*);

	/* the thread that do IO operate CUSE or PTY*/
	pthread_t thread;
	void *private;

};

#define vte_lock(vte)		do{ pthread_mutex_lock(&vte->lock);  }while(0)
#define vte_unlock(vte)	do{ pthread_mutex_unlock(&vte->lock); }while(0)


struct vte * vte_new()__attribute__((visibility("hidden")));
struct vte * vte_cuse_new()__attribute__((visibility("hidden")));
