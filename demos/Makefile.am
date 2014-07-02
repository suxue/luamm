AM_CPPFLAGS = -I$(srcdir)/..

lib_LTLIBRARIES  = hello.la fs.la
hello_la_SOURCES = hellomod.cpp ../luamm.hpp
hello_la_LDFLAGS = -llua -module -avoid-version -shared -no-undefined

fs_la_SOURCES = fs.cpp ../luamm.hpp
fs_la_LDFLAGS = -llua -lboost_filesystem -module -avoid-version -shared -no-undefined
# -no-undefined  for libtool on mingw

test:  hello.la
	@cp .libs/hello.so . || cp .libs/hello.dll .
	@echo
	@echo "TEST through lua c module..."
	lua$(EXEEXT) $(srcdir)/hellomod.lua
	@echo
