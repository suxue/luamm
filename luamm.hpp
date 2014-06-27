#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <string>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/size.hpp>
#include <boost/function_types/parameter_types.hpp>
#include <boost/function_types/result_type.hpp>
#include <boost/preprocessor.hpp>

namespace luamm {

/* make aliases for lua types */
typedef lua_Number Number;

class Nil {};

typedef lua_CFunction CFunction;

template<typename T>
struct StackVariable {
    enum { value = 0 };
};

struct RuntimeError : std::runtime_error {
    RuntimeError(const std::string& s) : std::runtime_error(s) {}
    RuntimeError() : std::runtime_error("") {}
};


template<typename T>
struct VarPusher;

template<typename Container, typename Key>
struct Accessor;

template<typename Container, typename Key,
        typename KeyStore = Key, bool autoclean = false>
class Variant  {
    KeyStore index;
    Container state;
public:
    Variant(Container st, const Key& i) : index(i), state(st) {}

    template<typename T>
    operator T() const  {
        return Accessor<Container, KeyStore>::template get<T>(state, index);
    }

    template<typename T>
    Variant& operator=(const T& var) {
        Accessor<Container, KeyStore>::template set<T>(state, index, var);
        return *this;
    }

    int type() {
        return Accessor<Container, KeyStore>::type(state, index);
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
            Accessor<Container, KeyStore>::template get<CFunction>(state, index);
        } catch (RuntimeError e) {
            return false;
        }
        return true;
    }
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
    Table(Table&& o) : state(o.state), index(o.index) {
        o.state = nullptr;
        o.index = 0;
    }
private:
    Table(const Table&);
};

template<typename Sub>
struct HasMetaTable {
    void set(const Table& metatab);
    Table get();
};

