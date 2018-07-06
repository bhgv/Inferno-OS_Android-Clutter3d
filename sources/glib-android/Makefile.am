ACLOCAL_AMFLAGS = -I build/autotools

lib_LTLIBRARIES = libglib-android-1.0.la

headersdir = $(includedir)/glib-android-1.0/glib-android
headers_HEADERS = glib-android.h

LDADD = $(GA_LT_LDFLAGS) -export-symbol-regex "^g_android.*"
libglib_android_1_0_la_SOURCES = glib-android.c glib-android.h
libglib_android_1_0_la_CFLAGS =		\
	$(GLIB_CFLAGS)			\
	$(COMPILER_CFLAGS)		\
	-DG_LOG_DOMAIN=\"GlibAndroid\"	\
	$(NULL)
libglib_android_1_0_la_LIBADD = $(GLIB_LIBS)

pcfiles = $(PACKAGE)-$(GA_API_VERSION).pc

pkgconfig_DATA = $(pcfiles)
pkgconfigdir = $(libdir)/pkgconfig

$(PACKAGE)-$(GA_API_VERSION).pc : $(PACKAGE).pc
	$(AM_V_GEN)cp $< $@

CLEANFILES = $(pcfiles)
