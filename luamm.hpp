#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <assert.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/result_type.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/preprocessor.hpp>
#include <iostream>

namespace luamm {

    // utility classes and functions
    namespace {
        class AutoPopper {
            lua_State* state;
            int n;
        public:
            AutoPopper(lua_State* st, int n = 1) : state(st), n(n) {}
            ~AutoPopper() { if (n > 0) lua_pop(state, n);  }
        };

        inline void cleanup(lua_State* state, int index) {
            if (state && index == lua_gettop(state)) {
                lua_pop(state, 1);
            }
        }
    }


/* make aliases for lua types */
typedef lua_Number Number;
typedef lua_CFunction CFunction;

/* primitive lua typs */
class Nil {};

/* type trait distinguishes value(primitive) and non-value lua types */
// value == 0, iff non-value type, Closure, Table etc
// value == 1, iff value type Number, string, boolean, Nil etc
template<typename T>
struct StackVariable {
    enum { value = 0 };
};

/* Base class for errors */
struct RuntimeError : std::runtime_error {
    RuntimeError(const std::string& s) : std::runtime_error(s) {}
    RuntimeError() : std::runtime_error("") {}
};

/* defines a static member funcion push() for each type of variable
 * that permitted to be passed into lua runtime environmrnt */
template<typename T>
struct VarPusher;

/* defines a static member function get() for read a lua variable
 * uniquely identified by key of type Key into elem of type Elem
 */
template<typename Container, typename Key, typename Elem>
struct KeyGetter;

/* similar to KeyGetter, but set elem instead of getting */
template<typename Container, typename Key, typename Elem>
struct KeySetter;

/* return the lua type of variable indexed by key */
template<typename Container, typename Key>
struct KeyTyper;

template<typename Container, typename KeyStore>
struct AutoCleaner {
    template<typename T>
    static void clean(const Container& _, const KeyStore& k) {}
};

template<>
struct AutoCleaner<lua_State*,int> {
    template<typename T>
    static void clean(lua_State* state, int index) {
        if (lua_gettop(state) == index &&
                !StackVariable<T>::value) {
            lua_pop(state, 1);
        }
    }
};

/* router class for read/write/update a lua variable */
template<typename Container, typename Key, typename KeyStore = Key>
class Variant  {
private:
    KeyStore index;
    Container state;
    bool autoclean;
public:
    Variant(Container st, const Key& i, bool autoclean = false)
        : index(i), state(st), autoclean(autoclean)  {}

    template<typename T>
    T to() const {
        return KeyGetter<Container, KeyStore, T>::get(state, index);
    }

    template<typename T>
    operator T() const  {
        T o = to<T>();
        if (autoclean) {
            AutoCleaner<Container, KeyStore>::template clean<T>(state, index);
        }
        return o;
    }

    template<typename T>
    Variant& operator=(const T& var) {
        KeySetter<Container, KeyStore, T>::set(state, index, var);
        return *this;
    }

    template<typename T>
    T convert(T* ) {
        return this->operator T();
    }

    int type() {
        return KeyTyper<Container, KeyStore>::type(state, index);
    }

    bool isnum() { return type() == LUA_TNUMBER; }
    bool istab() { return type() == LUA_TTABLE; }
    bool isnil() { return type() == LUA_TNIL; }
    bool isbool() { return type() == LUA_TBOOLEAN; }
    bool isstr() { return type() == LUA_TSTRING; }
    bool isfun() { return type() == LUA_TFUNCTION; }
    bool isuserdata() { return type() == LUA_TUSERDATA; }
    bool isthread() { return type() == LUA_TTHREAD; }
    bool islight() { return type() == LUA_TLIGHTUSERDATA; }

    bool iscfun() {
        try {
            KeyGetter<Container, KeyStore, CFunction>::get(state, index);
        } catch (RuntimeError e) {
            return false;
        }
        return true;
    }
};

/* rip out the corresponding value within lua stack after a reading
 * iff it is a value-type variable
 */
class AutoVariant : public Variant<lua_State*, int> {
public:
    AutoVariant(lua_State* st, int i)
        : Variant<lua_State*,int>(st, i, true) {}
};


struct Table {
    lua_State* state;
    int index;
    Table(lua_State* st, int i);
    ~Table();

    template<typename T>
    Variant<Table*, T, typename VarPusher<T>::type> operator[](const T& k);

    void set(const Table& metatab);
    Table get();

    bool operator==(const Table& o) const {
        return o.state == state &&
                lua_compare(state, index, o.index, LUA_OPEQ);
    }

    bool operator!=(const Table& o) const {
        return !this->operator==(o);
    }

