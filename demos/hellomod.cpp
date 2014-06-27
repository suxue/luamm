// demonstrate how to write a lua c module with the help of luamm

#include "luamm.hpp"
#include <assert.h>
#include <iostream>
using namespace luamm;
using namespace std;

namespace {

    Table reg(State& st) {
        Table mod = st.newTable();
        st.push(Nil());

        mod["add"] = st.newCallable([](State& st, Number a, Number b) {
            return a+b;
        });
        return mod;
    }
}

extern "C" int luaopen_hello(lua_State *L) {
    State st(L);
    st.allocate(1); // allocate return value

    const char *modname = st[2];
    const char *modfile = st[3];

    Table mod = reg(st);
    cerr << "load " << modname << " from " << modfile << endl;

    st[1] = mod;
    st.settop(1);
    return 1;
}
