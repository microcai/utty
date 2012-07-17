dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(cusetty, 1.0)
AC_CONFIG_FILES(Makefile src/Makefile)


AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AC_PROG_CC_C99

AC_DEFINE(_GNU_SOURCE)

AC_CHECK_LIB(SDL,SDL_Init,,AC_MSG_FAILURE([need SDL],1))

PKG_CHECK_MODULES([FUSE],[fuse])

PKG_CHECK_MODULES([GLIB],[glib-2.0])
PKG_CHECK_MODULES([FONTS],[fontconfig freetype2])


AC_OUTPUT