    Table(Table&& o) : state(o.state), index(o.index) {
        o.state = nullptr;
        o.index = 0;
    }
private:
    Table(const Table&);
};

/* interface that lua variable types which can have corresponding metatable
 */
template<typename Sub>
struct HasMetaTable {
    void set(const Table& metatab);
    void set(const std::string& regkey);
    void check(const std::string& regkey);
    Table get();
};

/* a closure in lua is a tuple of
 * #1, a function pointer who follows the lua cfunction signature
 * #2, the number of upvalues
 */
struct CClosure {
    int index;
    CFunction func;
public:
    CClosure(CFunction func, int index = 0)
        : index(index), func(func) {}
    operator CFunction() {
        return func;
    }
};


struct UserData : HasMetaTable<UserData> {
    lua_State* state;
    int index;
    UserData(lua_State* st, int index)
        : state(st), index(lua_absindex(st, index)) {}
    UserData(UserData&& o) : state(o.state), index(o.index) {
        o.state = nullptr;
        o.index = 0;
    }

    template<typename T>
    T& to() {
        void *p = lua_touserdata(state, index);
        return *static_cast<T*>(p);
    }
    ~UserData();
private:
    UserData(const UserData&);
};

template<>
struct StackVariable<UserData> {
    enum { value = 1 };
};

template<typename LuaValue>
struct VarProxy;


struct VarBase {
    lua_State *state;

    VarBase(lua_State *st) : state(st) {}

    template<typename T>
    VarProxy<T>& to() { return static_cast<VarProxy<T>&>(*this); }
};


template<>
struct VarProxy<CClosure> : VarBase {
    bool push(CClosure c) {
        for (auto i = 0; i < c.index; i++)
            lua_pushnil(state);
        lua_pushcclosure(state, c.func, c.index);
        return true;
    }
};

// light userdata is just a void* pointer
template<>
struct VarProxy<void*> : VarBase {
    bool push(void* l) {
        lua_pushlightuserdata(state, l);
        return true;
    }

    void *get(int index, bool& success) {
        auto p = lua_touserdata(state, index);
        if (p) success = true;
        return p;
    }
    enum { tid = LUA_TLIGHTUSERDATA };
};

template<>
struct VarProxy<UserData> : VarBase {
    bool push(const UserData& l) {
        lua_pushnil(state);
        lua_copy(state, l.index, -1);
        return true;
    }

    UserData get(int index, bool& success) {
        success = true;
        return UserData(state, index);
    }
    enum { tid = LUA_TUSERDATA };
};

template<>
struct VarProxy<CFunction> : VarBase {
    CFunction get(int index, bool& success) {
        auto p = lua_tocfunction(state, index);
        if (p) success = true;
        return p;
    }
    enum { tid = LUA_TFUNCTION };
};


template<>
struct VarProxy<std::string> : VarBase {
    bool push(const std::string& v) {
        return lua_pushstring(state, v.c_str()) ? true : false;
    }
};

template<>
struct VarProxy<const char*> : VarBase {
    const char *get(int index, bool& success) {
        const char * r = lua_tostring(state, index);
        success = r ? true : false;
        return r;
    }
    enum { tid = LUA_TSTRING};
};

template<>
struct VarProxy<Number> : VarBase  {
    bool push(Number num) {
        lua_pushnumber(state, num);
        return true;
    }

    Number get(int index, bool& success) {
        int isnum;
        Number r = lua_tonumberx(state, index, &isnum);
        success = isnum ? true : false;
        return r;
    }
    enum { tid = LUA_TNUMBER };
};


/* all in/out lua variable types that expected to be converted to/from
 * automatically (as a function arguments or return value)
 * should be placed in this mpl vector.
 */
typedef boost::mpl::vector<
            std::string,
            const char*,
            Number,
            CClosure,
            Table,
            UserData,
            Nil
        > varproxies;
struct PlaceHolder {};

/* decide which type in varproxies provide the matching proxy for typename
 * Var
 */
template<typename Proxy, typename Var>
struct PredPush {
    typedef struct { char _[2]; } two;
    typedef char one;

    template<typename P, typename V>
    static one test(
            decltype(
                std::declval<VarProxy<P>>().push(std::declval<V>())));

    template<typename P, typename V>
    static two test(...);

    typedef boost::mpl::bool_<sizeof(test<Proxy, Var>(true)) == 1> type;
};


template<typename Proxy, typename Var>
struct PredGet {
    typedef struct { char _[2]; } two;
    typedef char one;

    template<typename P, typename V>
    static one test(
        typename std::conditional<
            std::is_convertible<
                decltype(
                    VarBase(static_cast<lua_State*>(nullptr))
                    .to<P>().get(
                        static_cast<int>(2),
                        std::declval<bool&>()
                    )
                ),
                V
            >::value,
            void *,
            double
        >::type
    );

    template<typename P, typename V>
    static two test(...);

