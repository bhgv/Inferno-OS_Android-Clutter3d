## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = libsoup po tests

EXTRA_DIST =				\
	data/effective_tld_names.dat	\
	libsoup-2.4.pc.in		\
	libsoup-gnome-2.4.pc.in		\
	libsoup-zip.in			\
	Makefile.glib

DISTCHECK_CONFIGURE_FLAGS = 

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libsoup-2.4.pc

if BUILD_LIBSOUP_GNOME
pkgconfig_DATA += libsoup-gnome-2.4.pc
endif
