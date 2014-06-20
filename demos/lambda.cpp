#include "luamm.hpp"

using namespace luamm;

int main(int argc, char *argv[])
{
    NewState lua;
    lua.openlibs();

    std::string msg("world");
    lua["hello"] = [&](State& st) -> int {
        st.push(msg);
        return 1;
    };
    lua.loadstring("print(hello())");

    if (lua.pcall(0, 0) != LUA_OK) {
        const char *msg = lua[1];
        fprintf(stderr, "%s\n", msg);
        return 1;
    } else {
        fprintf(stderr, "stack top at %d\n", lua.top());
        return 0;
    }
}
