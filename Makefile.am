ACLOCAL_AMFLAGS = -I autoconf-common

if BUILD_DEMOS
SUBDIRS = demos/
demo_test:
	$(MAKE) -C demos test
else
demo_test:
	true
endif

bin_PROGRAMS = luamm_test

AM_CPPFLAGS = $(BOOST_CPPFLAGS) $(LUA_CFLAGS)
AM_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
LIBS = -lboost_test_exec_monitor $(LUA_LIBS)

luamm_test_SOURCES = test.cpp luamm.hpp

test: luamm_test$(EXEEXT) demo_test
	./luamm_test$(EXEEXT) --build_info --detect_memory_leak=1 --random=1

## START COVERAGE
if HAVE_GCOV
include $(srcdir)/autoconf-common/Makefile.am.coverage
coverage-executation: test
AM_CPPFLAGS += $(COVERAGE_CXXFLAGS)
AM_LDFLAGS += $(COVERAGE_LDFLAGS)
endif
## END COVERAGE
