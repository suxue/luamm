ACLOCAL_AMFLAGS = -I ../m4

if BUILD_DEMOS
SUBDIRS = demos/
demo_test:
	$(MAKE) -C demos test
else
demo_test:
	true
endif

bin_PROGRAMS = luamm_test luamm_cov
#luamm_test_CPPFLAGS = $(BOOST_CPPFLAGS)
#luamm_test_LDFLAGS = $(BOOST_THREAD_LDFLAGS)
#luamm_test_LIBS = $(BOOST_THREAD_LIBS)

AM_CPPFLAGS = $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
LIBS = -lboost_test_exec_monitor -llua

luamm_test_SOURCES = test.cpp luamm.hpp

luamm_cov_SOURCES = test.cpp luamm.hpp
luamm_cov_CXXFLAGS = $(COVERAGE_CXXFLAGS)

luamm_cov_LDFLAGS = $(COVERAGE_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
luamm_cov_LDADD = -lboost_test_exec_monitor -llua

test: luamm_test demo_test
	./luamm_test

include $(srcdir)/../Makefile.am.coverage

coverage-executation: luamm_cov
	./luamm_cov
