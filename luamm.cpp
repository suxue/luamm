#include "luamm.hpp"

const char *
luamm_reader(lua_State *L, void *data, size_t *size)
{
    auto reader = static_cast<luamm::Reader*>(data);
    auto rpair = (*reader)();
    *size = rpair.first;
    return rpair.second;
}
