AM_CPPFLAGS = -I$(srcdir)/..

lib_LTLIBRARIES  = hello.la
hello_la_SOURCES = hellomod.cpp
hello_la_LDFLAGS = -llua -module -avoid-version -shared

test:  hello.la
	@cp .libs/hello.so .
	@echo
	@echo "TEST through lua c module..."
	lua $(srcdir)/hellomod.lua
	@echo
