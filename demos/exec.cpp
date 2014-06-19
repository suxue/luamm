#include "luamm.hpp"
#include <cstdio>
#include <utility>
#include <string>

using namespace std;
using namespace luamm;

void init(State& lua)
{
    lua["hello"] = "world";
    lua["world"] = 1;
}

int main(int argc, char *argv[])
{
    NewState lua;
    lua.openlibs();
    init(lua);
    int r;

    if (argc == 2) {
        auto filename = argv[1];
        r = lua.loadfile(filename);
    } else {
        r = lua.load([]() -> ReaderResult {
            static char buf;
            buf = getc(stdin);
            if (buf != EOF) {
                return ReaderResult(1, &buf);
            } else {
                return ReaderResult(0, nullptr);
            }
        }, "stdin");
    }

    if (r == LUA_OK) {
        if (lua.pcall(0, 0) != LUA_OK) {
            const char *msg = lua[1];
            fprintf(stderr, "%s\n", msg);
        } else {
            return 0;
        }
    } else if (r == LUA_ERRSYNTAX) {
        const char * msg  = lua[ Index::bottom()];
        printf("%s\n", msg);
    } else { }
}
