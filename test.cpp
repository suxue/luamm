/*!
 * @section LICENSE
 * Copyright (c) 2014 Hao Fei <mrfeihao@gmail.com>
 *
 * This file is part of libluamm.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 * unit testing by boost::unit_test
 */

#include "luamm.hpp"
#define BOOST_TEST_MODULE LuammTest
#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/mpl/list.hpp>
#include <string>
#include <cmath>

using namespace luamm;
using namespace std;

BOOST_AUTO_TEST_CASE( helloworld )
{
    NewState lua;

    // push number
    lua.push(1);
    BOOST_CHECK_EQUAL(Number(lua[1]),1);
    BOOST_CHECK_EQUAL(lua.top(), 1);
}


BOOST_AUTO_TEST_CASE( push )
{
    NewState lua;

    // push by negative index
    {
        lua.push("hello");
        string a = lua[-1];
        BOOST_CHECK_EQUAL(a, "hello");
    }
    BOOST_CHECK_EQUAL(lua.top(), 1);

    // push by positive index
    {
        lua.push(string("hello"));
        string a = lua[-1];
        BOOST_CHECK_EQUAL(a, "hello");
    }
    BOOST_CHECK_EQUAL(lua.top(), 2);

    // push nil
    {
        lua.push(Nil());
        BOOST_REQUIRE(lua[-1].isnil());
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    // set top
    {
        lua[-1] = true;
        BOOST_CHECK_EQUAL(bool(lua[-1]), true);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    // clear stack
    lua.pop(3);
    BOOST_CHECK_EQUAL(lua.top(), 0);
}


BOOST_AUTO_TEST_CASE( getvar )
{
    NewState lua;

    for (auto i = 0; i < 100; i++) {
        lua.push(i);
        BOOST_REQUIRE(lua.top() == i+1);
    }

    for (auto i = 0; i < 100; i++) {
        BOOST_CHECK_EQUAL(Number(lua[i+1]), i);
    }
}

BOOST_AUTO_TEST_CASE( lambda )
{
    string msg = "foobar";
    NewState lua;
    // firstlu, push the function to be executed
    lua.push([&msg](State& lua) -> int {
        Number inarg = lua[-1];
        lua[-1] = msg + to_string((int)inarg);
        return 1;
    });
    // then the first (and only) input argument
    lua.push(1);

    // pcall need to know number of in args and return values
    BOOST_CHECK_EQUAL(lua.pcall(1, 1), LUA_OK);

    const char *result = lua[-1];
    BOOST_CHECK_EQUAL(result, "foobar1");
    BOOST_CHECK_EQUAL(lua.top(), 1);
}


BOOST_AUTO_TEST_CASE( luastdlib )
{
    NewState lua;
    lua.openlibs();
    Number num = 100;
    Number log1 = log(num);

    BOOST_REQUIRE(lua["math"].istab());

    //CClosure lua_log = math["log"];
    //lua.push(lua_log);
    //lua.push(100);
    //BOOST_CHECK_EQUAL(lua.pcall(1, 1), LUA_OK);
    //Number log2 = lua[-1];

    //// lua only depends on c standard library, so SHOULD emit the same
    //// result
    //BOOST_CHECK_EQUAL(log2, log1);
}