    typedef boost::mpl::bool_<sizeof(test<Proxy, Var>(nullptr)) == 1> type;
};

template<template<class, class> class Pred, typename T>
struct SelectImpl
{
    typedef typename boost::mpl::fold<
        varproxies,
        PlaceHolder,
        // _1 is state, _2 is current item
        boost::mpl::if_<Pred<boost::mpl::_2, T>,
                        boost::mpl::_2, // select current item
                        boost::mpl::_1> // keep last item
    >::type type;
};

template<>
struct VarProxy<Nil> : VarBase {
    bool push(Nil _) {
        lua_pushnil(state);
        return true;
    }
    enum { tid = LUA_TNIL};
};

template<>
struct VarProxy<bool> : VarBase {
    bool push(bool b) {
        lua_pushboolean(state, b);
        return true;
    }

    bool get(int index, bool& success) {
        success = true;
        return lua_toboolean(state, index) ? true : false;
    }
    enum { tid = LUA_TBOOLEAN};
};

template<typename Exception>
struct Guard {
    bool status;
    Guard() : status(false) {}
    ~Guard() { if (!status) throw Exception(); }
};

struct VarPushError : RuntimeError { VarPushError() : RuntimeError("") {} };

/* generate a tuple to simulate multiple return value in c++*/
template<int I, typename Arg,  typename... Args>
struct GenTuple  {
    typedef typename GenTuple<I-1, Arg, Args..., Arg>::type type;
};

template<typename Arg,  typename... Args>
struct GenTuple<0, Arg, Args...> {
    typedef std::tuple<Args...> type;
};

/* a callable representing a lua variable exitsing in the stack,
 * either a c function or a lua function  */
struct Closure : public HasMetaTable<Closure> {
    lua_State* state;
    int index;
    Closure(lua_State* st, int index) : state(st), index(lua_absindex(st, index)) {}
    Variant<Closure*, int>  operator[](int n) {
        return Variant<Closure*, int>(this, n);
    }
    ~Closure();
    Closure(Closure&& o) : state(o.state), index(o.index) {
        o.state = nullptr;
        o.index = 0;
    }

    template<int rvals>
    struct Rvals {
        typedef typename std::conditional<rvals == 0,
                void,
                typename std::conditional<rvals == 1,
                    // true => auto cleanable variant
                    AutoVariant,
                    typename GenTuple<rvals, Variant<lua_State*,int>>::type
                >::type
            >::type type;
    };

    template<int rvals>
    typename Rvals<rvals>::type __return__();

    template <typename... Args>
    Rvals<1>::type operator()(Args&& ... args) {
        return call(std::forward<Args>(args)...);
    }

    template<int rvals = 1, typename... Args>
    typename Rvals<rvals>::type call(Args&&... args) {
        // push the function to be called
        lua_pushnil(state);
        lua_copy(state, index, -1);
        // passing arguments
        return __call__<rvals, 0, Args...>(std::forward<Args>(args)...);
    }

    template<int rvals, int count, typename T, typename... Args>
    typename Rvals<rvals>::type __call__(T&& a, Args&&... args) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<T>::push(state, std::forward<T>(a));
        }
        return this->__call__<rvals, count+1, Args...>(std::forward<Args>(args)...);
    }

    template<int rvals, int count>
    typename Rvals<rvals>::type __call__() {
        auto i = lua_pcall(state, count, rvals, 0);
        if (i != LUA_OK) {
            if (lua_type(state, -1) == LUA_TSTRING) {
                throw RuntimeError(lua_tostring(state, -1));
            }
        }
        return __return__<rvals>();
    }
private:
    Closure(const Closure&);
};

template<>
inline void Closure::__return__<0>() {}

template<>
inline typename Closure::Rvals<1>::type Closure::__return__<1>() {
    // auto cleanable variant
    return Closure::Rvals<1>::type(state, lua_gettop(state));
}

/* the maximum return values you can retrieved after calling Closure */
#ifndef LUAMM_MAX_RETVALUES
#define LUAMM_MAX_RETVALUES 15
#endif

#define LUAMM_X(n) Variant<lua_State*, int>(state, -n)BOOST_PP_COMMA_IF(BOOST_PP_SUB(n,1))
#define LUAMM_Y(a, b, c) LUAMM_X(BOOST_PP_SUB(c, b))
#define LUAMM_ARGS(n) BOOST_PP_REPEAT(n, LUAMM_Y, n)
#define LUAMM_TMPL(_a, n, _b) template<>\
    inline typename Closure::Rvals<n>::type Closure::__return__<n>() {\
        return std::make_tuple(LUAMM_ARGS(n)); \
    }
BOOST_PP_REPEAT_FROM_TO(2, LUAMM_MAX_RETVALUES, LUAMM_TMPL,)
#undef LUAMM_X
#undef LUAMM_Y
#undef LUAMM_ARGS
#undef LUAMM_TMPL

template<>
struct StackVariable<Closure> {
    enum { value = 1 };
};

