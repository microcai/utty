
#include <SDL/SDL.h>

#include "console.h"

static 	int init(struct kbdop * op)
{
	return SDL_InitSubSystem(SDL_INIT_EVENTTHREAD);
}

static 	int wait(struct kbdop * op,struct input_event * input_event)
{
	SDL_Event event;

	SDL_WaitEvent(&event);

	switch(event.type){
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		input_event->keycode = event.key.keysym.scancode;
		input_event->keysym = event.key.keysym.sym;
		input_event->mods = event.key.keysym.sym;
		input_event->unicode = event.key.keysym.unicode;
		return input_event_key;
	case SDL_VIDEOEXPOSE:
		return input_event_expose;
	case SDL_USEREVENT:
		return input_event_interrupt;
	case SDL_QUIT:
		return input_event_terminate;
	}

	return 0;
}

static int interrupt(struct kbdop * op)
{
	SDL_Event event={0};

	event.type = SDL_USEREVENT;
	event.user.code = input_event_interrupt;

	SDL_PushEvent(&event);
}

static struct kbdop sdlkbd={
		.init = init,
		.wait = wait,
		.interrupt = interrupt,
};

struct kbdop *kbd_sdl_new()
{
	return &sdlkbd;
}
