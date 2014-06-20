#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <utility>
#include <iostream>
#include <type_traits>
#include <cassert>
#include <boost/type_traits.hpp>

extern "C" {
    const char *luamm_reader(lua_State *L, void *data, size_t *size);
    int luamm_function_cleaner(lua_State* L);
    int luamm_closure(lua_State *L);
}

namespace luamm {

    struct RuntimeError : std::runtime_error {
        RuntimeError(const std::string& msg);
    };

    struct SyntaxError : RuntimeError {
        SyntaxError(const std::string& msg) : RuntimeError(msg) {}
    };

    struct MemoryError : RuntimeError {
        MemoryError(const std::string& msg) : RuntimeError(msg) {}
    };

    struct GCError : RuntimeError {
        GCError(const std::string& msg) : RuntimeError(msg) {}
    };

    typedef lua_Number Number;

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

    class State;
    struct CClosure {
        lua_CFunction func;
        int upvalues;
        lua_State *state;
        Index index;
        CClosure(lua_CFunction f, int uv = 0, Index index = 0);

        class ReturnValue {
            CClosure *cl;
            int n;
        public:
            ReturnValue(CClosure *closure, int n) : cl(closure), n(n) {}
            template<typename T>
            operator T();

            template<typename T>
            ReturnValue& operator=(const T& t);
        };

        ReturnValue operator[](int n) {
            return ReturnValue(this, n);
        }
    };

    typedef std::function<int(State&)> Function;

    class Nil {};

    template<typename T> struct VarTypeTrait;

    struct ValidVarType {
        static const bool isvar = true;
    };

    class UserBlock {};
    class UserData {
        friend VarTypeTrait<UserData>;
        friend VarTypeTrait<Function>;
        UserBlock* ptr;
        Index index;
    public:
        UserData(UserBlock *p, Index index) : ptr(p), index(index) {}
        UserData() : ptr(nullptr), index(0) {}
        template<typename T>
        T* get() { return reinterpret_cast<T*>(ptr); }
    };

    template<>
    struct VarTypeTrait<UserData> : public ValidVarType {
        typedef UserData vartype;
        enum { tid = LUA_TUSERDATA };
        static void push(lua_State* st, vartype ud) {
            lua_pushnil(st);
            lua_copy(st, ud.index, -1);
        }

        static bool get(lua_State* st, vartype&  out, int index) {
            void *p = lua_touserdata(st, index);
            if (!p) {
                return false;
            } else {
                out.index = index;
                out.ptr = static_cast<UserBlock*>(p);
                return true;
            }
        }
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

#ifndef _MSC_VER
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
#endif

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

        static void push(lua_State* st, const vartype& c) {
            lua_settop(st, lua_gettop(st) + c.upvalues);
            lua_pushcclosure(st, c.func, c.upvalues);
            vartype& closure = const_cast<vartype&>(c);
            // FIXME, validate access eontrol
            closure.index = lua_gettop(st);
            closure.state = st;
        }

