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
#include <boost/mpl/not.hpp>
#include <boost/mpl/logical.hpp>
#include <cstdlib>

using namespace luamm;
using namespace std;
using namespace boost::mpl;

BOOST_AUTO_TEST_CASE( PrePush_test )
{
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, const char*> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, const char*&> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, const char*&&> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, char const * const &> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, char[2]> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, const char[2]> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, std::string&> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, std::string&&> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<std::string>, const std::string&> ));

    BOOST_MPL_ASSERT(( PredPush<VarProxy<Number>, int> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<Number>, unsigned int> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<Number>, long> ));
    BOOST_MPL_ASSERT(( PredPush<VarProxy<Number>, double> ));
}

BOOST_AUTO_TEST_CASE( PredGet_test )
{
    BOOST_MPL_ASSERT(( PredGet<VarProxy<Number>, Number> ));
    BOOST_MPL_ASSERT(( PredGet<VarProxy<Number>, double> ));
    BOOST_MPL_ASSERT(( PredGet<VarProxy<Number>, int> ));
    BOOST_MPL_ASSERT(( PredGet<VarProxy<Number>, bool> ));
    BOOST_MPL_ASSERT(( not_<PredGet<VarProxy<Number>, void*>>::type ));


    BOOST_MPL_ASSERT(( PredGet<VarProxy<const char*>, string> ));
    BOOST_MPL_ASSERT(( PredGet<VarProxy<const char*>, const char*> ));
    BOOST_MPL_ASSERT(( not_<PredGet<VarProxy<const char*>, char*>>::type ));
}

BOOST_AUTO_TEST_CASE( Choose )
{
    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredPush, const char*>::impl,
                        string
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredGet, string>::impl,
                        const char*
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredGet, int>::impl,
                        Number
                      >));

    BOOST_MPL_ASSERT((std::is_same<
                        SelectImpl<PredGet, unsigned int>::impl,
                        Number
                      >));
}

BOOST_AUTO_TEST_CASE( Basic_Load )
{
    NewState lua;

    int num = rand();
    lua.push(num);
    BOOST_CHECK_EQUAL(Number(lua[-1]), num);

    lua.push("hello");
    BOOST_CHECK_EQUAL((const char*)lua[-1], "hello");

    lua.push(true);
    BOOST_REQUIRE( bool(lua[-1]) );
}

