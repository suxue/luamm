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

#define BOOST_TEST_MODULE LuammTest
#include "luamm.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/mpl/assert.hpp>
#include <cstdlib>
#include <cmath>
#include <functional>

using namespace luamm;
using namespace std;
using namespace boost::mpl;

BOOST_AUTO_TEST_CASE( PrePush_test )
{
    BOOST_MPL_ASSERT(( PredPush<std::string, const char*> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, const char*&> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, const char*&&> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, char const * const &> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, char[2]> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, const char[2]> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, std::string&> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, std::string&&> ));
    BOOST_MPL_ASSERT(( PredPush<std::string, const std::string&> ));

    BOOST_MPL_ASSERT(( PredPush<Number, int> ));
    BOOST_MPL_ASSERT(( PredPush<Number, unsigned int> ));
    BOOST_MPL_ASSERT(( PredPush<Number, long> ));
    BOOST_MPL_ASSERT(( PredPush<Number, double> ));
}

BOOST_AUTO_TEST_CASE( PredGet_test )
{
    BOOST_MPL_ASSERT(( PredGet<Number, Number> ));
    BOOST_MPL_ASSERT(( PredGet<Number, double> ));
    BOOST_MPL_ASSERT(( PredGet<Number, int> ));
    BOOST_MPL_ASSERT(( PredGet<Number, bool> ));
    BOOST_MPL_ASSERT(( not_<PredGet<Number, void*>> ));


    BOOST_MPL_ASSERT(( PredGet<const char*, string> ));
    BOOST_MPL_ASSERT(( PredGet<const char*, const char*> ));
    BOOST_MPL_ASSERT(( not_<PredGet<const char*, char*>> ));
}

#ifndef _MSC_VER
BOOST_AUTO_TEST_CASE( Choose )
{
    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredPush, const char*>::type,
                        string
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredGet, string>::type,
                        const char*
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredGet, int>::type,
                        Number
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredGet, unsigned int>::type,
                        Number
                      >));
}
#endif

static int cfunction(lua_State* st) {
    return 1;
}

BOOST_AUTO_TEST_CASE( Basic_Load )
{
    NewState lua;
    lua.openlibs();

    int num = rand();
    lua.push(num);
    BOOST_CHECK_EQUAL(Number(lua[-1]), num);
    BOOST_CHECK_EQUAL(lua.top(), 1);

    lua.push("hello");
    BOOST_CHECK_EQUAL((const char*)lua[-1], "hello");
    BOOST_CHECK_EQUAL(lua.top(), 2);

    lua.push(true);
    BOOST_REQUIRE( bool(lua[-1]) );
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table tbl = lua.newTable();
        tbl[1] = "hello";
        BOOST_CHECK_EQUAL(lua.top(), 4);

        BOOST_CHECK_EQUAL((const char*)tbl[1], "hello");
        BOOST_CHECK_EQUAL(lua.top(), 4);

        tbl[2] = cfunction;
        CFunction cfunc = tbl[2];
        BOOST_CHECK_EQUAL(cfunc, cfunction);
        BOOST_CHECK_EQUAL(lua.top(), 4);

        BOOST_MPL_ASSERT(( std::is_same<VarPusher<Table>::type, Table> ));
        auto math = lua["math"];
        BOOST_CHECK_EQUAL(math.type(), LUA_TTABLE);

        Table math_tab = math;
        BOOST_CHECK_EQUAL(math_tab.index, 5);
        BOOST_CHECK_EQUAL(lua.top(), 5);

        auto pi = math_tab["pi"];
        BOOST_CHECK_EQUAL(pi.type(), LUA_TNUMBER);
        BOOST_CHECK_EQUAL(lua.top(), 5);

        Number pino = pi;
        BOOST_REQUIRE(pino > 3.14 && pino < 3.15);
        BOOST_CHECK_EQUAL(lua.top(), 5);

        // has 1 upvalue
        math_tab["xxx"] = CClosure(cfunction, 1);
        BOOST_CHECK_EQUAL(lua.top(), 5);

        BOOST_REQUIRE(math_tab["xxx"].iscfun());
        Closure func = math_tab["xxx"];
        BOOST_CHECK_EQUAL(lua.top(), 6);

        func[1] = "nice";
        BOOST_CHECK_EQUAL(lua.top(), 6);

        const char *nice = func[1];
        BOOST_CHECK_EQUAL(lua.top(), 6);
        BOOST_CHECK_EQUAL(nice, "nice");
    }

    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table t = lua.newTable();
        BOOST_CHECK_EQUAL(t.index, 4);

        Table mt = lua.newTable();
        BOOST_CHECK_EQUAL(mt.index, 5);

        t.set(mt);
        Table _mt = t.get();
        BOOST_REQUIRE(mt == _mt);
    }

    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table math = lua["math"];
        UserData ud = lua.newUserData<double>(100);
        math["userdata"] = ud;
    }

    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table math = lua["math"];
        UserData ud = math["userdata"];
        BOOST_CHECK_EQUAL(*ud.to<double>(), 100);
    }

    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table math = lua["math"];
        math["userdata"] = static_cast<void*>(new double(101));
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table math = lua["math"];
        void *p = math["userdata"];
        auto pp = static_cast<double*>(p);
        BOOST_CHECK_EQUAL(*pp, 101);
        delete pp;
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Table math = lua["math"];
        Closure log = math["log"];
        BOOST_CHECK_EQUAL(Number(log(100)), std::log(100));
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Closure log = lua.newFunc("return math.log(...)");
        // lua not distinguishes string and number
        BOOST_CHECK_EQUAL(Number( log("100") ), std::log(100));
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        // light cfunction (no upvalue
        lua.push(CClosure(
        [](lua_State* st) {
            // inside a lua cfunction,
            // the top is 0 initialy, and first argument at position 1, etc
            // to return values, push first rval first,
            // finally return number of return values
            State s(st);
            Number in = s[1];
            s.push(in + 1);
            return 1;
        }));
        Number in = rand();
        Closure cl = lua[-1];
        BOOST_CHECK_EQUAL( Number(cl(in)), in+1 );
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Closure cl = lua.newCallable([](State st, Number num) -> Number {
            return num + 7777;
        });
        BOOST_CHECK_EQUAL(lua.top(), 4);

        Number in = rand();
        Number num = cl(in);
        BOOST_CHECK_EQUAL(num, in + 7777);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Closure cl = lua.newCallable([](State& st, Table&& pair) -> Number {
            return Number(pair["num1"]) + Number(pair["num2"]);
        });

        Table in = lua.newTable();
        in["num1"] = 12;
        in["num2"] = 13;

        Number num = cl(in);
        BOOST_CHECK_EQUAL(num, 25);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

}

