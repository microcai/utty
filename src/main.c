
#include <console/console.h>
#include <font/font.h>
#include <tty/tty.h>
#include <vte/vte.h>

int main(int argc, char **argv)
{
	struct vte * vte;
	struct screenop * screen;
	struct kbdop * kbd;
	struct fontop * font;
	struct tty	*	tty;

	//open I/O device, screen and seat
	screen = screen_sdl_new();
	kbd = kbd_sdl_new();

	// font init
	font = font_static_new();

	// tty device create
	tty = tty_cuse_new();

	// VT init
	vte = vte_new();

	// console init
	console_init(screen,kbd,font);

	// attatch vte to console
	console_attatch_vte(vte);

	// bind vte to tty
	vte_bint_tty(vte,tty);

	// main loop
	console_run();
	return 0;
}
