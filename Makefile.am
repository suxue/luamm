ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = luamm_test
#luamm_test_CPPFLAGS = $(BOOST_CPPFLAGS)
#luamm_test_LDFLAGS = $(BOOST_THREAD_LDFLAGS)
#luamm_test_LIBS = $(BOOST_THREAD_LIBS)

AM_CPPFLAGS = $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_UNIT_TEST_FRAMEWORK_LDFLAGS)
LIBS = -lboost_test_exec_monitor -llua

luamm_test_SOURCES = test.cpp luamm.hpp