struct CClosure {
    int index;
    CFunction func;
    // used as in parameter, represents number of upvalues
    // used as out parameter, represents index in the stack
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
    T* to() {
        void *p = lua_touserdata(state, index);
        return static_cast<T*>(p);
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

// light userdata
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

template<typename T>
struct PassingConvention {
    typedef T type;
};

template<>
struct PassingConvention<Table> {
    typedef Table& type;
};


struct Closure : public HasMetaTable<Closure> {
    lua_State* state;
    int index;
    Closure(lua_State* st, int index) : state(st), index(lua_absindex(st, index)) {}
    Variant<Closure*, int>  operator[](int n) {
        return Variant<Closure*, int>(this, n);
    }
    ~Closure() {
        if (state) {
            if (lua_gettop(state) == index) {
                lua_pop(state, 1);
            }
            //} else {
                //throw RuntimeError(
                    //std::string("cannot clean up stack variable (closure)")
                        //+ std::to_string(index));
            //}
        }
    }
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
                    Variant<lua_State*,int, int, true>,
                    std::array<Variant<lua_State*, int, int, true>, rvals>
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
struct PassingConvention<Closure> {
    typedef Closure& type;
};

template<>
inline void Closure::__return__<0>() {}

template<>
inline typename Closure::Rvals<1>::type Closure::__return__<1>() {
    // auto cleanable variant
    return Closure::Rvals<1>::type(state, lua_gettop(state));
}
// TODO multiple return values

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



class AutoPopper {
    lua_State* state;
    int n;
public:
    AutoPopper(lua_State* st, int n = 1) : state(st), n(n) {}
    ~AutoPopper() { if (n > 0) lua_pop(state, n);  }
};


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

template<typename T>
struct VarPusher {
    typedef typename std::conditional<VarDispatcher<T>::use_indirect_push,
                 typename SelectImpl<PredPush, T>::type,
                 T
            >::type type;
    static_assert( ! std::is_same<PlaceHolder, type>::value,
            "no push() implmentation can be selected for T" );
    static bool push(lua_State* st, const T& v) {
        return VarBase(st).to<type>().push(v);
    }
};

template<typename T>
struct VarGetter {
    typedef typename std::conditional<VarDispatcher<T>::use_indirect_get,
                 typename SelectImpl<PredGet, T>::type,
                 T
            >::type type;
    static_assert( ! std::is_same<PlaceHolder, type>::value,
            "no get() implmentation can be selected for T" );
    static T get(lua_State* st, int index, bool& success) {
        return VarBase(st).to<type>().get(index, success);
    }
};

template<typename Container, typename Key>
struct Accessor;


template<>
struct Accessor<Closure*, int> {
    template<typename Var>
    static Var get(Closure* cl, int key) {
        auto p = lua_getupvalue(cl->state, cl->index, key);
        if (!p) { throw VarGetError(); }
        Guard<VarGetError> gd;
        AutoPopper ap(cl->state, 1 - StackVariable<Var>::value);
        return VarGetter<Var>::get(cl->state, -1, gd.status);
    }

    static int type(Closure* cl, int key) {
        auto p = lua_getupvalue(cl->state, cl->index, key);
        if (!p) { throw VarGetError(); }
        AutoPopper ap(cl->state);
        return lua_type(cl->state, -1);
    }

    template<typename Var>
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
template<>
struct Accessor<lua_State*, int> {
    template<typename Var>
    static Var get(lua_State* container, int key) {
        Guard<VarGetError> gd;
        return VarGetter<Var>::get(container, key, gd.status);
    }

    template<typename Var>
    static void set(lua_State* container, int key, const Var& nv) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Var>::push(container, nv);
        }
        AutoPopper ap(container);
        lua_copy(container, -1, key);
    };

    static int type(lua_State* st, int i) {
        return lua_type(st, i);
    }
};

// auto clean variant if it is not a stack variable(which handle life
// crycle by itself, this process is conservative:
// that if not on top, do noting
template<>
template<typename T>
Variant<lua_State*, int, int, true>::operator T() const {
    T o = Accessor<lua_State*, int>::template get<T>(state, index);
    if (lua_gettop(state) == index && !StackVariable<T>::value) {
        lua_pop(state, 1);
    }
    return o;
}

// global variable key
template<>
struct Accessor<lua_State*, std::string> {
    template<typename Var>
    static Var get(lua_State* container, const std::string& key) {
        lua_getglobal(container, key.c_str());
        Guard<VarGetError> gd;
        AutoPopper ap(container, 1 - StackVariable<Var>::value);
        return VarGetter<Var>::get(container, -1, gd.status);
    }

    static int type(lua_State* st, const std::string& k) {
        lua_getglobal(st, k.c_str());
        AutoPopper ap(st);
        return lua_type(st, -1);
    }

    template<typename Var>
    static void set(lua_State* container, const std::string& key, const Var& nv) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Var>::push(container, nv);
        }
        lua_setglobal(container, key.c_str());
    }
};


template<typename Key>
struct Accessor<Table*, Key> {

    static int type(Table *t, const Key& k) {
        {
            Guard<VarPushError> gd;
            gd.status = VarPusher<Key>::push(t->state, k);
        }
        lua_gettable(t->state, t->index);
        AutoPopper ap(t->state);
        return lua_type(t->state, -1);
    }

    template<typename Var>
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

