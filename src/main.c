
#include <console/console.h>
#include <font/font.h>
#include <vte/vte.h>

int main(int argc, char **argv)
{
	struct vte * vte;
	struct screenop * screen;
	struct kbdop * kbd;
	struct fontop * font;

	//open I/O device, screen and seat
	screen = screen_sdl_new();
	kbd = kbd_sdl_new();

	// font init
	font = font_static_new();

	// VT init
	vte = vte_cuse_new();

	// console init
	console_init(screen,kbd,font);

	// attatch vte to console
	console_attatch_vte(vte);

	// main loop
	console_run();
	return 0;
}