template<>
struct VarProxy<Closure> : VarBase {
    bool push(const Closure& c) {
        lua_pushnil(state);
        lua_copy(state, c.index, -1);
        return true;
    }

    Closure get(int index, bool& success) {
        if (lua_isfunction(state, index)) {
            success = true;
            return Closure(state, index);
        } else {
            return Closure(nullptr, 0);
        }
    }
    enum { tid = LUA_TFUNCTION };
};

template<>
struct StackVariable<Table> {
    enum { value = 1 };
};

struct KeyGetError : RuntimeError { KeyGetError() : RuntimeError("") {} };
struct KeyPutError : RuntimeError { KeyPutError() : RuntimeError("") {} };
struct VarGetError : RuntimeError { VarGetError() : RuntimeError("") {} };


template<>
struct VarProxy<Table> : VarBase {
    bool push(const Table& tb) {
        lua_pushnil(state);
        lua_copy(state, tb.index, -1);
        return true;
    }

    Table get(int index, bool& success) {
        if (!lua_istable(state, index)) {
            throw RuntimeError("is not a table");
        }
        success = true;
        return Table(state, index);
    }
    enum { tid = LUA_TTABLE };
};

namespace {
    template<typename T>
    struct VarDispatcher {

       template<typename C>
       static char testget( decltype(&VarProxy<C>::get) );

       template<typename C>
       static double testget( ... );

       template<typename C>
       static char testpush( decltype(&VarProxy<C>::push) );

       template<typename C>
       static double testpush( ... );

       enum { use_indirect_get = sizeof(testget<T>(nullptr)) != 1 };
       enum { use_indirect_push = sizeof(testpush<T>(nullptr)) != 1 };
    };
}

template<typename T>
struct VarPusher {
    typedef typename std::conditional<VarDispatcher<T>::use_indirect_push,
                 typename SelectImpl<PredPush, T>::type,
                 T
            >::type type;
    static bool push(lua_State* st, const T& v) {
        static_assert( ! std::is_same<PlaceHolder, type>::value,
                "no push() implmentation can be selected for T" );
        return VarBase(st).to<type>().push(v);
    }
};

template<typename T>
struct VarGetter {
    typedef typename std::conditional<VarDispatcher<T>::use_indirect_get,
                 typename SelectImpl<PredGet, T>::type,
                 T
            >::type type;
    static T get(lua_State* st, int index, bool& success) {
        static_assert( ! std::is_same<PlaceHolder, type>::value,
                "no get() implmentation can be selected for T" );
        return VarBase(st).to<type>().get(index, success);
    }
};


template<typename Var>
struct KeyGetter<Closure*, int, Var> {
    static Var get(Closure* cl, int key) {
        auto p = lua_getupvalue(cl->state, cl->index, key);
        if (!p) { throw VarGetError(); }
        Guard<VarGetError> gd;
        AutoPopper ap(cl->state, 1 - StackVariable<Var>::value);
        return VarGetter<Var>::get(cl->state, -1, gd.status);
    }
};

template<>
struct KeyTyper<Closure*, int> {
    static int type(Closure* cl, int key) {
        auto p = lua_getupvalue(cl->state, cl->index, key);
        if (!p) { throw VarGetError(); }
        AutoPopper ap(cl->state);
        return lua_type(cl->state, -1);
    }
};

template<typename Var>
struct KeySetter<Closure*, int, Var> {
    static void set(Closure* cl, int key, const Var& nv) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Var>::push(cl->state, nv);
        }
        if (!lua_setupvalue(cl->state, cl->index, key)) {
            lua_pop(cl->state, 1);
            throw RuntimeError("cannot set upvalue");
        }
    }

};

// stack + position
template<typename Var>
struct KeyGetter<lua_State*, int, Var> {
    static Var get(lua_State* container, int key) {
        Guard<VarGetError> gd;
        return VarGetter<Var>::get(container, key, gd.status);
    }
};

template<typename Var>
struct KeySetter<lua_State*, int, Var> {
    static void set(lua_State* container, int key, const Var& nv) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Var>::push(container, nv);
        }
        AutoPopper ap(container);
        lua_copy(container, -1, key);
    }
};

template<>
struct KeyTyper<lua_State*, int> {
    static int type(lua_State* st, int i) {
        return lua_type(st, i);
    }
};

// auto clean variant if it is not a stack variable(which handle life
// crycle by itself, this process is conservative:
// that if not on top, do noting

// global variable key
template<typename Var>
struct KeyGetter<lua_State*, std::string, Var> {
    static Var get(lua_State* container, const std::string& key) {
        lua_getglobal(container, key.c_str());
        Guard<VarGetError> gd;
        AutoPopper ap(container, 1 - StackVariable<Var>::value);
        return VarGetter<Var>::get(container, -1, gd.status);
    }
};

