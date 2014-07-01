AM_CPPFLAGS = -I$(srcdir)/..

lib_LTLIBRARIES  = hello.la
hello_la_SOURCES = hellomod.cpp
hello_la_LDFLAGS = -llua -module -avoid-version -shared

test:  .libs/hello.so
	cp .libs/hello.so .
	lua $(srcdir)/hellomod.lua
