#include "luamm.hpp"
#include <string>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

using namespace luamm;
using namespace std;
using boost::system::error_code;
namespace fs = boost::filesystem;

const char *path_registry_key = "boost_filesystem_path";

namespace types {
    void make_path(State& st, UserData& path)
    {
        Table mtab = st.registry()[path_registry_key];
        path.set(mtab);
    }
}

namespace metatable {
    string path_tostring(State& st, UserData&& path) {
        return path.to<fs::path>().string();
    }

    Closure path_index(State& st, UserData&& ud, const char *key) {
        Table mtab = ud.get();
        return mtab[key];
    }

    UserData path_root_name(State& st, UserData&& path) {
        auto root_name = path.to<fs::path>().root_name();
        UserData np = st.newUserData<fs::path>(root_name);
        types::make_path(st, np);
        return np;
    }

    UserData path_filename(State& st, UserData&& path) {
        auto filename = path.to<fs::path>().filename();
        UserData np = st.newUserData<fs::path>(filename);
        types::make_path(st, np);
        return np;
    }

    UserData path_parent_path(State& st, UserData&& path) {
        auto parent_path = path.to<fs::path>().parent_path();
        UserData np = st.newUserData<fs::path>(parent_path);
        types::make_path(st, np);
        return np;
    }

    UserData path_relative_path(State& st, UserData&& path) {
        auto relative_path = path.to<fs::path>().relative_path();
        UserData np = st.newUserData<fs::path>(relative_path);
        types::make_path(st, np);
        return np;
    }

    UserData path_extension(State& st, UserData&& path) {
        auto extension = path.to<fs::path>().extension();
        UserData np = st.newUserData<fs::path>(extension);
        types::make_path(st, np);
        return np;
    }

    UserData path_root_path(State& st, UserData&& path) {
        auto root_path = path.to<fs::path>().root_path();
        UserData np = st.newUserData<fs::path>(root_path);
        types::make_path(st, np);
        return np;
    }

    UserData path_root_directory(State& st, UserData&& path) {
        auto root_directory = path.to<fs::path>().root_directory();
        UserData np = st.newUserData<fs::path>(root_directory);
        types::make_path(st, np);
        return np;
    }

    bool path_empty(State& st, UserData&& path) {
        return path.to<fs::path>().empty();
    }

    bool path_is_absolute(State& st, UserData&& path) {
        return path.to<fs::path>().is_absolute();
    }

    UserData path_concat(State& st, UserData&& p1, UserData&& p2) {
        fs::path np = p1.to<fs::path>();
        np  += p2.to<fs::path>();
        UserData ud = st.newUserData<fs::path>(np);
        types::make_path(st, ud);
        return ud;
    }

    string path_native(State&, UserData&& p) {
        return p.to<fs::path>().native();
    }

    string path_generic(State&, UserData&& p) {
        return p.to<fs::path>().generic_string();
    }

    int path_compare(State&, UserData&& a, UserData&& b) {
        return a.to<fs::path>().compare(b.to<fs::path>());
    }

    bool path_equal(State& _, UserData&& a, UserData&& b) {
        return path_compare(_,
                forward<UserData>(a),
                forward<UserData>(b)) == 0;
    }

    bool path_less(State& _, UserData&& a, UserData&& b) {
        return path_compare(_,
                forward<UserData>(a),
                forward<UserData>(b)) < 0;
    }

    bool path_le(State& _, UserData&& a, UserData&& b) {
        return path_compare(_,
                forward<UserData>(a),
                forward<UserData>(b)) <= 0;
    }


    Table path(State& st)
    {
        Table mtab = st.newTable();
        mtab["__tostring"] = st.newCallable(path_tostring);
        mtab["__index"] = mtab;
        mtab["__concat"] = st.newCallable(path_concat);
        mtab["__eq"] = st.newCallable(path_equal);
        mtab["__le"] = st.newCallable(path_le);
        mtab["__lt"] = st.newCallable(path_less);
        mtab["root_name"] = st.newCallable(path_root_name);
        mtab["filename"] = st.newCallable(path_filename);
        mtab["parent_path"] = st.newCallable(path_parent_path);
        mtab["relative_path"] = st.newCallable(path_relative_path);
        mtab["extension"] = st.newCallable(path_extension);
        mtab["root_path"] = st.newCallable(path_root_path);
        mtab["root_directory"] = st.newCallable(path_root_directory);
        mtab["empty"] = st.newCallable(path_empty);
        mtab["is_absolute"] = st.newCallable(path_is_absolute);
        mtab["native"] = st.newCallable(path_native);

        {
            st.push(CClosure([](lua_State* _) {
                State st(_);
                if (!st[1].isuserdata()) {
                    st.error("arg#1 should be a path userdata");
                }

                bool isreverse = false;
                if (st.top() == 2 && st[2].isbool()) {
                    isreverse = st[2];
                }

                UserData self = st[1];
                auto& path = self.to<fs::path>();

                st.push(CClosure([](lua_State* _) -> int {
                    State st(_);

                    bool isreverse = st[1];

                    UserData ud_p = st[st.upvalue(1)];
                    UserData ud_e = st[st.upvalue(2)];

                    auto& p = ud_p.to<fs::path::iterator>();
                    auto& e = ud_e.to<fs::path::iterator>();
                    if (p == e) {
                        st.push(Nil());
                    } else if (!isreverse) {
                        UserData ret = st.newUserData<fs::path>(*p++);
                        types::make_path(st, ret);
                        st.push(ret);
                    } else {
                        UserData ret = st.newUserData<fs::path>(*--p);
                        types::make_path(st, ret);
                        st.push(ret);
                    }

                    return 1;
                }, 3));
                Closure f = st[-1];

                if (isreverse){
                    f[1] = st.newUserData<fs::path::iterator>(path.end());
                    f[2] = st.newUserData<fs::path::iterator>(path.begin());
                } else {
                    f[1] = st.newUserData<fs::path::iterator>(path.begin());
                    f[2] = st.newUserData<fs::path::iterator>(path.end());
                }

                st.push(f);
                st.push(isreverse);
                st.push(Nil());
                return 3;
            }));
            Closure each = st[-1];
            mtab["each"] = each;
        }

        return mtab;
    }
}

namespace constructor {
    UserData path(State& st, const char *pathstr)
    {
        UserData new_path = st.newUserData<fs::path>(pathstr);
        types::make_path(st, new_path);
        return new_path;
    }
}


Table reg(State& st)
{
    {
        Table path_mtab = metatable::path(st);
        st.registry()[path_registry_key] = path_mtab;
    }
    Table tab = st.newTable();
    tab["path"] = st.newCallable(constructor::path);

    return tab;
}


extern "C" int luaopen_fs(lua_State *L) {
    State st(L);

    const char *modname = st[1];
    const char *modfile = st[2];

    Table mod = reg(st);
    mod["modname"] = modname;
    mod["modfile"] = modfile;

    st[1] = mod;
    st.settop(1);
    return 1;
}
