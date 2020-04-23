#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([gpicann], [0.91], [masm+github@masm11.me])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/settings.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [cos])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

# Checks for library functions.
AC_CHECK_FUNCS([strerror])

IT_PROG_INTLTOOL(0.50.0)
GETTEXT_PACKAGE="${PACKAGE}"
AC_SUBST(GETTEXT_PACKAGE)

PKG_CHECK_MODULES(GTK, [gtk+-3.0])

AC_CONFIG_FILES([icons/Makefile src/Makefile po/Makefile.in Makefile])
AC_OUTPUT
