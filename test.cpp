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
#include <tuple>

using namespace luamm;
using namespace std;
using namespace boost::mpl;

    BOOST_MPL_ASSERT(( detail::PredPush<std::string, const char*> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, const char*&> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, const char*&&> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, char const * const &> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, char[2]> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, const char[2]> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, std::string&> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, std::string&&> ));
    BOOST_MPL_ASSERT(( detail::PredPush<std::string, const std::string&> ));

    BOOST_MPL_ASSERT(( detail::PredPush<Number, int> ));
    BOOST_MPL_ASSERT(( detail::PredPush<Number, unsigned int> ));
    BOOST_MPL_ASSERT(( detail::PredPush<Number, long> ));
    BOOST_MPL_ASSERT(( detail::PredPush<Number, double> ));

    BOOST_MPL_ASSERT(( detail::PredGet<Number, Number> ));
    BOOST_MPL_ASSERT(( detail::PredGet<Number, double> ));
    BOOST_MPL_ASSERT(( detail::PredGet<Number, int> ));
    BOOST_MPL_ASSERT(( detail::PredGet<Number, bool> ));
    BOOST_MPL_ASSERT(( not_<detail::PredGet<Number, void*>> ));


    BOOST_MPL_ASSERT(( detail::PredGet<const char*, string> ));
    BOOST_MPL_ASSERT(( detail::PredGet<const char*, const char*> ));
    BOOST_MPL_ASSERT(( not_<detail::PredGet<const char*, char*>> ));

    BOOST_MPL_ASSERT((std::is_same<
                        detail::SelectImpl<detail::PredPush, const char*>::type,
                        string
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        detail::SelectImpl<detail::PredGet, string>::type,
                        const char*
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        detail::SelectImpl<detail::PredGet, int>::type,
                        Number
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        detail::SelectImpl<detail::PredGet, unsigned int>::type,
                        Number
                      >));

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

        t.setmetatable(mt);
        Table _mt = t.getmetatable();
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
        BOOST_CHECK_EQUAL(ud.to<double>(), 100);
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
        Number res = log(100);
        BOOST_CHECK_EQUAL(res, std::log(100));
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
        Closure cl = lua.push(CClosure(
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
        BOOST_CHECK_EQUAL( Number(cl(in)), in+1 );
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Closure cl = lua.newCallable([](State& st, Number num) -> Number {
            return num + 7777;
        });
        BOOST_CHECK_EQUAL(lua.top(), 4);

        Number in = rand();
        Number num = cl(in);
        BOOST_CHECK_EQUAL(num, in + 7777);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    { // return void
        Closure cl = lua.newCallable([](State& st, string&& key, Number num) {
            st[key] = num;
        });
        cl.call<0>("hello", 12);
        BOOST_CHECK_EQUAL(Number(lua["hello"]), 12);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    { // simple multiple return
        Closure cl = lua.newCallable([](State& st, Number a, Number b) {
            return make_tuple(b, a);
        });
        lua["test"] = cl;
        Closure recv = lua.newFunc(
                "local a, b = test(11, 12); return tostring(a)");
        Number ret  = recv();
        BOOST_CHECK_EQUAL(ret, 12);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    { // simple multiple return
        auto scope = lua.newScope();

        Closure cl = lua.newCallable([](State& st, Number a, Number b) {
            return make_tuple(b, a);
        });

        auto ret = cl.call<2>(11, 12);

        BOOST_CHECK_EQUAL(lua.top(), 6);
        Number a = get<0>(ret);
        Number b = get<1>(ret);
        BOOST_CHECK_EQUAL(a, 12);
        BOOST_CHECK_EQUAL(b, 11);
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

    {
        auto scope = lua.newScope();
        Closure cl = lua.newFunc("return 1, 2, 3, 5, 8, 13;");
        int a, b, d, e, f;
        tie(a, b, ignore, d, e, f) = cl.call<6>();
        BOOST_CHECK_EQUAL(a+b+d+e+f, 29);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    {
        Closure cl = lua.newCallable([](int i, int j) {
                return i*j;
        });
        BOOST_CHECK_EQUAL(Number(cl(2,4)), 8);
        BOOST_CHECK_EQUAL(Number(cl(200,300)), 60000);
    }
    BOOST_CHECK_EQUAL(lua.top(), 3);

    lua.settop(0);
    {
        // test module system
        struct Hello {};
        Table mod = move(
            lua.class_<Hello>("hello")
            .def("add", [](int i, int j) { return i+j; })
            .init()
        );
        BOOST_CHECK_EQUAL((const char*)mod["className"], "hello");
        Closure add = lua.newFunc(
   "local hello, a,b = ...; local obj = hello(); return obj.add(a, b)");
        BOOST_CHECK_EQUAL(Number(add(mod, 1, 2)), 3);
    }
    BOOST_CHECK_EQUAL(lua.top(), 0);

    {
        // simple class binding
        struct Counter {
            int num;
            Counter(int initial_value) : num(initial_value) {}
            void add(int i) { num += i; }
            int get() { return num; }
        };
        Table mod = move(
            lua.class_<Counter>("counter")
            .def("get", &Counter::get)
            .def("add", &Counter::add)
            .init<int>() // enable default constructor
        );
        lua["counter"] = mod;
        Closure testcl = lua.newFunc(R"==(
            x = counter(0);
            x:add(100);
            x:add(1000);
            return x:get()
        )==");
        BOOST_CHECK_EQUAL(Number(testcl()), 1100);
    }
    BOOST_CHECK_EQUAL(lua.top(), 0);

    {
        // class member getter and setter
        struct Data {
            int num;
        };
        Table mod = move(
            lua.class_<Data>("data")
            .init()
            .attribute("num", &Data::num)
        );
        Closure setter = lua.newFunc(R"==(
            local data = ...
            d = data();
            return d:set_num(5);
        )==");
        UserData d = setter(mod, mod);
        BOOST_CHECK_EQUAL(d.to<Data>().num, 5);

        d.to<Data>().num = 10;
        Number result = lua.newFunc(R"==(
            local d = ...
            return d:get_num()
        )==")(d);
        BOOST_CHECK_EQUAL(result, 10);
    }
    BOOST_CHECK_EQUAL(lua.top(), 0);

    {
        auto scope = lua.newScope();
        struct Data {
            Data(int i) : num(i) {}
            int num;
        };
        Table mod = std::move(
            lua.class_<Data>("data")
            .attribute("num", &Data::num)
            .init<int>()
        );

        lua.newFunc(R"==(
            local mod = ...
            local d = mod(5);
            assert(d.num == 5, "d is initialized to 5")
            d.num = 10
            assert(d.num == 10, "d is set to 10")
        )==")(mod);
    }

    {
        // long parameter list
        auto scope = lua.newScope();
        Closure cl = lua.newCallable([](int a, int b, int c, int d, int e) {
            return make_tuple(e, d, c, b, a);
        });
        int a, b, c, d, e;
        tie(e, d, c, b, a) = cl.call<5>(1,2,3,4,5);
        BOOST_CHECK_EQUAL(a, 1);
        BOOST_CHECK_EQUAL(b, 2);
        BOOST_CHECK_EQUAL(c, 3);
        BOOST_CHECK_EQUAL(d, 4);
        BOOST_CHECK_EQUAL(e, 5);
    }
    BOOST_CHECK_EQUAL(lua.top(), 0);
}

