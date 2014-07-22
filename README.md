Dependencies
------------

* C++ 11 compatiable compiler, for example, g++-4.8.1
* Boost 1.53+
* Lua-5.2

Project Setup
--------------

luamm is a single-file header-only library, so generally you can compile
your code by

    g++ -std=c++11 your_code.cpp -Ipath_to_luamm/ -llua

Testing and Coverage
--------------------

    ./configure
    make test
    make coverage-html

Documentation
--------------

[github wiki](https://github.com/suxue/luamm/wiki)

License
--------

see luamm.hpp

Development
-----------

luamm development is hosted by github, the luamm page is at

http://github.com/suxue/luamm

Bug reports are welcomed to be sent to github issues page.
