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
        RuntimeError(const std::string& msg);
    };

    typedef lua_Number Number;
    struct CClosure {
        lua_CFunction func;
        int upvalues;
        CClosure(lua_CFunction f = nullptr, int uv = 0);
    };

    typedef std::pair<size_t, const char*> ReaderResult;
    typedef std::function<ReaderResult()> Reader;

    class Index {
        int index;
    public:
        Index() : index(0) {}
        Index(int i);
        int get() const;
        operator int() const;
        static Index bottom();
        static Index top();
        static Index upvalue(int i);
        static Index registry();
        static Index mainThread();
        static Index globals();
    };

    class Nil {};

    template<typename T> struct VarTypeTrait;

    struct ValidVarType {
        static const bool isvar = true;
    };

    template<>
    struct VarTypeTrait<const char *> : public ValidVarType {
        typedef const char * vartype;
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, vartype str) {
            lua_pushstring(st, str);
        }

        static bool get(lua_State* st, vartype&  out, int index) {
            return (out = lua_tostring(st, index)) != nullptr;
        }
    };

    template<>
    struct VarTypeTrait<std::string> : public ValidVarType {
        typedef std::string vartype;
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, const vartype& str) {
            return VarTypeTrait<const char*>::push(st, str.c_str());
        }

        static bool get(lua_State* st, vartype& out, int index) {
            const char *o = lua_tostring(st, index);
            if (o) {
                out = o;
                return true;
            } else {
                return false;
            }
        }
    };

    template<std::size_t N>
    struct VarTypeTrait<char[N]> : public ValidVarType {
        typedef char vartype[N];
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, const vartype& str) {
            return VarTypeTrait<const char*>::push(st, (const char*)&str);
        }
    };

    template<std::size_t N>
    struct VarTypeTrait<const char [N]> : public ValidVarType {
        typedef const char vartype[N];
        enum { tid = LUA_TSTRING };
        static void push(lua_State* st, const vartype& str) {
            return VarTypeTrait<const char*>::push(st, (const char*)&str);
        }
    };

    template<>
    struct VarTypeTrait<Nil> : public ValidVarType {
        typedef Nil vartype;
        enum { tid = LUA_TNIL };
        static void push(lua_State* st, const vartype& _) {
            lua_pushnil(st);
        }

        static bool get(lua_State* st, vartype& out, int index) {
            return !!lua_isnil(st, index);
        }
    };

    template<>
    struct VarTypeTrait<Number> : public ValidVarType {
        typedef Number vartype;
        enum { tid = LUA_TNUMBER };
        static void push(lua_State* st, vartype num) {
            lua_pushnumber(st, num);
        }

        static bool get(lua_State* st, vartype& out, int index) {
            int isnum;
            vartype num = lua_tonumberx(st, index, &isnum);
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
        typedef CClosure vartype;
        enum { tid = LUA_TFUNCTION };
        static void push(lua_State* st, const vartype& closure) {
            lua_pushcclosure(st, closure.func, closure.upvalues);
        }

        static bool get(lua_State* st, vartype& out, int index) {
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
        typedef bool vartype;
        enum { tid = LUA_TBOOLEAN };
        static void push(lua_State* st, vartype b) {
            lua_pushboolean(st, b);
        }

        static bool get(lua_State* st, vartype& out, int index) {
            out = lua_toboolean(st, index) == 0 ? false : true;
            return true;
        }
    };

    template<>
    struct VarTypeTrait<void*> : public ValidVarType {
        typedef void* vartype;
        enum { tid = LUA_TLIGHTUSERDATA };
        static void push(lua_State* st, const vartype& u) {
            lua_pushlightuserdata(st, u);
        }

        static bool get(lua_State* st, vartype& out, int index) {
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

    struct DummyVarSetterGetter {
        static const bool iskey = false;
    };

    template<>
    struct VarSetterGetter<Index> {
        typedef const Index& Key;
        static const bool iskey = true;
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
        static const bool iskey = true;
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

    template<typename T>
    struct StorageType {
        typedef typename std::conditional<
            std::is_trivial<T>::value && sizeof(T) <= 2*sizeof(void*),
            T, const T&>::type type;
    };

    class State;
    class Table {
        friend VarTypeTrait<Table>;
        lua_State  *state;
        Index  index;
    public:
        Table(lua_State *state, Index index) : state(state), index(index) {}

        template<typename K>
        class ReturnValue {
            Table *table;
            typename StorageType<K>::type key;
        public:
            ReturnValue(Table* table, decltype(key) key) : table(table), key(key) {}
            template<typename V>
            operator V&&();

            template<typename V>
            ReturnValue<K> operator=(const V& v);
        };

        template<typename K>
        ReturnValue<K> operator[](K key) {
            static_assert(VarTypeTrait<K>::isvar, "key is not a var type");
            return ReturnValue<K>(this, key);
        }
    };


    template<>
    struct VarTypeTrait<Table> : public ValidVarType {
        typedef Table vartype;
        enum { tid = LUA_TTABLE };
        static void push(lua_State* st, vartype tab) {
            lua_pushnil(st);
            lua_copy(st, tab.index, Index::top());
        }

        static bool get(lua_State* st, vartype&  out, int index);
    };

    class State {
        friend Table;
    protected:
        lua_State *ptr;
    public:
        template<typename Key>
        class ReturnValue {
            typedef typename VarKeyMatcher<Key>::type Accessor;
            static_assert(Accessor::iskey, "not a valid key type");
            State *state;
            typename StorageType<Key>::type key;
        public:
            class TypeError : public RuntimeError {
            public:
                TypeError(ReturnValue *p, const std::string& msg);
            };
            ReturnValue(State *s, Key k);

            template<typename T>
            operator T&&();

            template<typename T>
            ReturnValue& operator=(const T& value);

            int type();
        };

        State(lua_State *ref);
        ~State() {}

        int pcall(int nargs, int nresults, int msgh = 0);

        void copy(Index from, Index to);

        void openlibs();

        template<typename T>
        void push(const T& v);

        Table pushTable(int narray = 0, int nother = 0);

        void remove(Index i);

        void replace(Index i);

        template<typename Key>
        ReturnValue<Key> operator[](Key i);

        const Number version();

        void pop(int n = 1);

        //! stack size, or top index (because lua table is 1-based)
        int top();

        int load(Reader reader,
                const std::string source, const char *mode = nullptr);

        int loadstring(const std::string& str);
        int loadfile(const std::string& file);
    private:
        State(const State&);
        State& operator=(const State&);
    };

    class NewState : public State {
    public:
        NewState();
        ~NewState();
    };

    /******************************************
     *  IMPLEMENTATION
    ******************************************/
    inline RuntimeError::RuntimeError(const std::string& msg)
        : std::runtime_error(msg) {}

    inline CClosure::CClosure(lua_CFunction f, int uv)
            : func(f), upvalues(uv) {}

    inline std::ostream& operator<<(std::ostream out, CClosure closure) {
        return out << "luamm::closure(" << closure.func << "("
                    << closure.upvalues << ")";
    }

    inline Index::Index(int i) : index(i) {}
    inline int Index::get() const {
        if (index == 0) {
            throw std::out_of_range("access to index 0 is not permitted");
        }
        return index;
    }

    inline Index::operator int() const { return index; }
    inline Index Index::bottom() { return Index(1); }
    inline Index Index::top() { return Index(-1); }
    inline Index Index::upvalue(int i) { return Index(lua_upvalueindex(i)); }
    inline Index Index::registry() { return Index(LUA_REGISTRYINDEX); }
    inline Index Index::mainThread() { return Index(LUA_RIDX_MAINTHREAD); }
    inline Index Index::globals() { return Index(LUA_RIDX_GLOBALS); }

    inline std::ostream& operator<<(std::ostream& out, Nil nil) {
        return out << "luamm::Nil";
    };

    template<typename Key>
    State::ReturnValue<Key>::TypeError::TypeError(
                ReturnValue *p, const std::string& msg)
           : RuntimeError(std::string("type mismatch, expect") + msg) {}

    template<typename Key>
    State::ReturnValue<Key>::ReturnValue(State *s, Key k)
        : state(s), key(k) {}


    template<typename Key>
    template<typename T>
    State::ReturnValue<Key>::operator T&&() {
        static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
        T out;
        if (! Accessor::get(state->ptr, out, key)) {
            throw RuntimeError("get error");
        }
        return std::move(out);
    }

    template<typename Key>
    template<typename T>
    State::ReturnValue<Key>& State::ReturnValue<Key>::operator=(const T& value) {
        static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
        Accessor::set(state->ptr, value, key);
        return *this;
    }


    template<typename Key>
    inline int State::ReturnValue<Key>::type() {
        return lua_type(state->ptr, key);
    }

    inline State::State(lua_State *ref) : ptr(ref) {}

    inline int State::pcall(int nargs, int nresults, int msgh) {
        return lua_pcall(ptr, nargs, nresults, msgh);
    }

    inline void State::copy(Index from, Index to) {
        lua_copy(ptr, from, to);
    };

    inline void State::openlibs() { luaL_openlibs(ptr); }

    template<typename T>
    void State::push(const T& v) {
        static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
        VarTypeTrait<T>::push(ptr, v);
    }

    inline Table State::pushTable(int narray, int nother) {
        lua_createtable(ptr, narray, nother);
        return Table(this->ptr, top());
    }

    inline void State::remove(Index i) { lua_remove(ptr, i.get()); }
    inline void State::replace(Index i) { lua_replace(ptr, i.get()); }

    template<typename Key>
    State::ReturnValue<Key> State::operator[](Key i) {
        return ReturnValue<Key>(this, i);
    }

    inline const Number State::version() {
        return *lua_version(ptr);
    }

    inline void State::pop(int n) { lua_pop(ptr, n); }
    inline int State::top() { return lua_gettop(ptr); }


    inline int State::load(Reader reader,
            const std::string source, const char *mode) {
        return lua_load(ptr, luamm_reader, &reader, source.c_str(), mode);
    }

    inline NewState::NewState() : State(luaL_newstate()) {
        if (!ptr) { throw RuntimeError("allocate new lua state"); }
    }

    inline NewState::~NewState() { lua_close(ptr); }

    template<typename K>
    template<typename V>
    Table::ReturnValue<K> Table::ReturnValue<K>::operator=(const V& v) {
        static_assert(VarTypeTrait<V>::isvar, "key is not a var type");
        State st(table->state);
        // push key
        st.push(key);
        st.push(v);
        lua_settable(table->state, table->index);
        return *this;
    }

    template<typename K>
    template<typename V>
    Table::ReturnValue<K>::operator V&&() {
        static_assert(VarTypeTrait<V>::isvar, "key is not a var type");
        State st(table->state);
        st.push(key);
        lua_gettable(table->state, table->index);
        V v = st[Index::top()];
        st.pop();
        return std::move(v);
    }

    inline bool VarTypeTrait<Table>::get(lua_State* st, vartype&  out, int index) {
        out = Table(st, index);
        return true;
    }

} // end namespace luamm

#endif
