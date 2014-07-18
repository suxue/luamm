ACLOCAL_AMFLAGS = -I autoconf-common/m4

if BUILD_DEMOS
SUBDIRS = demos/
demo_test:
	$(MAKE) -C demos test
else
demo_test:
	true
endif

bin_PROGRAMS = luamm_test

AM_CPPFLAGS = $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
LIBS = -lboost_test_exec_monitor -llua

luamm_test_SOURCES = test.cpp luamm.hpp

test: luamm_test$(EXEEXT) demo_test
	./luamm_test$(EXEEXT)

## START COVERAGE
if HAVE_GCOV
bin_PROGRAMS += luamm_cov

include $(srcdir)/autoconf-common/Makefile.am.coverage

coverage-executation: luamm_cov$(EXEEXT)
	./luamm_cov$(EXEEXT)

luamm_cov_SOURCES = test.cpp luamm.hpp
luamm_cov_CXXFLAGS = $(COVERAGE_CXXFLAGS)

luamm_cov_LDFLAGS = $(COVERAGE_LDFLAGS) $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
luamm_cov_LDADD = -lboost_test_exec_monitor -llua

endif
## END COVERAGE