template<>
struct KeyTyper<lua_State*, std::string> {
    static int type(lua_State* st, const std::string& k) {
        lua_getglobal(st, k.c_str());
        AutoPopper ap(st);
        return lua_type(st, -1);
    }
};

template<typename Var>
struct KeySetter<lua_State*, std::string, Var> {
    static void set(lua_State* container, const std::string& key, const Var& nv) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Var>::push(container, nv);
        }
        lua_setglobal(container, key.c_str());
    }
};


template<typename Key>
struct KeyTyper<Table*, Key> {
    static int type(Table *t, const Key& k) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Key>::push(t->state, k);
        }
        lua_gettable(t->state, t->index);
        AutoPopper ap(t->state);
        return lua_type(t->state, -1);
    }
};

template<typename Key, typename Var>
struct KeyGetter<Table*, Key, Var> {
    static Var get(Table* container, const Key& key) {
        {
            // push key
            Guard<VarPushError> gd;
            gd.status = VarPusher<Key>::push(container->state, key);
        }

        // access table
        lua_gettable(container->state, container->index);

        // return reteieved value
        Guard<VarGetError> gd;
        AutoPopper ap(container->state, 1 - StackVariable<Var>::value);
        return VarGetter<Var>::get(container->state, -1, gd.status);
    }
};

template<typename Key, typename Var>
struct KeySetter<Table*, Key, Var> {
    static void set(Table *container, const Key& key, const Var& nv) {
        {
            // push key
            Guard<VarPushError> gd;
            gd.status = VarPusher<Key>::push(container->state, key);
        }
        {
            // push key
            Guard<VarPushError> gd;
            gd.status = VarPusher<Var>::push(container->state, nv);
        }
        // access table
        lua_settable(container->state, container->index);
    }
};

class State;
template<typename Class>
class Class_ {
    friend State;
    Class_(const std::string& name, State& state);
    std::string name;
    State& state;
    Table mod;
public:
    template<typename T>
    typename std::enable_if<
        std::is_member_function_pointer<T*>::value, Class_<Class>&>::type
    def(const std::string& method, T* method_pointer);


    template<typename T>
    typename std::enable_if<
        !std::is_member_function_pointer<T*>::value, Class_<Class>&>::type
    def(const std::string& method, T callable);

    Class_(Class_&& o)
        : name(std::move(o.name)), state(o.state), mod(std::move(o.mod)) {}
    operator Table() && { return std::move(mod); }
};



/* wrap an existing lua_State */
class State {
protected:
    lua_State *ptr_;
public:

    class Scope {
        State *state;
        int origtop;
    public:
        Scope(State* st) : state(st), origtop(st->top()) {}
        ~Scope() { state->settop(origtop); }
    };

    Scope newScope() {
        return Scope(this);
    }

    State(lua_State *p);
    lua_State* ptr();

    void pop(int n=1) {
        lua_pop(ptr(), n);
    }

    template<typename T>
    void push(const T& value) {
        VarPusher<T>::push(ptr(), value);
    }

    Table newTable(int narray = 0, int nother = 0) {
        lua_createtable(ptr(), 0, 0);
        return Table(ptr(), top());
    }

    template<typename T, typename... Args>
    UserData newUserData(Args&& ... args) {
        void * buf = lua_newuserdata(ptr(), sizeof(T));
        new (buf) T(std::forward<Args>(args)...);
        return UserData(ptr(), -1);
    }

    Variant<lua_State*, int> operator[](int pos) {
        return Variant<lua_State*, int>(ptr(), pos);
    }

    Variant<lua_State*, std::string> operator[](const std::string& key) {
        return Variant<lua_State*, std::string>(ptr(), key);
    }

    int top() {
        return lua_gettop(ptr());
    }

    void openlibs() {
        luaL_openlibs(ptr());
    }

    template<typename T>
    void error(const T& t) {
        push(t);
        lua_error(ptr());
    }

    Table registry() {
        return this->operator[](LUA_REGISTRYINDEX);
    }

    Closure newFunc(const std::string& str) {
        auto code =  luaL_loadstring(ptr(), str.c_str());
        if (code != LUA_OK) {
            std::string msg = this->operator[](-1);
            pop();
            throw RuntimeError(msg);
        } else {
            return Closure(ptr(), -1);
        }
    }

    Closure newFile(const std::string& filename) {
        auto code = luaL_loadfile(ptr(), filename.c_str());
        if (code != LUA_OK) {
            std::string msg = this->operator[](-1);
            pop();
            throw RuntimeError(msg);
        } else {
            return Closure(ptr(), -1);
        }
    }

    State& operator=(State&& o) {
       ptr_ = o.ptr_;
       o.ptr_ = nullptr;
       return *this;
    }

    template<typename F>
    Closure newCallable(F func, int extra_upvalues = 0);

    State(const State& o);

    int upvalue(int i) {
        return lua_upvalueindex(i);
    }

