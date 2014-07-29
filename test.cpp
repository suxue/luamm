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

BOOST_MPL_ASSERT(( std::is_same<VarPusher<Table>::type, Table> ));


class TestLuaState : public NewState {
public:
    TestLuaState() : NewState() {}
    ~TestLuaState() {
        BOOST_CHECK_EQUAL(top(), 0);
    }
};

BOOST_AUTO_TEST_CASE( push )
{
    TestLuaState lua;
    {
        int num = rand();
        lua.push(num);
        BOOST_CHECK_EQUAL(Number(lua.gettop()), num);
    }
}

BOOST_AUTO_TEST_CASE( get_top_by_negative_index )
{
    TestLuaState lua;
    {
        int num = rand();
        lua.push(num);
        BOOST_CHECK_EQUAL(Number(lua[-1]), num);
    }
    // top is not popped, because negative index cannot be equal to top
    // index
    BOOST_CHECK_EQUAL(lua.top(), 1);
    lua.pop();
}

BOOST_AUTO_TEST_CASE( read_c_string )
{
    TestLuaState lua;
    {
        lua.push("hello");
        BOOST_CHECK_EQUAL((const char*)lua.gettop(), "hello");
    }
}

BOOST_AUTO_TEST_CASE( read_bool )
{
    TestLuaState lua;
    {
        lua.push(true);
        BOOST_REQUIRE( bool(lua.gettop()) );
    }
}

BOOST_AUTO_TEST_CASE( access_c_string_from_table )
{
    TestLuaState lua;
    {
        Table tbl = lua.newTable();
        tbl[1] = "hello";
        BOOST_CHECK_EQUAL((const char*)tbl[1], "hello");
    }
}

static int cfunction(lua_State* st) {
    return 1;
}

BOOST_AUTO_TEST_CASE( read_write_c_function_pointer_from_table )
{
    TestLuaState lua;
    {
        Table tbl = lua.newTable();
        tbl["cfunction"] = cfunction;
        BOOST_CHECK_EQUAL(CFunction(tbl["cfunction"]), cfunction);
    }
}

BOOST_AUTO_TEST_CASE( check_internal_math_library )
{
    TestLuaState lua;
    lua.openlibs();
    auto math = lua["math"];
    BOOST_CHECK_EQUAL(math.type(), LUA_TTABLE);
}

BOOST_AUTO_TEST_CASE( use_internal_math_library )
{
    TestLuaState lua;
    lua.openlibs();
    Table math = lua["math"];
    Number pi = math["pi"];
    BOOST_REQUIRE(pi > 3.14 && pi < 3.15);
}

BOOST_AUTO_TEST_CASE( read_write_closure_upvalue )
{
    TestLuaState lua;
    {
        // has 1 upvalue
        lua.push(CClosure(cfunction, 1));
        Closure closure = lua[1];
        closure[1] = "uvval";
        BOOST_CHECK_EQUAL((const char*)closure[1], "uvval");
    }
}

BOOST_AUTO_TEST_CASE( getset_metatable )
{
    TestLuaState lua;
    {
        Table t = lua.newTable();

        Table mt = lua.newTable();

        t.setmetatable(mt);
        Table _mt = t.getmetatable();
        BOOST_REQUIRE(mt == _mt);
    }
}

BOOST_AUTO_TEST_CASE( read_write_userdata )
{
    TestLuaState lua;
    {
        Table tab = lua.newTable();
        UserData ud = lua.newUserData<double>(100);
        tab["ud"] = ud;

        UserData ud_2 = tab["ud"];
        BOOST_CHECK_EQUAL(ud_2.to<double>(), 100);
    }
}

BOOST_AUTO_TEST_CASE( read_write_light_userdata )
{
    TestLuaState lua;
    {
        Table tab = lua.newTable();
        tab["lud"] = static_cast<void*>(new double(101));
        double *p = static_cast<double*>((void*)tab["lud"]);
        BOOST_CHECK_EQUAL(*p, 101);
        delete p;
    }
}

BOOST_AUTO_TEST_CASE( call_lua_math_log )
{
    TestLuaState lua;
    {
        Table math = lua.open(luaopen_math);
        Closure log = math["log"];
        Number res = log(100);
        BOOST_CHECK_EQUAL(res, std::log(100));
    }
}

BOOST_AUTO_TEST_CASE( call_lua_math_log_through_luac )
{
    TestLuaState lua;
    lua["math"] = lua.open(luaopen_math);
    {
        Closure log = lua.newFunc("return math.log(...)");
        // lua automatically convert string to number
        BOOST_CHECK_EQUAL(Number( log("100") ), std::log(100));
    }
}

BOOST_AUTO_TEST_CASE( call_light_cfunction )
{
    TestLuaState lua;
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
}


