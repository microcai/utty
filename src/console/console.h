#pragma once

#include <stdint.h>

struct screenop;
struct fontop;
struct vte;
struct fontbmp;

typedef enum input_event_type{
	input_event_none,
	input_event_key,
	input_event_expose,
	input_event_interrupt,
	input_event_terminate,
}input_event_type;

struct input_event {
	uint16_t keycode;	/* linux keycode - KEY_* - linux/input.h */
	uint32_t keysym;	/* X keysym - XK_* - X11/keysym.h */
	unsigned int mods;	/* active modifiers - uterm_modifier mask */
	uint32_t unicode;	/* ucs4 unicode value or UTERM_INPUT_INVALID */
};

struct kbdop{
	int (*init)(struct kbdop * op);

	int (*wait)(struct kbdop * op,struct input_event * input_event);

	/* interrupt the wait call*/
	int (*interrupt)(struct kbdop * op);

	void * private;
};

struct screenop{
	int (*init)(struct screenop * op);

	int (*setwindowsize)(int cols,int rows);
	void (*getwindowsize)(struct screenop *,int *width,int *height);

	/* bitblt used by fontop*/
	void (*bitblt)(struct screenop * screen,int x ,int y , struct fontbmp*);

	/*do page flip or swap buffers*/
	void (*flip)(struct screenop * screen);
	//	SDL_Flip(screen->private);


	void * private;
};

struct console{
	struct screenop *screen;
	struct kbdop *kbd;
	struct fontop* font;

	// draw to console
	void (*con_redraw)(struct console* con);
	void (*con_scroll)();

	int (*setfont)(struct fontop * fontop );

	struct vte * vte; // fg vte;

	int	need_redraw:1;	// need redraw
};

struct kbdop *kbd_sdl_new();

struct screenop * screen_sdl_new();
struct screenop * screen_drm_new();
struct screenop * screen_wayland_new();


int console_init(struct screenop * screen, struct kbdop * kbd,struct fontop* font);

int console_attatch_vte(struct vte * vte);

/* main loop*/
void console_run();
