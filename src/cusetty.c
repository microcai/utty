/*
 ============================================================================
 Name        : cusetty.c
 Author      : microcai
 Version     :
 Copyright   : MIT
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fuse/cuse_lowlevel.h>
#include <SDL/SDL.h>
#include <SDL/SDL_video.h>
#include <glib.h>
#include <sys/errno.h>
#include <ft2build.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <termio.h>
#include <poll.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define KDGKBMODE 0x4B44  /* gets current keyboard mode */

#define           K_UNICODE       0x03

/* Struct to process the read request or poll*/
struct io_request
{
	fuse_req_t req;
	size_t size;
	off_t off;
	struct fuse_file_info *fi;

	int		revent;

};


static struct fuse_session * device_init();
static int cuse_loop( void * threadparam);
static void setup_display();

static void screen_nextline();

FT_Library ftlib;
FT_Face ftface;
int screen_width, screen_rows;

SDL_mutex	* mutex;
GQueue 	readers;

// read clients
gunichar *screenbuf;
int	  pos_Current;

static void init_irq()
{
	mutex = SDL_CreateMutex();
	g_queue_init(&readers);
}


void irq_kbd(SDL_Event * event)
{
	SDL_LockMutex(mutex);

	struct io_request * request = g_queue_pop_head(&readers);
	SDL_UnlockMutex(mutex);

	if(!request)
	{
		return ;
	}

	glong written;
	glong readed;

	if (event->key.keysym.unicode == '\r')
	{
		event->key.keysym.unicode = '\n';
		screen_nextline();
	}

	char * keystok = g_utf16_to_utf8(&event->key.keysym.unicode,1,&readed,&written,0);

	if(request->size < written){
		fuse_reply_err(request->req,EAGAIN);
	}else{
		fuse_reply_buf(request->req,keystok,written);
	}
	g_free(keystok);
	g_free(request);


}

static void tty_read(fuse_req_t req, size_t size, off_t off,
			 struct fuse_file_info *fi)
{
//	fuse_reply_buf(req,"hello",5);
	//make request
	struct io_request * request = g_new0(struct io_request,1);

	request->fi = fi;
	request->off = off;
	request->req = req;
	request->revent = POLL_IN;
	request->size = size;

	// attach to the readclient line
	SDL_LockMutex(mutex);
	g_queue_push_tail(&readers,request);
	SDL_UnlockMutex(mutex);
}


void screen_wrap()
{
	if(pos_Current >= screen_width*screen_rows) // move out of screen, scroll one line
	{
		memmove(screenbuf,screenbuf + screen_width, screen_width*(screen_rows-1)*sizeof(gunichar));
		memset(screenbuf + screen_width*(screen_rows-1) , 0 ,screen_width*sizeof(gunichar));


		pos_Current -= screen_width;
	}
}

static void screen_nextline()
{
	// move to new line, if possible, scroll the buffer
	pos_Current = ((pos_Current / screen_width) +1 )* screen_width;

	screen_wrap();
}

void screen_write_char(gunichar  c)
{
	screenbuf[pos_Current] = c;
	pos_Current++;

	screen_wrap();
}



void screen_write(gunichar * chars, glong written)
{
	for(int i=0; i< written; i++)
	{
		switch(chars[i])
		{
		case '\n':
			screen_nextline();
			continue;
		}

		screen_write_char(chars[i]);
		if(	g_unichar_iswide(chars[i]))
			screen_write_char(chars[i]);
	}
}

static void tty_write(fuse_req_t req, const char *buf, size_t size,
			  off_t off, struct fuse_file_info *fi)
{
	glong written;

	fuse_reply_write(req,size);

	printf("we got %s",buf);

	// convert to ucs char

	gunichar * chars = g_utf8_to_ucs4_fast(buf,size,&written);

	// put it on-screen buffer
	screen_write(chars,written);

	SDL_Event event={0};
	event.type = SDL_USEREVENT;
	event.user.code = 100;

	//redraw the screen
	SDL_PushEvent(&event);

}

static void tty_ioctl(fuse_req_t req, int cmd, void *arg,
			  struct fuse_file_info *fi, unsigned flags,
			  const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	static pid_t control_pid = 0;
	static struct termios termios;

	if(flags & FUSE_IOCTL_COMPAT){
		fuse_reply_err(req,ENOSYS);
		return;
	}



	struct iovec in,out;

	in.iov_base = &termios;
	in.iov_len  = sizeof(termios);
	out.iov_base = &termios;
	out.iov_len  = sizeof(termios);


	switch(cmd){
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

	case TIOCMGET:
		fuse_reply_err(req,EINVAL);
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
	case TCFLSH:
		fuse_reply_ioctl(req, 0, 0,0);
		break;
	default:
		fuse_reply_err(req,ENOSYS);

//		fuse_reply_ioctl(req,EPERM,0,0);
		return ;
	}
	printf("handled cmd=%d . arg=%p\n",cmd,arg);
}


static void tty_open(fuse_req_t req, struct fuse_file_info *fi)
{
	fuse_reply_open(req, fi);
}

/*
 * redraw current screen
 */
