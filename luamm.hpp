#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include <ostream>
#include <type_traits>

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

    inline std::ostream& operator<<(std::ostream out, CClosure closure) {
        return out << "luamm::closure(" << closure.func << "("
                    << closure.upvalues << ")";
    }

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
        static Index bottom() { return Index(1); }
        static Index top() { return Index(-1); }
        static Index upvalue(int i) { return Index(lua_upvalueindex(i)); }
        static Index registry() { return Index(LUA_REGISTRYINDEX); }
        static Index mainThread() { return Index(LUA_RIDX_MAINTHREAD); }
        static Index globals() { return Index(LUA_RIDX_GLOBALS); }
    };

    class Nil {};
    inline std::ostream& operator<<(std::ostream& out, Nil nil) {
        return out << "luamm::Nil";
    };

    template<typename T> struct VarTypeTrait;

    struct ValidVarType {
        static const bool isvar = true;
    };


    template<typename T>
    struct PassConvention {
        enum { issimple = std::is_trivial<T>::value && sizeof(T) <= sizeof(void*)  };
        typedef typename std::conditional<issimple, T, const T&>::type intype;
        typedef T& outtype;
    };

    template<>
    struct VarTypeTrait<const char *> : public ValidVarType {
        typedef const char * keytype;
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, PassConvention<keytype>::intype str) {
            lua_pushstring(st, str);
        }

        static bool get(lua_State* st, PassConvention<keytype>::outtype  out, int index) {
            return (out = lua_tostring(st, index)) != nullptr;
        }
    };

    template<>
    struct VarTypeTrait<std::string> : public ValidVarType {
        typedef std::string keytype;
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, const keytype& str) {
            return VarTypeTrait<const char*>::push(st, str.c_str());
        }

        static bool get(lua_State* st, keytype& out, int index) {
            const char *o = lua_tostring(st, index);
            if (o) {
                out = o;
                return true;
            } else {
                return false;
            }
        }
    };

    template<>
    struct VarTypeTrait<Nil> : public ValidVarType {
        typedef Nil keytype;
        enum { tid = LUA_TNIL };
        static void push(lua_State* st, const keytype& _) {
            lua_pushnil(st);
        }

        static bool get(lua_State* st, keytype& out, int index) {
            return !!lua_isnil(st, index);
        }
    };

    template<>
    struct VarTypeTrait<Number> : public ValidVarType {
        typedef Number keytype;
        enum { tid = LUA_TNUMBER };
        static void push(lua_State* st, keytype num) {
            lua_pushnumber(st, num);
        }

        static bool get(lua_State* st, keytype& out, int index) {
            int isnum;
            keytype num = lua_tonumberx(st, index, &isnum);
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
        typedef CClosure keytype;
        enum { tid = LUA_TFUNCTION };
        static void push(lua_State* st, const keytype& closure) {
            lua_pushcclosure(st, closure.func, closure.upvalues);
        }

        static bool get(lua_State* st, keytype& out, int index) {
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
        typedef bool keytype;
        enum { tid = LUA_TBOOLEAN };
        static void push(lua_State* st, keytype b) {
            lua_pushboolean(st, b);
        }

        static bool get(lua_State* st, keytype& out, int index) {
            out = lua_toboolean(st, index) == 0 ? false : true;
            return true;
        }
    };

    template<>
    struct VarTypeTrait<void*> : public ValidVarType {
        typedef void* keytype;
        enum { tid = LUA_TLIGHTUSERDATA };
        static void push(lua_State* st, const keytype& u) {
            lua_pushlightuserdata(st, u);
        }

        static bool get(lua_State* st, keytype& out, int index) {
            out = lua_touserdata(st, index);
            return !!out;
        }
    };

    template<typename T>
    struct VarTypeTrait {
        static const bool isvar = std::is_convertible<T, Number>::value;

        static void push(lua_State* st, const T& u) {
            VarTypeTrait<Number>::push(st, u);
        }

        static bool get(lua_State* st, T& out, int index) {
            Number tmp;
            if (VarTypeTrait<Number>::get(st, tmp, index)) {
                out = tmp;
                return true;
            } else {
                return false;
            }
        }
    };



    template<typename Key>
    struct VarSetterGetter;

    struct DummyVarSetterGetter {};

    template<>
    struct VarSetterGetter<Index> {
        typedef const Index& Key;
        // get key to out
        template<typename T>
        static bool get(lua_State* st, T& out, Key key) {
            return VarTypeTrait<T>::get(st, out, key);
        }

        // set key to _in
        template<typename T>
        static void set(lua_State* st, const T& _in, Key key) {
            VarTypeTrait<T>::push(st, _in);
            lua_replace(st, key);
        }
    };

    template<>
    struct VarSetterGetter<const std::string&> {
        typedef const std::string& Key;
        template<typename T>
        static bool get(lua_State* st, T& out, const Key& key) {
            lua_getglobal(st, key.c_str());
            return VarTypeTrait<T>::get(st, out, Index::top());
        }

        template<typename T>
        static void set(lua_State* st, const T& _in, const Key& key) {
            VarTypeTrait<T>::push(st, _in);
            lua_setglobal(st, key.c_str());
        }
    };

    template<typename Key>
    struct VarKeyMatcher {
        typedef typename std::conditional<std::is_convertible<Key, Index>::value,
            VarSetterGetter<Index>,
            typename std::conditional<
                std::is_convertible<Key, const std::string&>::value,
                VarSetterGetter<const std::string&>,
                DummyVarSetterGetter
            >::type
        >::type type;
    };

    template<typename Key>
    struct VarSetterGetter {
        template<typename T>
        static bool get(lua_State* st, T& out, Key key) {
            return VarKeyMatcher<Key>::type::get(st, out, key);
        }

        template<typename T>
        static void set(lua_State* st, const T& _in, const Key& key) {
            typedef typename VarKeyMatcher<Key>::type imp;
            imp::set(st, _in, key);
        }
    };

    class State {
    protected:
        lua_State *ptr;
    public:
        template<typename Key>
        class ReturnValue {
            State *state;
            Key key;
        public:
            class TypeError : public RuntimeError {
                public:
                    TypeError(ReturnValue *p, const std::string& msg)
               : RuntimeError(std::string("type mismatch, expect") + msg) {}
            };
            ReturnValue(State *s, Key k) : state(s), key(k) {}

            template<typename T>
            operator T&&() {
                static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
                T out;
                if (! VarSetterGetter<Key>::get(state->ptr, out, key)) {
                    throw RuntimeError("get error");
                }
                return std::move(out);
            }

            template<typename T>
            ReturnValue& operator=(const T& value) {
                static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
                VarSetterGetter<Key>::set(state->ptr, value, key);
                return *this;
            }

            int type() {
                return lua_type(state->ptr, key);
            }
        };

        State(lua_State *ref) : ptr(ref) {}
        ~State() {}

        int pcall(int nargs, int nresults, int msgh = 0) {
            return lua_pcall(ptr, nargs, nresults, msgh);
        }

        void copy(Index from, Index to) {
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

        void remove(Index i) {
            lua_remove(ptr, i.get());
        }

        void replace(Index i) {
            lua_replace(ptr, i.get());
        }

        template<typename Key>
        ReturnValue<Key> operator[](Key i) {
            return ReturnValue<Key>(this, i);
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
