
#include <glib.h>
#include <string.h>

#include "utf8.h"

int utf8_ucs(uint32_t * out, int size,const char * utf8in)
{
	glong items;
	gunichar * out_malloc = g_utf8_to_ucs4_fast(utf8in,size,&items);

	memcpy(out,out_malloc,items * sizeof(gunichar));

	g_free(out_malloc);

	return items;
}

int iswide(uint32_t c)
{
	return g_unichar_iswide_cjk(c);
}

int isprintable(uint32_t c)
{
	return g_unichar_isprint(c);
}
