bin_PROGRAMS=utty
utty_SOURCES=utty.c utty.h\
				font.c font.h\
				console.c console.h\
				vt.c vt.h vt_ioctl.c \
				kernel.h 

AM_CPPFLAGS=@FUSE_CFLAGS@ @GLIB_CFLAGS@ @FONTS_CFLAGS@

LIBS+=@FUSE_LIBS@ @GLIB_LIBS@ @FONTS_LIBS@

