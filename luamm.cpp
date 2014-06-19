#include "luamm.hpp"
#include <string>

using namespace std;

const char *
luamm_reader(lua_State *L, void *data, size_t *size)
{
    auto reader = static_cast<luamm::Reader*>(data);
    auto rpair = (*reader)();
    *size = rpair.first;
    return rpair.second;
}


int luamm::State::loadstring(const std::string& str)
{
    return luaL_loadstring(ptr, str.c_str());
}

int
luamm::State::loadfile(const std::string& file)
{
    return luaL_loadfile(ptr, file.c_str());
}
