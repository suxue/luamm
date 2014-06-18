#include "luamm.hpp"
#include <cstdio>
#include <utility>
#include <string>

using namespace std;
using namespace luamm;

int main()
{
    NewState lua;
    lua.openlibs();
    auto r = lua.load([]() {
        static char buf;
        buf = getchar();
        if (buf != EOF) {
            return ReaderResult(1, &buf);
        } else {
            return ReaderResult(0, nullptr);
        }
    }, "stdin");
    if (r == LUA_OK) {
        // printf("%s\n", "ok");
        if (lua.pcall(0, 0) != LUA_OK) {
            string msg = lua.get(StackIndex(1));
            printf("%s\n", msg.c_str());
        }
    } else if (r == LUA_ERRSYNTAX) {
        string msg = lua.get(StackIndex(1));
        printf("%s\n", msg.c_str());
    } else { }
    return 0;
}