static void 	SDL_redraw()
{
	int i;

	FT_Bitmap bmp;
	SDL_Surface	* screen;
	SDL_Rect		pos;

	screen = SDL_GetVideoSurface();

	int sdldeeps[]={
			32,1,8,2,4,8,8
	};

	SDL_FillRect(screen,0,0);

	for(i=0;i < screen_width*screen_rows ; i++ )
	{
		pos.x = (i % screen_width) * 8;
		pos.y = (i / screen_width) * 16;//pos.h;

//		pos.w = 8;
//		pos.h = 16;

		FT_Load_Char(ftface,screenbuf[i],FT_LOAD_RENDER|FT_LOAD_TARGET_NORMAL);

		bmp = ftface->glyph->bitmap;

		SDL_Surface * source = SDL_CreateRGBSurfaceFrom(bmp.buffer,
				bmp.width,bmp.rows,sdldeeps[bmp.pixel_mode],bmp.pitch,0xff,0xff,0xff,0);

		SDL_BlitSurface(source,0,screen,&pos);

		SDL_FreeSurface(source);
		i+=g_unichar_iswide(screenbuf[i]);
	}

	SDL_Flip(screen);
}

static void loadfont();


int main(int argc, char **argv)
{


	SDL_Event event;

//	unsetenv("DISPLAY");

	init_irq();

	loadfont();

	setup_display();

	SDL_CreateThread(cuse_loop, device_init());

	while (SDL_WaitEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			SDL_Quit();
			return 0;
		case SDL_USEREVENT:
			SDL_redraw();
			break;
		case SDL_KEYDOWN:
			// process keyborad event
			irq_kbd(&event);
		}
//		printf("event type %d\n", event.type);
	}
}


static void tty_poll(fuse_req_t req, struct fuse_file_info *fi,struct fuse_pollhandle *ph)
{
//	sleep(1);

//	fuse_reply_poll(req,POLL_);

}

static const struct cuse_lowlevel_ops tty_clop = {
	.open		= tty_open,
	.read		= tty_read,
	.write		= tty_write,
	.ioctl		= tty_ioctl,
	.poll		= tty_poll,
};



struct fuse_session * device_init()
{
	struct fuse_args args={0};

	const char *dev_info_argv[] = { "DEVNAME=utty","MODE=0666" };
	struct cuse_info ci={0};

	ci.dev_info_argc = 1;
	ci.dev_info_argv = dev_info_argv;
	ci.flags = CUSE_UNRESTRICTED_IOCTL;

	fuse_opt_add_arg(&args, "utty");
	fuse_opt_add_arg(&args, "-d");



	int no=0;

	return  cuse_lowlevel_setup(args.argc,args.argv, &ci, &tty_clop, &no ,NULL);

}

int cuse_loop( void * threadparam)
{
	struct fuse_session * fuse = threadparam;

	fuse_session_loop(fuse);

	exit(0);
}

void setup_display()
{
	SDL_Surface * screen;

	setenv("SDL_VIDEO_CENTERED","1",1);

	SDL_Init(SDL_INIT_EVENTTHREAD|SDL_INIT_VIDEO);

	SDL_EnableUNICODE(1);

	int SDLCALL EventFilter(const SDL_Event *event)
	{
		switch (event->type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_QUIT:
		case SDL_USEREVENT:
			return 1;
		}
		return 0;
	}

	// we only want keyborad event
	SDL_SetEventFilter(EventFilter);

	char buf[128]={0};
	SDL_VideoDriverName(buf,sizeof buf);
	printf("video driver %s ， suported modes: ",buf);

	SDL_Rect ** modes = SDL_ListModes(NULL, SDL_SWSURFACE);

	if (modes && modes != (void*)-1)
	{
		SDL_Rect * m = *modes;

		screen = SDL_SetVideoMode(m->w, m->h, 32 , SDL_SWSURFACE);

		screen_width = m->w / 8;
		screen_rows = m->h/16;

		printf("pixels: %dx%d , text  %dx%d\n",m->w,m->h,screen_width,screen_rows);

	}
	else if(!modes)
	{
		printf("no modes\n");
		exit(1);
	}else
	{
		screen = SDL_SetVideoMode(800, 600, 32, SDL_SWSURFACE);

		screen_width = 800 / 8;
		screen_rows = 600/16;

		printf("any, so i set text mode  %dx%d\n\n",screen_width,screen_rows);

	}


	SDL_Color colors[256];

	for(int i=0;i<256;i++){
	  colors[i].r=i;
	  colors[i].g=i;
	  colors[i].b=i;
	}

//	SDL_SetColors(screen, colors, 0, 256);

	screenbuf = g_malloc0_n(screen_width*screen_rows,sizeof(gunichar));

	SDL_Surface * bmp = SDL_LoadBMP("test.bmp");

	if(SDL_BlitSurface(bmp,0,screen,0))
		fprintf(stderr,"error SDL!!!\n");
	SDL_Flip(screen);	SDL_FreeSurface(bmp);
}

static void loadfont()
{
	int ret;

	FT_Init_FreeType(&ftlib);

	FcConfigEnableHome(FALSE);
	FcInit();

	FcPattern * fcpat =  FcPatternCreate();
	FcConfigSubstitute(0,fcpat,FcMatchPattern);

	FcDefaultSubstitute(fcpat);

	ret = FcPatternAddString(fcpat,FC_FAMILY,"Monospace");

	FcFontSet * fcfs = FcFontSetCreate ();

	FcResult result;
	FcFontSetAdd(fcfs,FcFontMatch(0,fcpat,&result));
	FcPatternDestroy(fcpat);

	FcFontSetPrint(fcfs);

	FcPattern * matchedpattern = FcFontSetMatch(0,&fcfs,1,fcpat,&result);

	FcChar8 * ttf;

	ret = FcPatternGetString(matchedpattern,FC_FILE,0,&ttf);

	FT_New_Face(ftlib,ttf,0,&ftface);
	FcPatternDestroy(matchedpattern);

	FT_Set_Pixel_Sizes(ftface,0,16);
}