    template<typename Var>
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

class State {
protected:
    lua_State *ptr_;
public:

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
    UserData newUserData(Args... args) {
        void * buf = lua_newuserdata(ptr(), sizeof(T));
        new (buf) T(args...);
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
    Closure newCallable(F func);

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
};

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

inline NewState::NewState()
    : State(luaL_newstate()) {
}

inline NewState::~NewState() {
    lua_close(ptr());
}

inline Table::Table(lua_State* st, int i)
    : state(st), index(lua_absindex(st,i)) {}

inline Table::~Table() {
    if (state) {
        if (lua_gettop(state) == index) {
            lua_pop(state, 1);
        }
        //} else {
            //throw RuntimeError(
                //std::string("cannot clean up stack variable (table)")
                    //+ std::to_string(index));
        //}
    }
}

inline UserData::~UserData() {
    if (state) {
        if (lua_gettop(state) == index) {
            lua_pop(state, 1);
        }
        //else {
            //throw RuntimeError(
                //std::string("cannot clean up stack variable (userdata)")
                    //+ std::to_string(index));
        //}
    }
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
    static void check(State& st) {
        TypeChecker<TL, n-1>::check(st);
        int rtid = st[n+1].type();
        if (rtid != Getter::tid) {
            st.error(std::string("bad argument#") + std::to_string(n) + " ("
                     + st.typerepr(Getter::tid) + " expected, got " +
                     st.typerepr(rtid) + ")");
        }
    }
};

template<typename TL>
struct TypeChecker<TL, 0> { static void check(State& st) {} };

template<typename C>
struct CallableCall {
    typedef C  lambda_t;
    // formal parameters
    typedef typename boost::function_types::parameter_types<
        decltype(&lambda_t::operator())>::type fullpara_t;

    typedef typename boost::function_types::result_type<
        decltype(&lambda_t::operator())>::type result_t;

    // paramter list, include the leading State
    typedef typename boost::mpl::pop_front<fullpara_t>::type para_t;

    typedef boost::mpl::size<para_t> nargs_t;

    static result_t call(C c, lua_State* st) {
        State state(st);
        // type checking
        TypeChecker<para_t, boost::mpl::size<para_t>::value - 1>::check(state);
        return CallLambda<C, nargs_t::value>::call(c, state);
    }
};

template<typename C>struct  CallLambda<C, 1> {
    static typename CallableCall<C>::result_t call(C& func, State& st) {
        return func(st);
    }}; // special case, c function has no argument

#ifndef LUAMM_LAMBDA_PARANUMBER
#define LUAMM_LAMBDA_PARANUMBER 15
#endif
#define LUAMM_ARGPACK(n) st[n]
#define LUAMM_ARGLIST(z, n, _) LUAMM_ARGPACK(BOOST_PP_ADD(n, 2)),
#define LUAMM_ARG(n) BOOST_PP_REPEAT(BOOST_PP_SUB(n, 1), LUAMM_ARGLIST, ) LUAMM_ARGPACK(BOOST_PP_INC(n))
#define LUAMM_TEMPL(_a, n, _b) template<typename C>struct CallLambda<C, n> {\
    static typename CallableCall<C>::result_t call(C& func, State& st) {\
        return func(st, LUAMM_ARG(BOOST_PP_SUB(n,1))); }};

BOOST_PP_REPEAT_FROM_TO(2, LUAMM_LAMBDA_PARANUMBER, LUAMM_TEMPL,)
#undef LUAMM_ARGPACK
#undef LUAMM_ARGLIST
#undef LUAMM_ARG
#undef LUAMM_TEMPL
#undef LUAMM_LAMBDA_PARANUMBER


extern int luamm_cclosure(lua_State*);
extern int luamm_cleanup(lua_State*);

typedef std::function<int(lua_State*)> lua_Lambda;

template<typename F>
Closure State::newCallable(F func)
{
    lua_Lambda lambda = [func](lua_State* st) -> int {
        // allocate a slot for return value
        State lua(st);

        // shift +1 to allocate slot for return value
        lua_pushnil(st);
        lua_insert(st, 1);

        lua[1] = CallableCall<F>::call(func, st);

        // leave return value one the stack, wipe out other things
        lua_settop(st, 1);
        return 1;
    };

    push(CClosure(luamm_cclosure, 1));
    Closure cl = this->operator[](-1);
    UserData ud = newUserData<lua_Lambda>(lambda);

    {
        Table reg = registry();
        auto gctab = reg["LUAMM_COMMON_GC"];
        if (!gctab.istab()) {
            Table mtab = newTable();
            mtab["__gc"] = CClosure(luamm_cleanup);
            gctab = mtab;
        }

        Table mtab = gctab;
        ud.set(mtab);
    }

    cl[1] = ud;
    return cl;
}

} // end namespace

#endif
