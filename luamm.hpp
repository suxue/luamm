#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>

extern "C" const char *luamm_reader(lua_State *L, void *data, size_t *size);


namespace luamm {

    struct RuntimeError : std::runtime_error {
        RuntimeError(const std::string& msg) : std::runtime_error(msg) {}
    };

    typedef lua_Number Number;
    struct CClosure {
        lua_CFunction func;
        int upvalues;
        CClosure(lua_CFunction f = nullptr, int uv = 0)
            : func(f), upvalues(uv) {}
    };

    typedef std::pair<size_t, const char*> ReaderResult;
    typedef std::function<ReaderResult()> Reader;

    class Index {
        int index;
    public:
        Index() : index(0) {}
        Index(int i) : index(i) {}
        int get() const {
            if (index == 0) {
                throw std::out_of_range("access to index 0 is not permitted");
            }
            return index;
        }
        operator int() const { return index; }
        Index& operator++() { return operator+=(1); }
        Index& operator+=(int x) { // towards top
            if (index > 0) {
                index += x;
                if (index < 0) index = 0;
            } else {
                index -= x;
                if (index > 0) index = 0;
            }
            return *this;
        }


        Index& operator-=(int x) { // towards bottom
            if (index < 0) {
                index += x;
                if (index > 0) index = 0;
            } else {
                index -= x;
                if (index < 0) index = 0;
            }
            return *this;
        }
        Index& operator--() { return operator-=(1); }
    };

    class Nil {};

    template<typename T> struct VarTypeTrait {
        static const bool isvar = false;
        typedef T type;
    };

    struct ValidVarType {static const bool isvar = true; };

    template<>
    struct VarTypeTrait<const char *> : public ValidVarType {
        typedef const char * type;
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, const type& str) {
            lua_pushstring(st, str);
        }

        static bool get(lua_State* st, type& out, int index) {
            return (out = lua_tostring(st, index)) != nullptr;
        }
    };

    template<>
    struct VarTypeTrait<Nil> : public ValidVarType {
        typedef Nil type;
        enum { tid = LUA_TNIL };
        static void push(lua_State* st, const type& _) {
            lua_pushnil(st);
        }

        static bool get(lua_State* st, type& out, int index) {
            return !!lua_isnil(st, index);
        }
    };

    template<>
    struct VarTypeTrait<Number> : public ValidVarType {
        typedef Number type;
        enum { tid = LUA_TNUMBER };
        static void push(lua_State* st, const type& num) {
            lua_pushnumber(st, num);
        }

        static bool get(lua_State* st, type& out, int index) {
            int isnum;
            type num = lua_tonumberx(st, index, &isnum);
            if (isnum) {
                out = num;
                return true;
            } else {
                return false;
            }
        }
    };

    template<>
    struct VarTypeTrait<CClosure> : public ValidVarType {
        typedef CClosure type;
        enum { tid = LUA_TFUNCTION };
        static void push(lua_State* st, const type& closure) {
            lua_pushcclosure(st, closure.func, closure.upvalues);
        }

        static bool get(lua_State* st, type& out, int index) {
            auto f = lua_tocfunction(st, index);
            if (f) {
                out.func = f;
                out.upvalues = -1;
                return true;
            } else {
                return false;
            }
        }
    };

    template<>
    struct VarTypeTrait<bool> : public ValidVarType {
        typedef bool type;
        enum { tid = LUA_TBOOLEAN };
        static void push(lua_State* st, const type& b) {
            lua_pushboolean(st, b);
        }

        static bool get(lua_State* st, type& out, int index) {
            out = lua_toboolean(st, index) == 0 ? false : true;
            return true;
        }
    };

    template<>
    struct VarTypeTrait<void*> : public ValidVarType {
        typedef void* type;
        enum { tid = LUA_TLIGHTUSERDATA };
        static void push(lua_State* st, const type& u) {
            lua_pushlightuserdata(st, u);
        }

        static bool get(lua_State* st, type& out, int index) {
            out = lua_touserdata(st, index);
            return !!out;
        }
    };

    class StackIndex : public Index {
    public:
        StackIndex(int i) : Index(i) {}
    };

    class BottomIndex : public StackIndex {
    public:
       BottomIndex() : StackIndex(1) {}
    };

    class TopIndex : public StackIndex {
    public:
        TopIndex() : StackIndex(-1) {}
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


    class State {
    protected:
        lua_State *ptr;
    public:
        class ReturnValue {
            State *state;
            Index index;
        public:
            class TypeError : public RuntimeError {
                public:
                    TypeError(ReturnValue *p, const std::string& msg)
               : RuntimeError(std::string("type mismatch, expect") + msg) {}
            };
            ReturnValue(State *s, int i) : state(s), index(i) {}

            template<typename T>
            operator T&&() {
                static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
                T out;
                if (! VarTypeTrait<T>::get(state->ptr, out, index)) {
                    throw RuntimeError("get error");
                }
                return std::move(out);
            }

            template<typename T>
            ReturnValue& operator=(const T& value) {
                static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
                auto i = index.get();
                auto absi = std::abs(i);
                if (absi < 1 || absi > state->top()) {
                    throw RuntimeError("not a valid index");
                }
                VarTypeTrait<T>::push(state->ptr, value);
                state->replace(StackIndex(index.get()));
                return *this;
            }

            int type() {
                return lua_type(state->ptr, index);
            }
        };

        State(lua_State *ref) : ptr(ref) {}
        ~State() {}

        int pcall(int nargs, int nresults, int msgh = 0) {
            return lua_pcall(ptr, nargs, nresults, msgh);
        }

        void copy(Index from, StackIndex to) {
            lua_copy(ptr, from, to);
        };

        void openlibs() {
            luaL_openlibs(ptr);
        }

        template<typename T>
        void push(const T& v) {
            static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
            VarTypeTrait<T>::push(ptr, v);
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

        ReturnValue at(Index i) {
            return ReturnValue(this, i.get());
        }

        ReturnValue operator[](Index i) {
            return at(i);
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
