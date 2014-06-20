#include "luamm.hpp"
#include <string>
#include <iostream>

using namespace std;

const char *
luamm_reader(lua_State *L, void *data, size_t *size)
{
    auto reader = static_cast<luamm::Reader*>(data);
    auto rpair = (*reader)();
    *size = rpair.first;
    return rpair.second;
}

int luamm_function_cleaner(lua_State* L)
{
    luamm::State st(L);
    lua_getupvalue(L, -1, 1); // +1
    luamm::UserData ud = st[-1];
    auto fp = ud.get<luamm::Function*>();
    delete fp;
    lua_pop(L, 2);
    return 0;
}

int luamm_closure(lua_State *L)
{
    luamm::State st(L);
    luamm::UserData ud = st[luamm::Index::upvalue(1)];
    auto  fp = *ud.get<luamm::Function*>();
    return (*fp)(st);
}

void luamm::State::loadstring(const std::string& str)
{
    load_error(luaL_loadstring(ptr, str.c_str()));
}

void
luamm::State::loadfile(const std::string& file)
{
    load_error(luaL_loadfile(ptr, file.c_str()));
}
