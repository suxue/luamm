# Copyright (c) 2014 Hao Fei <mrfeihao@gmail.com>
#
# This file is part of luamm.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(C11)
Find_package(MyBoost)

# actually we need lua52
find_package(Lua51)
include_directories(${LUA_INCLUDE_DIR})
MESSAGE(STATUS "LUA_FOUND = ${LUA51_FOUND}")
MESSAGE(STATUS "LUA_INCLUDE_DIR = ${LUA_INCLUDE_DIR}")
MESSAGE(STATUS "LUA_LIBRARY = ${LUA_LIBRARIES}")

add_library(luamm STATIC luamm.hpp)
set_target_properties(luamm PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(luamm ${LUA_LIBRARIES})

target_include_directories(luamm INTERFACE ./)
target_include_directories(luamm PRIVATE ${LUA_INCLUDE_DIR})
#set_property(TARGET luamm APPEND PROPERTY COMPILE_FLAGS "-fPIC")

add_executable(luamm_test test.cpp)
target_link_libraries(luamm_test ${Boost_TEST_EXEC_MONITOR_LIBRARY})
target_link_libraries(luamm_test luamm)

enable_testing ()

if (BUILD_DEMOS)
    add_subdirectory(demos)
endif()

ADD_TEST(NAME testdep/luamm  COMMAND "${CMAKE_COMMAND}" --build
    ${CMAKE_BINARY_DIR} --target luamm_test)
add_test (NAME test/luamm COMMAND luamm_test -o=report)

SET_TESTS_PROPERTIES ( test/luamm PROPERTIES DEPENDS testdep/luamm )
