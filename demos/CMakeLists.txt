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

add_library(hellomod MODULE hellomod.cpp)
target_link_libraries(hellomod luamm)
set_target_properties(hellomod  PROPERTIES OUTPUT_NAME hello PREFIX "")

#add_custom_command(TARGET hellomod POST_BUILD
    #COMMAND lua ${CMAKE_CURRENT_SOURCE_DIR}/hellomod.lua)
#add_custom_target(hellomod_test COMMAND
    #lua ${CMAKE_CURRENT_SOURCE_DIR}/hellomod.lua DEPENDS hellomod)

ADD_TEST(NAME testdep/modhello  COMMAND "${CMAKE_COMMAND}" --build
    ${CMAKE_BINARY_DIR} --target hellomod)
ADD_TEST(test/modhello
    lua ${CMAKE_CURRENT_SOURCE_DIR}/hellomod.lua
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
SET_TESTS_PROPERTIES ( test/modhello PROPERTIES DEPENDS testdep/modhello)
