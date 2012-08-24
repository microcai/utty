SUBDIRS = font console tty vte

bin_PROGRAMS=utty
utty_SOURCES=main.c
				
LIBS+=@FUSE_LIBS@ @GLIB_LIBS@

utty_LDADD=font/libfont.a console/libconsole.a tty/libtty.a vte/libvte.a

utty_DEPENDENCIES=$(utty_LDADD)