#include "luamm.hpp"
#include <assert.h>
using namespace std;

namespace luamm {
int luamm_cclosure(lua_State* _)
{
    State st(_);
    UserData ud = st[st.upvalue(1)];
    auto lambda = ud.to<lua_Lambda>();
    return lambda->operator()(_);
}

int luamm_cleanup(lua_State* _)
{
    assert(lua_gettop(_) == 1);
    State st(_);
    UserData ud = st[1];
    auto lambda = ud.to<lua_Lambda>();
    lambda->~lua_Lambda();
    return 0;
}

}
