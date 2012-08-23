
dnl AX_CHECK_COMPILE_FLAG
AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.69)dnl for _AC_LANG_PREFIX
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_IF([test x"AS_VAR_GET(CACHEVAR)" = xyes],
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(cusetty, 1.0)
AC_CONFIG_FILES(Makefile src/Makefile src/console/Makefile src/font/Makefile src/vte/Makefile)


AC_CANONICAL_SYSTEM
AC_PROG_RANLIB
AM_INIT_AUTOMAKE()

AC_PROG_CC

AX_CHECK_COMPILE_FLAG([-std=gnu99],[CC="$CC -std=gnu99"],[])

AC_DEFINE(_GNU_SOURCE)

AC_CHECK_LIB(SDL,SDL_Init,,AC_MSG_FAILURE([need SDL],1))

PKG_CHECK_MODULES([FUSE],[fuse])
PKG_CHECK_MODULES([GLIB],[glib-2.0])


AC_OUTPUT

