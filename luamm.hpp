#ifndef LUAMM_HPP
#define LUAMM_HPP

#include <lua.hpp>
#include <string>
#include <functional>
#include <type_traits>
#include <boost/mpl/list.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/push_back.hpp>

namespace luamm {

/* make aliases for lua types */
typedef lua_Number Number;

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

    static const Number get(lua_State* st, int index, bool& success) {
        int isnum;
        Number r = lua_tonumberx(st, index, &isnum);
        success = r ? true : false;
        return r;
    }
};

typedef boost::mpl::list<std::string,const char*, Number> varproxies;

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

class State {
protected:
    lua_State *ptr;
public:
    State(lua_State *p);
    lua_State* get();

    template<typename T>
    void push(T value);
};

class NewState : public State {
public:
    NewState(lua_State *p);
    ~NewState();
};

inline State::State(lua_State *p)
    : ptr(p) {
}

inline lua_State* State::get() {
    return ptr;
}

inline NewState::NewState(lua_State *p)
    : State(p) {
}

inline NewState::~NewState() {
    lua_close(ptr);
}

} // end namespace

#endif
