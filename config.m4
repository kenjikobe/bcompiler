dnl
dnl $Id: config.m4 305695 2010-11-23 20:01:22Z val $
dnl

PHP_ARG_ENABLE(bcompiler, whether to enable bcompiler support,
[  --enable-bcompiler      Enable bcompiler support])

PHP_ARG_ENABLE(bcompiler-debug, whether to include debug code in bcompiler (default: no),
[  --enable-bcompiler-debug  bcompiler: Include debugging code], no, no)


if test "$PHP_BCOMPILER" != "no"; then
  bcompiler_sources="php_bcompiler.c bcompiler.c bcompiler_zend.c"

  if test "$PHP_BCOMPILER_DEBUG" = "yes"; then
    AC_DEFINE(BCOMPILER_DEBUG_ON, 1, [ ])
    bcompiler_sources="$bcompiler_sources bcompiler_debug.c"
  fi

  AC_DEFINE(HAVE_BCOMPILER, 1, [ ])
  PHP_NEW_EXTENSION(bcompiler, $bcompiler_sources, $ext_shared)
fi
