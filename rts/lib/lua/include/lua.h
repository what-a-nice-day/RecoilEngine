#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../src/lua.h"


static inline float      (lua_tonumber) (lua_State *L, int idx) {
    return static_cast<float>(lua_tonumber_double(L, idx));
}
static inline void       (lua_pushnumber) (lua_State *L, float n) {
    lua_pushnumber_double(L, static_cast<lua_Number>(n));
}


#ifdef __cplusplus
}
#endif