#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <string>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <boost/mpl/list.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/fold.hpp>

namespace luamm {

/* make aliases for lua types */
typedef lua_Number Number;

class Nil {};

typedef lua_CFunction CFunction;

struct CClosure {
    CFunction func;
    int upvalues;
public:
    CClosure(CFunction func, int upvalues = 0)
        : func(func), upvalues(upvalues) {}
    operator CFunction() {
        return func;
    }
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
        lua_pushcclosure(state, c.func, c.upvalues);
        return true;
    }

    CClosure get(int index, bool& success) {
        auto p = lua_tocfunction(state, index);
        if (p) success = true;
        return p;
    }
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
        success = r ? true : false;
        return r;
    }
};


typedef boost::mpl::list<
            std::string,
            const char*,
            Number,
            CClosure
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
struct SelectImpl {
    typedef typename boost::mpl::fold<
        varproxies,
        PlaceHolder,
        // _1 is state, _2 is current item
        boost::mpl::if_<Pred<boost::mpl::_2, T>,
                        boost::mpl::_2, // select current item
                        boost::mpl::_1> // keep last item
    >::type type;
    static_assert( ! std::is_same<PlaceHolder, type>::value,
            "no implmentation can be selected for T" );
};

template<>
struct VarProxy<Nil> : VarBase {
    bool push(Nil _) {
        lua_pushnil(state);
        return true;
    }
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
};


struct RuntimeError : std::runtime_error {
    RuntimeError(const std::string& s) : std::runtime_error(s) {}
};
struct KeyGetError : RuntimeError { KeyGetError() : RuntimeError("") {} };
struct KeyPutError : RuntimeError { KeyPutError() : RuntimeError("") {} };
struct VarGetError : RuntimeError { VarGetError() : RuntimeError("") {} };
struct VarPushError : RuntimeError { VarPushError() : RuntimeError("") {} };

template<typename Container, typename Key>
struct SetterGetter;

template<typename Exception>
struct Guard {
    bool status;
    Guard() : status(false) {}
    ~Guard() { if (!status) throw Exception(); }
};

class AutoPopper {
    lua_State* state;
    int n;
public:
    AutoPopper(lua_State* st, int n = 1) : state(st), n(n) {}
    ~AutoPopper() { if (n > 0) lua_pop(state, n);  }
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
    static T get(lua_State* st, int index, bool& success) {
        return VarBase(st).to<type>().get(index, success);
    }
};

template<>
struct SetterGetter<lua_State*, int> {
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
};


template<typename Container, typename Key>
class Variant  {
    Key index;
    Container state;
public:
    Variant(Container st, const Key& i) : index(i), state(st) {}

    template<typename T>
    operator T() {
        return SetterGetter<Container, Key>::template get<T>(state, index);
    }

    template<typename T>
    Variant& operator=(const T& var) {
        SetterGetter<Container, Key>::template set<T>(state, index, var);
        return *this;
    }
};

struct Table {
    lua_State* state;
    int index;
    Table(lua_State* st, int i) : state(st), index(i) {}

    template<typename T>
    Variant<Table*, T> operator[](const T& k) {
        return  Variant<Table*, T>(this, k);
    }
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
        return Table(state, index);
    }
};

template<typename Key>
struct SetterGetter<Table*, Key> {
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
        AutoPopper ap(container->state);
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
    void push(T value) {
        VarPusher<T>::push(ptr(), value);
    }

    Table newTable(int narray = 0, int nother = 0) {
        lua_createtable(ptr(), 0, 0);
        return Table(ptr(), top());
    }

    Variant<lua_State*, int> operator[](int pos) {
        return Variant<lua_State*, int>(ptr(), pos);
    }

    int top() {
        return lua_gettop(ptr());
    }
};

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


} // end namespace

#endif