BOOST_AUTO_TEST_CASE( call_cpp_lambda )
{
    TestLuaState lua;
    {
        Closure cl = lua.newCallable([](State& st, Number num) -> Number {
            return num + 7777;
        });
        Number in = rand();
        Number num = cl(in);
        BOOST_CHECK_EQUAL(num, in + 7777);
    }
}

BOOST_AUTO_TEST_CASE( call_cpp_lambda_and_check_side_effect )
{
    TestLuaState lua;
    { // return void
        Closure cl = lua.newCallable([](State& st, string&& key, Number num) {
            st[key] = num;
        });
        cl("hello", 12);
        BOOST_CHECK_EQUAL(Number(lua["hello"]), 12);
    }
}


BOOST_AUTO_TEST_CASE( lambda_return_multiple_values )
{
    TestLuaState lua;
    lua.openlibs();
    { // simple multiple return
        Closure cl = lua.newCallable([](State& st, Number a, Number b) {
            return make_tuple(b, a);
        });
        lua["test"] = cl;
        Closure recv = lua.newFunc(
                "local a, b = test(11, 12); return tostring(a)");
        Number ret = recv();
        BOOST_CHECK_EQUAL(ret, 12);
    }
}

BOOST_AUTO_TEST_CASE( multiple_return_values )
{
    TestLuaState lua;
    {
        // multiple return values cannot be automatically destructed from
        // lua stack, so make a new scope
        auto scope = lua.newScope();
        Closure cl = lua.newCallable([](State& st, Number a, Number b) {
            return make_tuple(b, a);
        });

        std::tuple<int, int> ret = cl(11, 12);
        BOOST_CHECK_EQUAL(std::get<0>(ret), 12);
        BOOST_CHECK_EQUAL(std::get<1>(ret), 11);
    }
}

BOOST_AUTO_TEST_CASE( lambda_with_short_arglist )
    // no State& in the beginning of argument list
{
    TestLuaState lua;
    {
        Closure cl = lua.newCallable([](Table&& pair) -> Number {
            return Number(pair["num1"]) + Number(pair["num2"]);
        });

        Table in = lua.newTable();
        in["num1"] = 12;
        in["num2"] = 13;

        Number num = cl(in);
        BOOST_CHECK_EQUAL(num, 25);
    }
}

BOOST_AUTO_TEST_CASE( tie_multiple_return_values_to_tuple )
{
    TestLuaState lua;
    {
        auto scope = lua.newScope();
        Closure cl = lua.newFunc("return 1, 2, 3, 5, 8, 13;");
        int a, b, d, e, f;
        luamm::tie(a, b, ignore, d, e, f) = cl.call();
        BOOST_CHECK_EQUAL(a+b+d+e+f, 29);
    }
}

BOOST_AUTO_TEST_CASE( trival_lambda )
{
    TestLuaState lua;
    {
        Closure cl = lua.newCallable([](int i, int j) {
                return i*j;
        });
        BOOST_CHECK_EQUAL(Number(cl(2,4)), 8);
        BOOST_CHECK_EQUAL(Number(cl(200,300)), 60000);
    }
}

BOOST_AUTO_TEST_CASE( cpp_class_to_lua_table )
{
    TestLuaState lua;
    {
        // test module system
        struct Hello {};
        Table mod = move(
            lua.class_<Hello>("hello")
            .def("add", [](int i, int j) { return i+j; })
            .init()
        );
        BOOST_CHECK_EQUAL((const char*)mod["className"], "hello");
        Closure add = lua.newFunc(R"==(
            local hello, a,b = ...
            local obj = hello()
            return obj.add(a, b)
        )==");
        BOOST_CHECK_EQUAL(Number(add(mod, 1, 2)), 3);
    }
}

BOOST_AUTO_TEST_CASE( class_member_function_binding )
{
    TestLuaState lua;
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
}


BOOST_AUTO_TEST_CASE( class_member_setter_getter_auto_generator )
{
    TestLuaState lua;
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
}

BOOST_AUTO_TEST_CASE( class_attribute_binding )
{
    TestLuaState lua;
    lua["_G"] = lua.open(luaopen_base);
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
}

BOOST_AUTO_TEST_CASE( long_arglist )
{
    TestLuaState lua;
    {
        // long parameter list
        auto scope = lua.newScope();
        Closure cl = lua.newCallable([](int a, int b, int c, int d, int e) {
            return make_tuple(e, d, c, b, a);
        });
        int a, b, c, d, e;
        luamm::tie(e, d, c, b, a) = cl(1,2,3,4,5);
        BOOST_CHECK_EQUAL(a, 1);
        BOOST_CHECK_EQUAL(b, 2);
        BOOST_CHECK_EQUAL(c, 3);
        BOOST_CHECK_EQUAL(d, 4);
        BOOST_CHECK_EQUAL(e, 5);
    }
}
