# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

plugindir = $(libdir)/gnunet

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
   rest.conf


if MINGW
  WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIBS = -lgcov
endif

lib_LTLIBRARIES = \
	libgnunetrest.la 

libexec_PROGRAMS = \
 gnunet-rest-server

EXTRA_DIST = \
 rest.conf

gnunet_rest_server_SOURCES = \
 gnunet-rest-server.c

gnunet_rest_server_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL) -lmicrohttpd

libgnunetrest_la_SOURCES = \
  rest.c
libgnunetrest_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la $(XLIB) \
  $(GN_LIBINTL) -lmicrohttpd 
libgnunetrest_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) \
  -version-info 0:0:0

