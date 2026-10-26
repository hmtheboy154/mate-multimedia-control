# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.66])
AC_INIT([mate-applet-multimedia-control], [0.2], [mati86dl@gmail.com])

AM_INIT_AUTOMAKE([-Wall])

AC_CONFIG_HEADERS([config.h])

# prefix
if test "x$prefix" = "xNONE" ; then
	prefix="/usr/local"
fi
if test "x$exec_prefix" = "xNONE" ; then
	exec_prefix=$prefix
fi
PREFIX=$prefix
AC_SUBST([PREFIX])
BINDIR=$exec_prefix/bin
AC_SUBST([BINDIR])
SBINDIR=$exec_prefix/sbin
AC_SUBST([SBINDIR])
DATAROOTDIR=$prefix/share
AC_SUBST([DATAROOTDIR])
LIBEXECDIR=$prefix/libexec
if test "$libexecdir" != '${exec_prefix}/libexec'; then
        LIBEXECDIR=$libexecdir
fi
AC_SUBST([LIBEXECDIR])

### Checks for programs

# GCC
AC_PROG_CC
MAKEFILE_CC=`which gcc`
AC_SUBST([MAKEFILE_CC])

### Checks for libraries.
LIBMATEPANELAPPLET="no"
PKG_CHECK_MODULES([LIBMATEPANELAPPLET_4], [libmatepanelapplet-4.0], [LIBMATEPANELAPPLET="libmatepanelapplet-4.0"], [
        PKG_CHECK_MODULES([LIBMATEPANELAPPLET_3], [libmatepanelapplet-3.0], [LIBMATEPANELAPPLET="libmatepanelapplet-3.0"], [
                PKG_CHECK_MODULES([LIBMATEPANELAPPLET_2], [libmatepanelapplet-2.0], [LIBMATEPANELAPPLET="libmatepanelapplet-2.0"], [
                        AC_MSG_ERROR(["*** libmatepanelapplet not found."])
                ])
        ])
])

### Check for packages
PKG_CHECK_MODULES([GTK], [gtk+-3.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([PLAYERCTL], [playerctl])
PKG_CHECK_MODULES([SOUP], [libsoup-3.0])

# Translation support (used by the menu labels)
GETTEXT_PACKAGE=mate-applet-multimedia-control
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE([GETTEXT_PACKAGE], ["mate-applet-multimedia-control"], [Gettext package])

GLIB_GSETTINGS

# Makefiles
MULTIMEDIA_CONTROL_FLAGS=`$PKG_CONFIG --cflags-only-I $LIBMATEPANELAPPLET playerctl`
MULTIMEDIA_CONTROL_LIBS=`$PKG_CONFIG --libs $LIBMATEPANELAPPLET playerctl`
AC_SUBST([MULTIMEDIA_CONTROL_FLAGS])
AC_SUBST([MULTIMEDIA_CONTROL_LIBS])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_FILES([src/org.mate.applets.MultimediaControlApplet.mate-panel-applet])
AC_CONFIG_FILES([src/org.mate.panel.applet.MultimediaControlApplet.service])

# Grand finale
AC_OUTPUT

echo
echo "Configuration complete."
echo
echo "Now, run make."
echo 
