#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <functional>
#include <utility>

extern "C" const char *luamm_reader(lua_State *L, void *data, size_t *size);


namespace luamm {

    class RuntimeError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    typedef lua_Number Number;
    typedef lua_CFunction CFunction;

    typedef std::pair<size_t, const char*> ReaderResult;
    typedef std::function<ReaderResult()> Reader;

    class Index {
        const int index;
    protected:
        Index(int i) : index(i) {}
    public:
        int get() const { return index; }
    };

    class StackIndex : public Index {
    public:
        StackIndex(unsigned i) : Index(i) {}
    };

    class UpvalueIndex : public Index {
    public:
        UpvalueIndex(int i) : Index(lua_upvalueindex(i)) {}
    };

    //! access the registry
    class RegistryIndex : public Index {
    public:
        RegistryIndex() : Index(LUA_REGISTRYINDEX) {}
    };

    //! access items inside the registry table
    class RIndex : public Index {
    protected:
        RIndex(int i) : Index(i) {}
    };

    class MainThreadIndex : public RIndex {
    public:
        MainThreadIndex() : RIndex(LUA_RIDX_MAINTHREAD) {}
    };

    class GlobalsIndex : public RIndex {
    public:
        GlobalsIndex() : RIndex(LUA_RIDX_GLOBALS) {}
    };

    struct Types {
        enum type {
            Nil = LUA_TNIL,
            Number = LUA_TNUMBER,
            Boolean = LUA_TBOOLEAN,
            String = LUA_TSTRING,
            Table = LUA_TTABLE,
            Function = LUA_TFUNCTION,
            Userdata = LUA_TUSERDATA,
            Thread = LUA_TTHREAD,
            LightUserdata = LUA_TLIGHTUSERDATA
        };
    };

    class State {
    protected:
        lua_State *ptr;
    public:
        class ReturnValue {
            State *state;
            int index;
        public:
            class TypeError : public RuntimeError {
                public:
                    TypeError(ReturnValue *p, const std::string& msg) :
                        RuntimeError(std::string("#") +
                             std::to_string(p->index) + " is not " + msg) {}
            };
            ReturnValue(State *s, int i) : state(s), index(i) {}
            operator std::string() {
                size_t len;
                auto r = lua_tolstring(state->ptr, index, &len);
                if (r)
                    return std::string(r, len);
                else
                    throw TypeError(this, "string");
            }
            operator Number() {
                int isnum;
                auto r = lua_tonumberx(state->ptr, index, &isnum);
                if (isnum) return r; else throw TypeError(this, "number");
            }
            operator void*() {
                auto r = lua_touserdata(state->ptr, index);
                if (r)
                    return r;
                else
                    throw TypeError(this, "userdata");
            }
            operator bool() {
                return !!lua_toboolean(state->ptr, index);
            }
            operator CFunction() {
                auto func = lua_tocfunction(state->ptr, index);
                if (func)
                    return func;
                else
                    throw TypeError(this, "cfunction");
            }

        };

        State(lua_State *ref) : ptr(ref) {}
        ~State() {}

        int pcall(int nargs, int nresults, int msgh = 0) {
            return lua_pcall(ptr, nargs, nresults, msgh);
        }

        void copy(Index from, StackIndex to) {
            lua_copy(ptr, from.get(), to.get());
        };

        void openlibs() {
            luaL_openlibs(ptr);
        }

        void push(bool v) {
            lua_pushboolean(ptr, v);
        }

        void push(const std::string& value) {
            lua_pushstring(ptr, value.c_str());
        }

        void push(void *light_userdata) {
            lua_pushlightuserdata(ptr, light_userdata);
        }

        void push(const char *s, std::size_t len) {
            lua_pushlstring(ptr, s, len);
        }

        void push(Number num) {
            lua_pushnumber(ptr, num);
        }

        void push(CFunction func, std::uint8_t upvalues = 0) {
            lua_pushcclosure(ptr, func, upvalues);
        }

        void push(GlobalsIndex g) {
            lua_pushglobaltable(ptr);
        }

        void pushNil() {
            lua_pushnil(ptr);
        }

        void pushTable(int narray = 0, int nother = 0) {
            lua_createtable(ptr, narray, nother);
        }

        void remove(StackIndex i) {
            lua_remove(ptr, i.get());
        }

        void replace(StackIndex i) {
            lua_replace(ptr, i.get());
        }

        ReturnValue get(Index i) {
            return ReturnValue(this, i.get());
        };

        Types::type type(Index i) {
            return static_cast<Types::type>(lua_type(ptr, i.get()));
        }

        const Number version() {
            return *lua_version(ptr);
        }

        void pop(int n) {
            lua_pop(ptr, n);
        }

        //! stack size, or top index (because lua table is 1-based)
        int top() {
            return lua_gettop(ptr);
        }

        int load(Reader reader,
                const std::string source, const char *mode = nullptr) {
            return lua_load(ptr, luamm_reader, &reader, source.c_str(), mode);
        }

    private:
        State(const State&);
        State& operator=(const State&);
    };

    class NewState : public State {
    public:
        NewState() : State(luaL_newstate()) {
            if (!ptr) { throw RuntimeError("allocate new lua state"); }
        }
        ~NewState() {
            lua_close(ptr);
        }
    };
}

#endif
