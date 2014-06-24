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

template<typename LuaValue>
struct VarProxy;

template<>
struct VarProxy<std::string> {
    static bool push(lua_State* st, const std::string& v) {
        return lua_pushstring(st, v.c_str()) ? true : false;
    }
};

template<>
struct VarProxy<const char*> {
    static const char *get(lua_State* st, int index, bool& success) {
        const char * r = lua_tostring(st, index);
        success = r ? true : false;
        return r;
    }
};

template<>
struct VarProxy<Number> {
    static bool push(lua_State* st, Number num) {
        lua_pushnumber(st, num);
        return true;
    }

    static Number get(lua_State* st, int index, bool& success) {
        int isnum;
        Number r = lua_tonumberx(st, index, &isnum);
        success = r ? true : false;
        return r;
    }
};


typedef boost::mpl::list<
            std::string,
            const char*,
            Number
        > varproxies;
struct PlaceHolder {};

template<typename Proxy, typename Var>
struct PredPush {
    typedef struct { char _[]; } two;
    typedef char one;

    template<typename P, typename V>
    static one test(
            decltype(P::push(
                static_cast<lua_State*>(nullptr),
                std::declval<V>()
            )));

    template<typename P, typename V>
    static two test(...);

    typedef boost::mpl::bool_<sizeof(test<Proxy, Var>(true)) == 1> type;
};


template<typename Proxy, typename Var>
struct PredGet {
    typedef struct { char _[]; } two;
    typedef char one;

    template<typename P, typename V>
    static one test(
        typename std::conditional<
            std::is_convertible<
                decltype(P::get(
                    static_cast<lua_State*>(nullptr),
                    static_cast<int>(2),
                    std::declval<bool&>()
                )),
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
        boost::mpl::if_<Pred<VarProxy<boost::mpl::_2>, T>,
                        boost::mpl::_2, // select current item
                        boost::mpl::_1> // keep last item
    >::type impl;
    static_assert( ! std::is_same<PlaceHolder, impl>::value,
            "no implmentation can be selected for T" );
    typedef VarProxy<impl> type;
};

template<>
struct VarProxy<Nil> {
    static bool push(lua_State* st, Nil _) {
        lua_pushnil(st);
        return true;
    }
};

template<>
struct VarProxy<bool> {
    static bool push(lua_State* st, bool b) {
        lua_pushboolean(st, b);
        return true;
    }

    static bool get(lua_State* st, int index, bool& success) {
        success = true;
        return lua_toboolean(st, index);
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
                 VarProxy<T>
            >::type type;
    static bool push(lua_State* st, const T& v) {
        return type::push(st, v);
    }
};

template<typename T>
struct VarGetter {
    typedef typename std::conditional<VarDispatcher<T>::use_indirect_get,
                 typename SelectImpl<PredGet, T>::type,
                 VarProxy<T>
            >::type type;
    static T get(lua_State* st, int index, bool& success) {
        return type::get(st, index, success);
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
struct VarProxy<Table> {
    static bool push(lua_State* st, const Table& tb) {
        lua_pushnil(st);
        lua_copy(st, tb.index, -1);
        return true;
    }

    static Table get(lua_State* st, int index, bool& success) {
        if (!lua_istable(st, index)) {
            throw RuntimeError("is not a table");
        }
        return Table(st, index);
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
