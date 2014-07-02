AM_CPPFLAGS = -I$(srcdir)/..

lib_LTLIBRARIES  = hello.la fs.la
hello_la_SOURCES = hellomod.cpp ../luamm.hpp
hello_la_LDFLAGS = -llua -module -avoid-version -shared -no-undefined

fs_la_SOURCES = fsmod.cpp ../luamm.hpp
fs_la_LDFLAGS = -llua -lboost_filesystem -module -avoid-version -shared -no-undefined
# -no-undefined  for libtool on mingw

test:  test_hello test_fs

test_fs: fs.la
	@cp .libs/fs.so . || cp .libs/fs.dll .
	@echo
	@echo "TEST c module: fs ..."
	lua$(EXEEXT) $(srcdir)/fsmod.lua
	@echo

test_hello: hello.la
	@cp .libs/hello.so . || cp .libs/hello.dll .
	@echo
	@echo "TEST c module: hello ..."
	lua$(EXEEXT) $(srcdir)/hellomod.lua
	@echo