    const char *typerepr(int tid) {
        return lua_typename(ptr(), tid);
    }

    void allocate(int i) {
        lua_pushnil(ptr());
        lua_insert(ptr(), i);
    }

    void settop(int i) {
        lua_settop(ptr(), i);
    }

    bool onstack(int index) {
        return index > LUAI_FIRSTPSEUDOIDX;
    }

    template<typename Class>
    Class_<Class> class_(const std::string& name) {
        return Class_<Class>(name, *this);
    }
};

template<typename MemberFuncPtr, typename ThisType, typename... Args>
struct MemberFunctionWrapper {
    MemberFuncPtr p;
    MemberFunctionWrapper(MemberFuncPtr p) : p(p) {}

    boost::function_types::result_type<MemberFuncPtr>
    operator()(State& st, UserData&& self, Args... args) {
        auto& hidden_this = self.to<ThisType>();
        return (hidden_this.*p)(std::forward<Args>(args)...);
    }
};

template<typename MemberFuncPtr, typename ThisType,
         int size, typename PARALIST, typename... Args>
struct MemberFunctionTransformer {
    typedef typename MemberFunctionTransformer<
        MemberFuncPtr, ThisType, size-1,
        typename boost::mpl::pop_front<PARALIST>::type,
        typename boost::mpl::at_c<PARALIST, 0>::type, Args...>::type type;
};

template<typename MemberFuncPtr, typename ThisType,
         typename PARALIST, typename... Args>
struct MemberFunctionTransformer<MemberFuncPtr, ThisType, 0, PARALIST, Args...> {
    typedef MemberFunctionWrapper<MemberFuncPtr, ThisType, Args...> type;
};

template<typename Class>
template<typename T>
typename std::enable_if<
    std::is_member_function_pointer<T*>::value, Class_<Class>&>::type
Class_<Class>::def(const std::string& method, T* method_ptr)
{
    typedef typename boost::function_types::parameter_types<T*>::type
        full_para_t;
    typedef std::remove_reference<
        typename boost::mpl::at_c<full_para_t, 0>::type> this_t;
    typedef typename boost::mpl::pop_front<full_para_t>::type para_t;
    typedef typename MemberFunctionTransformer<T*, this_t,
            boost::mpl::size<para_t>::value, para_t>::type Wrapper;
    mod[method] = state.newCallable(Wrapper(method_ptr));
    return *this;
}

template<typename Class>
template<typename T>
typename std::enable_if<
    !std::is_member_function_pointer<T*>::value, Class_<Class>&>::type
Class_<Class>::def(const std::string& method, T callable)
{
    mod[method] = state.newCallable(callable);
    return *this;
}

template<typename Class>
Class_<Class>::Class_(const std::string& name, State& state)
    : name(name), state(state), mod(state.newTable()) {
    mod["modname"] = name;
    mod["modfilename"] = __FILE__;
}

inline State::State(const State& o) : ptr_(o.ptr_) {}

class NewState : public State {
public:
    NewState();
    ~NewState();
};

inline State::State(lua_State *p)
    : ptr_(p) {
}

inline lua_State* State::ptr() {
    return ptr_;
}

/* State with a new lua_State */
inline NewState::NewState()
    : State(luaL_newstate()) {
}

inline NewState::~NewState() {
    lua_close(ptr());
}

inline Table::Table(lua_State* st, int i)
    : state(st), index(lua_absindex(st,i)) {
    assert(state);
    assert(index);
}


inline Table::~Table() {
    cleanup(state, index);
}

inline UserData::~UserData() {
    cleanup(state, index);
}

inline Closure::~Closure() {
    cleanup(state, index);
}

template<typename T>
Variant<Table*, T, typename VarPusher<T>::type> Table::operator[](const T& k) {
    return  Variant<Table*, T, typename VarPusher<T>::type>(this, k);
}

inline void Table::set(const Table& metatab) {
    if (!VarPusher<Table>::push(state, metatab)) {
        throw RuntimeError("cannot push metatable");
    }
    lua_setmetatable(state, index);
}

struct NoMetatableError : public RuntimeError {
    NoMetatableError() : RuntimeError("") {}
};

inline Table Table::get() {
    if (!lua_getmetatable(state, index)) {
        throw NoMetatableError();
    }
    return Table(state, -1);
}

template<typename Sub>
void HasMetaTable<Sub>::set(const Table& metatab) {
    Sub* p = static_cast<Sub*>(this);
    {
        Guard<VarPushError> gd;
        gd.status = VarPusher<Table>::push(p->state, metatab);
    }
    lua_setmetatable(p->state, p->index);
}

template<typename Sub>
void HasMetaTable<Sub>::set(const std::string& registry_entry) {
    Sub* p = static_cast<Sub*>(this);
    lua_pushnil(p->state);
    lua_copy(p->state, p->index, -1);
    luaL_setmetatable(p->state, registry_entry.c_str());
}

