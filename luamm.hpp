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

// All template implementations should only depends on lua c api

/*
 *  VarProxy is a template struct which provide functions for store/load
 *  lua variable from/to the stack top.
 *
 *  bool push(T variable)
 *      - push variable of type T to stack
 *  T get(int index, bool& success)
 *      - return variable in stack position `index`
 *      - set success to false when unsuccess
 */
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
template<typename T>
struct VarDispatcher;

template<>
struct VarDispatcher<Nil> {
    static bool push(lua_State* st, Nil _) {
        lua_pushnil(st);
        return true;
    }
};


template<>
struct VarDispatcher<bool> {
    static bool push(lua_State* st, bool b) {
        lua_pushboolean(st, b);
        return true;
    }

    static bool get(lua_State* st, int index, bool& success) {
        success = true;
        return lua_toboolean(st, index);
    }
};

// catch all
template<typename T>
struct VarDispatcher {
    static bool push(lua_State* st, const T& v) {
        return SelectImpl<PredPush, T>::type::push(st, v);
    }

    static T get(lua_State* st, int index, bool& success) {
        return SelectImpl<PredGet, T>::type::get(st, index, success);
    }
};

/*
 * KeyProxy is a template that translate an Key(a position point to
 * somewhere inside lua state) to a stack position, either valid or pseudo.
 *
 * int get(Key k, bool& success)
 *      - if k is an int (actual position), just return it as is
 *      - otherwise, push that value to the stack and return 0, which
 *      -- implies that caller should clean the stack top after return
 *      - success denotes if the action is completed successfully
 *
 * bool put(Key k)
 *      - push the variable pointed by k to the stack
 *      - check return value to see if completed successfully
 */
template<typename K>
struct KeyProxy;

template<>
struct KeyProxy<int> {
    static int get(lua_State *st, int i, bool& success) {
        success = true;
        return i;
    }

    static bool put(lua_State *st, int i) {
        lua_pushnil(st);
        lua_copy(st, i, -1);
        return true;
    }
};

struct RuntimeError : std::runtime_error {
    RuntimeError(const std::string& s) : std::runtime_error(s) {}
};

struct KeyGetError : RuntimeError {
    KeyGetError() : RuntimeError("") {}
};

struct VarGetError : RuntimeError {
    VarGetError() : RuntimeError("") {}
};

struct VarPushError : RuntimeError {
    VarPushError() : RuntimeError("") {}
};

class Variant {
    int  index;
    lua_State* state;
public:
    Variant(lua_State* st, int i) : index(i), state(st) {}

    template<typename T>
    operator T();

    operator std::string();

    template<typename T>
    Variant& operator=(const T& var);
};

template<typename T>
Variant::operator T() {
    bool success = false;
    T out = VarDispatcher<T>::get(state, index, success);
    if (!success) {
        throw VarGetError();
    }
    return out;
}

template<typename T>
Variant& Variant::operator=(const T& var) {
    if (VarDispatcher<T>::push(state, var)) {
        throw VarPushError();
    }
    lua_copy(state, -1, index);
    lua_pop(state, 1);
    return *this;
}

class State {
protected:
    lua_State *ptr_;
public:
    State(lua_State *p);
    lua_State* ptr();

    void pop(int n=1);

    template<typename T>
    void push(T value);

    template<typename K>
    Variant operator[](const K& key);
};

inline void State::pop(int n) {
    lua_pop(ptr(), n);
}


template<typename K>
Variant State::operator[](const K& key) {
    bool success = false;
    int i = KeyProxy<K>::get(ptr(), key, success);
    if (!success) {
        throw KeyGetError();
    }

    if (i == 0) {
        i = -1;
    }

    if (i < 0) {
        i = lua_absindex(ptr(), i);
    }
    return Variant(ptr(), i);
}

template<typename T>
void State::push(T value) {
    VarDispatcher<T>::push(ptr(), value);
}

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
