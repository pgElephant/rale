AC_INIT([rale], [1.0], [support@pgelephant.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip])
AC_PROG_CC
AC_PROG_AR
AC_PROG_RANLIB

# Optional: use pkg-config to find cJSON; try libcjson first then cjson; fallback to Homebrew path
AC_ARG_VAR([PKG_CONFIG], [path to pkg-config])
PKG_PROG_PKG_CONFIG
m4_ifdef([PKG_CHECK_MODULES], [
  PKG_CHECK_MODULES([CJSON], [libcjson], [have_cjson=yes], [have_cjson=no])
  if test "x$have_cjson" = "xno"; then
    PKG_CHECK_MODULES([CJSON], [cjson], [have_cjson=yes], [have_cjson=no2])
  fi
  if test "x$have_cjson" = "xno2"; then
    AC_MSG_WARN([cJSON not found via pkg-config, using Homebrew default path])
    CJSON_CFLAGS="-I/opt/homebrew/opt/cjson/include -I/opt/homebrew/opt/cjson/include/cjson"
    CJSON_LIBS="-L/opt/homebrew/opt/cjson/lib -lcjson"
  fi
], [
  AC_MSG_WARN([pkg-config not available; using Homebrew defaults for cJSON])
  CJSON_CFLAGS="-I/opt/homebrew/opt/cjson/include -I/opt/homebrew/opt/cjson/include/cjson"
  CJSON_LIBS="-L/opt/homebrew/opt/cjson/lib -lcjson"
])

AC_SUBST([CJSON_CFLAGS])
AC_SUBST([CJSON_LIBS])

AC_CONFIG_FILES([
  Makefile
  librale/Makefile
  raled/Makefile
  ralectrl/Makefile
])
AC_OUTPUT