template<typename Sub>
void HasMetaTable<Sub>::check(const std::string& registry_entry) {
    Sub* p = static_cast<Sub*>(this);
    State st(p->state);
    try {
        Table mtab = get();
        Table target_mtab = st.registry()[registry_entry];
        if (target_mtab != mtab) {
            st.error(std::string("expect a userdata (") + registry_entry + ")");
        }
    } catch (NoMetatableError& e) {
        st.error(std::string("userdata has no metatable, expect") + registry_entry);
    }
}


template<typename Sub>
Table HasMetaTable<Sub>::get() {
    Sub* p = static_cast<Sub*>(this);
    if (!lua_getmetatable(p->state, p->index)) {
        throw NoMetatableError();
    }
    return Table(p->state, -1);
}

template<typename C, int n>
struct CallLambda;

template<typename TL, int n>
struct TypeChecker {
    typedef typename boost::mpl::at_c<TL, n>::type Elem;
    typedef VarProxy<typename VarGetter<Elem>::type> Getter;
    static_assert(Getter::tid >= 0, "not a valid type");
    static void check(State& st, int offset) {
        TypeChecker<TL, n-1>::check(st, offset);
        int rtid = st[n+offset].type();
        if (rtid != Getter::tid) {
            st.error(std::string("bad argument#") + std::to_string(n) + " ("
                     + st.typerepr(Getter::tid) + " expected, got " +
                     st.typerepr(rtid) + ")");
        }
    }
};

template<typename T>
struct IsSingleReturnValue {
    enum { value = !std::is_same<typename VarPusher<T>::type, PlaceHolder>::value };
};

template<typename T>
struct SingleReturn {
    static void collect(State& st, T&& ret) {
        st[1] = ret;
    }
    enum { value = 1 };
};

template<typename Tuple, int n>
struct MultiReturnUnpack {
    static void unpack(State& st, Tuple&& ret) {
        st[n+1] = std::move(std::get<n>(ret));
        MultiReturnUnpack<Tuple, n-1>::unpack(st, std::forward<Tuple>(ret));
    }
};

template<typename Tuple>
struct MultiReturnUnpack<Tuple, -1> {
    static void unpack(State& st, Tuple&& ret) {}
};

template<typename T>
struct MultiReturn {
    enum { value = std::tuple_size<T>::value };
    static void collect(State& st, T&& ret) {
        MultiReturnUnpack<T, value - 1>::unpack(st, std::forward<T>(ret));
    }
};

template<typename T>
struct ReturnValue {
    typedef typename std::conditional<IsSingleReturnValue<T>::value,
        SingleReturn<T>,
        MultiReturn<T>>::type type;
    enum { value = type::value };
    static void collect(State& st, T&& ret) {
        type::collect(st, std::forward<T>(ret));
    }
};

template<>
struct ReturnValue<void> {
    enum { value = 0 };
};

template<typename TL>
struct TypeChecker<TL, 0> {
    static void check(State& st, int offset) {} };


template<typename F>
struct ToLambda {
    typedef decltype(&F::operator()) lambda_t;
    typedef typename boost::function_types::parameter_types<
        lambda_t>::type fullpara_t;

    // paramter list, include the leading State
    typedef typename boost::mpl::pop_front<fullpara_t>::type para_t;
};

template<typename F>
struct ToLambda<F*> {
    static_assert(std::is_function<F>::value, "F* should be a function pointer");
    typedef F lambda_t;
    typedef typename boost::function_types::parameter_types<
        lambda_t>::type para_t;
};

template<typename Callable>
struct IsCanonicalCallable {
    enum { value = std::is_same<State&,
                      typename boost::mpl::at_c<
                      typename ToLambda<Callable>::para_t, 0>::type>::value };
};

template<typename Callable, typename... Args>
struct CanonicalWrapper {
    Callable callable;
    CanonicalWrapper(Callable callable) : callable(callable) { }
    typename boost::function_types::result_type<
        typename ToLambda<Callable>::lambda_t>::type
    operator()(State&, Args... args) {
        return callable(std::forward<Args>(args)...);
    }
};

template<typename Callable, int size, typename PARALIST, typename... Args>
struct CanonicalTransformer {
    typedef typename CanonicalTransformer<Callable, size-1,
                typename boost::mpl::pop_front<PARALIST>::type,
                typename boost::mpl::at_c<PARALIST,0>::type,
                Args...>::type type;
};

template<typename Callable, typename PARALIST, typename... Args>
struct CanonicalTransformer<Callable, 0, PARALIST, Args...> {
    typedef CanonicalWrapper<Callable, Args...> type;
};

template<typename Callable>
struct CanonicalCallable {
    typedef typename ToLambda<Callable>::para_t para_t;
    typedef typename CanonicalTransformer<Callable,
                                          boost::mpl::size<para_t>::value,
                                          para_t>::type type;
};

