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
    auto r = lua.load([]() -> ReaderResult {
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
        lua["hello"] = true;
        if (lua.pcall(0, 0) != LUA_OK) {
            const char * msg  = lua[1];
            printf("%s\n", msg);
        }
    } else if (r == LUA_ERRSYNTAX) {
        const char * msg  = lua[ Index::bottom()];
        printf("%s\n", msg);
    } else { }
    return 0;
}
