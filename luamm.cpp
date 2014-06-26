#include "luamm.hpp"

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
    State st(_);
    UserData ud = st[st.upvalue(1)];
    delete ud.to<lua_Lambda>();
    return 0;
}

}