// callables SHOULD carry State& as its first argument, if not, we would
// add one through this template
template<typename Callable>
struct ToCanonicalCallable {
    typedef typename std::conditional<IsCanonicalCallable<Callable>::value,
        Callable,
        typename CanonicalCallable<Callable>::type>::type type;
};

template<typename C>
struct CallableCall {
    typedef typename ToLambda<C>::para_t para_t;
    typedef typename boost::function_types::result_type<
        typename ToLambda<C>::lambda_t>::type result_t;

    typedef ReturnValue<result_t> RetType;

    typedef boost::mpl::size<para_t> nargs_t;

    struct NoRet {
        static void call(C& c, State& st) {
            CallLambda<C, nargs_t::value>::call(c, st, RetType::value);
        }
    };

    struct HasRet {
        static void call(C& c, State& st) {
            RetType::collect(st,
                CallLambda<C, nargs_t::value>::call(c, st, RetType::value));
        }
    };

    typedef typename std::conditional<RetType::value != 0,
            HasRet, NoRet>::type Ret;


    static void call(C c, lua_State* st) {
        State state(st);
        // type checking
        TypeChecker<para_t, boost::mpl::size<para_t>::value - 1>
            ::check(state, RetType::value);
        Ret::call(c, state);
    }
};

template<typename C>struct  CallLambda<C, 1> {
    static typename CallableCall<C>::result_t call(C& func, State& st, int offset) {
        return func(st);
    }}; // special case, c function has no argument

#ifndef LUAMM_LAMBDA_PARANUMBER
#define LUAMM_LAMBDA_PARANUMBER 15
#endif
#define LUAMM_ARGPACK(n) st[n + offset - 1]
#define LUAMM_ARGLIST(z, n, _) LUAMM_ARGPACK(BOOST_PP_ADD(n, 2)),
#define LUAMM_ARG(n) BOOST_PP_REPEAT(BOOST_PP_SUB(n, 1), LUAMM_ARGLIST, ) LUAMM_ARGPACK(BOOST_PP_INC(n))
#define LUAMM_TEMPL(_a, n, _b) template<typename C>struct CallLambda<C, n> {\
    static typename CallableCall<C>::result_t call(C& func, State& st, int offset) {\
        return func(st, LUAMM_ARG(BOOST_PP_SUB(n,1))); }};

BOOST_PP_REPEAT_FROM_TO(2, LUAMM_LAMBDA_PARANUMBER, LUAMM_TEMPL,)
#undef LUAMM_ARGPACK
#undef LUAMM_ARGLIST
#undef LUAMM_ARG
#undef LUAMM_TEMPL
#undef LUAMM_LAMBDA_PARANUMBER


typedef std::function<int(lua_State*)> lua_Lambda;

namespace {
    struct helper {

        static int luamm_cclosure(lua_State* _)
        {
            State st(_);
            UserData ud = st[st.upvalue(1)];
            auto& lambda = ud.to<lua_Lambda>();
            return lambda(_);
        }

        static int luamm_cleanup(lua_State* _)
        {
            State st(_);
            UserData ud = st[1];
            auto& lambda = ud.to<lua_Lambda>();
            lambda.~lua_Lambda();
            return 0;
        }
    };
}

template<typename F>
Closure State::newCallable(F func, int extra_upvalues)
{
    typename ToCanonicalCallable<F>::type canonical_callable(func);

    typedef ReturnValue<typename CallableCall<F>::result_t> RetType;
    lua_Lambda lambda = [canonical_callable](lua_State* st) -> int {
        // allocate a slot for return value
        State lua(st);

        const int rets = RetType::value;

        // shift +1 to allocate slot for return value
        for (auto i = 1; i <= rets; i++) {
            lua_pushboolean(st, 1);
            lua_insert(st, i);
        }

        try {
        CallableCall<decltype(canonical_callable)>::call(canonical_callable, st);
        } catch (std::exception& e) {
            lua.error(e.what());
        }

        // leave return value one the stack, wipe out other things
        lua_settop(st, rets);
        return rets;
    };

    push(CClosure(&helper::luamm_cclosure, 1 + extra_upvalues));
    Closure cl = this->operator[](-1);
    UserData ud = newUserData<lua_Lambda>(lambda);

    {
        Table reg = registry();
        auto gctab = reg["LUAMM_COMMON_GC"];
        if (!gctab.istab()) {
            Table mtab = newTable();
            mtab["__gc"] = CClosure(helper::luamm_cleanup);
            gctab = mtab;
        }

        Table mtab = gctab;
        ud.set(mtab);
    }

    cl[1] = ud;
    return cl;
}

} // end namespace

#define LUAMM_MODULE(name, state) extern "C" int luaopen_##name(lua_State *state)

#endif
