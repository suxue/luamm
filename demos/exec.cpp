#include "luamm.hpp"
#include <cstdio>
#include <utility>
#include <string>
#include <iostream>

using namespace std;
using namespace luamm;

void init(State& lua)
{
    lua["hello"] = string("hello world, its a nice day, is it?");

    lua["world"] = 1;

    Table tab = lua.pushTable();

    lua["mytab"] = tab;

    tab["hello"] = "world";

    string a = lua["world"];

    tab[1] = "world";

    tab[false] = 1;
    lua.pop();
}

int main(int argc, char *argv[])
{
    NewState lua;
    lua.openlibs();
    init(lua);

    if (argc == 2) {
        auto filename = argv[1];
        lua.loadfile(filename);
    } else {
        lua.load([]() -> ReaderResult {
            static char buf;
            buf = getc(stdin);
            if (buf != EOF) {
                return ReaderResult(1, &buf);
            } else {
                return ReaderResult(0, nullptr);
            }
        }, "stdin");
    }

    if (lua.pcall(0, 0) != LUA_OK) {
        const char *msg = lua[1];
        fprintf(stderr, "%s\n", msg);
    }
}