        static bool get(lua_State* st, vartype& out, int index) {
            auto f = lua_tocfunction(st, index);
            if (f) {
                out.func = f;
                out.index = index;
                out.state = st;
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


    struct DummyVarTypeTrait {
        static const bool isvar = false;
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

        static Index abs(lua_State* st, Index key) {
            return lua_absindex(st, key);
        }
    };

    template<>
    struct VarSetterGetter<const std::string&> {
        typedef const std::string& Key;
        static const bool iskey = true;
        template<typename T>
        static bool get(lua_State* st, T& out, const Key& key) {
            lua_getglobal(st, key.c_str());
            auto r = VarTypeTrait<T>::get(st, out, Index::top());
            if (r) lua_pop(st, 1);
            return r;
        }

        template<typename T>
        static void set(lua_State* st, const T& _in, const Key& key) {
            VarTypeTrait<T>::push(st, _in);
            lua_setglobal(st, key.c_str());
        }

        static const char *abs(lua_State* st, const char* key) {
            return key;
        }

        static const std::string& abs(lua_State* st, const std::string& key) {
            return key;
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

    class Table {
        friend VarTypeTrait<Table>;
        friend VarTypeTrait<Function>;
        lua_State  *state;
        Index  index;
    public:
        Table(lua_State *state, Index index) : state(state), index(index) {}
        Table() : state(nullptr), index(0) {}

        template<typename K>
        class ReturnValue {
            Table *table;
            K key;
        public:
            ReturnValue(Table* table, decltype(key) key) : table(table), key(key) {}
            template<typename V>
            operator V();

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
            Key key;
        public:
            class TypeError : public RuntimeError {
            public:
                TypeError(ReturnValue *p, const std::string& msg);
            };
            ReturnValue(State *s, Key k);

            template<typename T>
            operator T();

            template<typename T>
            ReturnValue& operator=(const T& value);

            int type();

            const char *typeName() {
                return lua_typename(state->ptr, type());
            }
        };

        State(lua_State *ref);
        ~State() {}

        int pcall(int nargs, int nresults, int msgh = 0);

        void copy(Index from, Index to);

        void openlibs();

        template<typename T>
        void push(const T& v);

        Table pushTable(int narray = 0, int nother = 0);

        template<typename T>
        UserData pushUserData() {
            static_assert(boost::has_trivial_copy<T>::value, "T must be trivial");
            auto p = lua_newuserdata(ptr, sizeof(T));
            return UserData(static_cast<UserBlock*>(p), top());
        }

        template<typename T>
        UserData pushUserData(const T& t) {
            UserData p = pushUserData<T>();
            *(p.get<T>()) = t;
            return p;
        }


        void remove(Index i);

        void replace(Index i);

        template<typename Key>
        ReturnValue<Key> operator[](Key i);

        const Number version();

        void pop(int n = 1);

        //! stack size, or top index (because lua table is 1-based)
        int top();

        void load(Reader reader,
                const std::string source, const char *mode = nullptr);

        void loadstring(const std::string& str);
        void loadfile(const std::string& file);
    private:
        void load_error(int code);
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

    inline CClosure::CClosure(lua_CFunction f, int uv, Index index)
            : func(f), index(index) {
        upvalues = uv;
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
        : state(s), key(k) {
    }


    template<typename Key>
    template<typename T>
    State::ReturnValue<Key>::operator T() {
        static_assert(VarTypeTrait<T>::isvar, "T is not a valid variable type");
        T out;
        if (! Accessor::get(state->ptr, out, key)) {
            throw RuntimeError("get error");
        }
        return out;
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
        return ReturnValue<Key>(this, VarKeyMatcher<Key>::type::abs(ptr, i));
    }

    inline const Number State::version() {
        return *lua_version(ptr);
    }

    inline void State::pop(int n) { lua_pop(ptr, n); }
    inline int State::top() { return lua_gettop(ptr); }

    inline void State::load_error(int code) {
        switch (code) {
            case LUA_OK:
                return;
            case LUA_ERRSYNTAX:
                {
                const char* msg = this->operator[](1);
                pop();
                throw SyntaxError(msg);
                }
            case LUA_ERRMEM:
                throw MemoryError("");
            case LUA_ERRGCMM:
                throw GCError("");
            default:
                assert(false);
        }
    }

    inline void State::load(Reader reader,
            const std::string source, const char *mode) {
        load_error(lua_load(ptr, luamm_reader, &reader, source.c_str(), mode));
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
    Table::ReturnValue<K>::operator V() {
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

    template<typename T>
    CClosure::ReturnValue::operator T() {
        static_assert(VarTypeTrait<T>::isvar, "T is not a var type");
        if (lua_getupvalue(cl->state, cl->index, n) == nullptr) {
            throw std::out_of_range("not a valid upvalue index");
        }
        T tmp;
        if (!VarTypeTrait<T>::get(cl->state, tmp, Index::top())) {
            throw RuntimeError("get error");
        }
        return std::move(tmp);
    }

    template<typename T>
    CClosure::ReturnValue& CClosure::ReturnValue::operator=(const T& t) {
        static_assert(VarTypeTrait<T>::isvar, "T is not a var type");
        VarTypeTrait<T>::push(cl->state, t);
        if (!lua_setupvalue(cl->state, cl->index, n)) {
            lua_pop(cl->state, 1);
            throw RuntimeError("cannot assign to noexist upvalue");
        }
        return *this;
    }

    template<>
    struct VarTypeTrait<Function> : public ValidVarType {
        enum { tid = LUA_TFUNCTION };
        static void push(lua_State* state, Function func) {
            // build delegated function
            State st(state);
            CClosure cl(luamm_closure, 1);
            VarTypeTrait<CClosure>::push(state, cl); // +1

            // copy std::function to fp
            Function *fp = new Function(func);

            // build userdata from fp
            auto ud = st.pushUserData(fp); // +1

            // set upvalue for closure
            cl[1] = ud;

            // build cleaner
            CClosure clcl(&luamm_function_cleaner);
            VarTypeTrait<CClosure>::push(state, clcl); // +1

            // set gc method for closure
            Table meta = st.pushTable(); // +1
            meta["__gc"] = clcl;
            lua_setmetatable(state, cl.index); // -1

            st.pop(2);
        }
    };

    template<typename T>
    struct VarTypeTrait {
        // fallback treatment, consider Number or Function(lambdas)
        typedef typename std::conditional<std::is_convertible<T, Number>::value,
                    VarTypeTrait<Number>,
                    typename std::conditional<std::is_convertible<T, Function>::value,
                        VarTypeTrait<Function>,
                        DummyVarTypeTrait
                    >::type
                >::type type;

        static const bool isvar = type::isvar;

        static void push(lua_State* st, const T& u) {
            type::push(st, u);
        }

        static bool get(lua_State* st, T& out, int index) {
            T tmp;
            if (VarTypeTrait<type>::get(st, tmp, index)) {
                out = tmp;
                return true;
            } else {
                return false;
            }
        }
    };

} // end namespace luamm

#endif
