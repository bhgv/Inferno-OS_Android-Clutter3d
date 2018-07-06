m4_define([ga_version_major], [0])
m4_define([ga_version_minor], [1])
m4_define([ga_version_micro], [0])

m4_define([ga_api_version_major], [1])
m4_define([ga_api_version_minor], [0])

m4_define([ga_version],
          [ga_version_major.ga_version_minor.ga_version_micro])

m4_define([ga_api_version], [ga_api_version_major.ga_api_version_minor])

m4_define([ga_interface_age], [0])
m4_define([ga_binary_age],
          [m4_eval(100 * ga_version_minor + ga_version_micro)])

AC_PREREQ(2.59)

AC_INIT([glib-android], [ga_version], [http://damien.lespiau.name])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_AUX_DIR([build/autotools])
AC_CONFIG_SRCDIR([glib-android.h])

AM_INIT_AUTOMAKE([1.10 -Wall -Wno-portability foreign no-define check-news])
AM_CONFIG_HEADER([config.h])
AM_SILENT_RULES([yes])

LT_PREREQ([2.2.6])
LT_INIT()

GA_VERSION_MAJOR=ga_version_major
GA_VERSION_MINOR=ga_version_minor
GA_VERSION_MICRO=ga_version_micro
GA_API_VERSION=ga_api_version
AC_SUBST(GA_VERSION_MAJOR)
AC_SUBST(GA_VERSION_MINOR)
AC_SUBST(GA_VERSION_MICRO)
AC_SUBST(GA_API_VERSION)

m4_define([lt_current],  [m4_eval(ga_binary_age - ga_interface_age)])
m4_define([lt_revision], [ga_interface_age])
m4_define([lt_age],      [m4_eval(ga_binary_age - ga_interface_age)])
GA_LT_CURRENT=lt_current
GA_LT_REV=lt_revision
GA_LT_AGE=lt_age
GA_LT_VERSION="$GA_LT_CURRENT:$GA_LT_REV:$GA_LT_AGE"
GA_LT_LDFLAGS="-version-info $GA_LT_VERSION"

AC_SUBST(GA_LT_VERSION)
AC_SUBST(GA_LT_LDFLAGS)

# Check for programs
AC_PROG_CC
PKG_PROG_PKG_CONFIG

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Check for header files
AC_HEADER_STDC

GA_REQUIRES="glib-2.0 >= 2.6.0"
AC_SUBST(GA_REQUIRES)

PKG_CHECK_MODULES([GLIB], [$GA_REQUIRES])

AS_COMPILER_FLAGS([COMPILER_CFLAGS],
                  ["-Wall -Wextra -Wno-unused-parameter
                   -Wno-missing-field-initializers -Winvalid-pch
                   -Wdeclaration-after-statement -Wmissing-declarations
                   -Wmissing-prototypes -Wredundant-decls -Wmissing-noreturn
                   -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings
                   -Winline -Wformat-nonliteral -Wformat-security -Wswitch-enum
                   -Wswitch-default -Winit-self -Wmissing-include-dirs -Wundef
                   -Waggregate-return -Wmissing-format-attribute -Wsync-nand
                   -Wnested-externs -Wunsafe-loop-optimizations -Wpacked"])
AC_SUBST(COMPILER_CFLAGS)

AC_CONFIG_FILES([
    Makefile
    glib-android.pc
])
AC_OUTPUT

echo ""
echo " glib-android $VERSION"
echo ""
