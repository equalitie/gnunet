# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

if MINGW
 WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
  rps.conf

bin_PROGRAMS = gnunet-rps

gnunet_rps_SOURCES = gnunet-rps.c
gnunet_rps_LDADD = \
  libgnunetrps.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(XLIB) $(GN_LIBINTL)

lib_LTLIBRARIES = libgnunetrps.la

libgnunetrps_la_SOURCES = \
  rps_api.c rps.h
libgnunetrps_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL) $(XLIB)
libgnunetrps_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  $(WINFLAGS) \
  -version-info 0:0:0


libexec_PROGRAMS = \
 gnunet-service-rps

if HAVE_TESTING
noinst_PROGRAMS = \
 gnunet-rps-profiler
endif


gnunet_service_rps_SOURCES = \
 gnunet-service-rps_sampler_elem.h gnunet-service-rps_sampler_elem.c \
 gnunet-service-rps_sampler.h gnunet-service-rps_sampler.c \
 gnunet-service-rps_custommap.h gnunet-service-rps_custommap.c \
 gnunet-service-rps_view.h gnunet-service-rps_view.c \
 rps-test_util.h rps-test_util.c \
 gnunet-service-rps.c


gnunet_service_rps_LDADD = \
  libgnunetrps.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/cadet/libgnunetcadet.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/nse/libgnunetnse.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(LIBGCRYPT_LIBS) \
  -lm -lgcrypt \
  $(GN_LIBINTL)


if HAVE_TESTING
check_PROGRAMS = \
 test_service_rps_view \
 test_service_rps_custommap \
 test_service_rps_sampler_elem \
 test_rps_malicious_1 \
 test_rps_malicious_2 \
 test_rps_malicious_3 \
 test_rps_seed_request \
 test_rps_single_req \
 test_rps_req_cancel \
 test_rps_seed_big \
 test_rps_churn
endif

ld_rps_test_lib = \
	libgnunetrps.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testbed/libgnunettestbed.la \
  -lm

rps_test_src = \
	test_rps.c \
	rps-test_util.h rps-test_util.c \
 gnunet-service-rps_sampler_elem.h gnunet-service-rps_sampler_elem.c

if ENABLE_TEST_RUN
AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;unset XDG_DATA_HOME;unset XDG_CONFIG_HOME;
TESTS = $(check_PROGRAMS)
endif

test_service_rps_view_SOURCES = \
  gnunet-service-rps_view.h gnunet-service-rps_view.c \
  test_service_rps_view.c
test_service_rps_view_LDADD = $(top_builddir)/src/util/libgnunetutil.la

test_service_rps_custommap_SOURCES = \
  gnunet-service-rps_custommap.h gnunet-service-rps_custommap.c \
  test_service_rps_custommap.c
test_service_rps_custommap_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la

test_service_rps_sampler_elem_SOURCES = \
  gnunet-service-rps_sampler_elem.h gnunet-service-rps_sampler_elem.c \
  rps-test_util.h rps-test_util.c \
  test_service_rps_sampler_elem.c
test_service_rps_sampler_elem_LDADD = $(top_builddir)/src/util/libgnunetutil.la

test_rps_malicious_1_SOURCES = $(rps_test_src)
test_rps_malicious_1_LDADD = $(ld_rps_test_lib)

test_rps_malicious_2_SOURCES = $(rps_test_src)
test_rps_malicious_2_LDADD = $(ld_rps_test_lib)

test_rps_malicious_3_SOURCES = $(rps_test_src)
test_rps_malicious_3_LDADD = $(ld_rps_test_lib)

test_rps_single_req_SOURCES = $(rps_test_src)
test_rps_single_req_LDADD = $(ld_rps_test_lib)

test_rps_seed_request_SOURCES = $(rps_test_src)
test_rps_seed_request_LDADD = $(ld_rps_test_lib)

test_rps_req_cancel_SOURCES = $(rps_test_src)
test_rps_req_cancel_LDADD = $(ld_rps_test_lib)

test_rps_seed_big_SOURCES = $(rps_test_src)
test_rps_seed_big_LDADD = $(ld_rps_test_lib)

test_rps_churn_SOURCES = $(rps_test_src)
test_rps_churn_LDADD = $(ld_rps_test_lib)

gnunet_rps_profiler_SOURCES = $(rps_test_src)
gnunet_rps_profiler_LDADD = $(ld_rps_test_lib)

EXTRA_DIST = \
  test_rps.conf
