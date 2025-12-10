#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lib/lua/src/lauxlib.h"

#define luaI_openlib	luaL_openlib
#define luaL_reg	luaL_Reg

static inline float (luaL_checknumber) (lua_State *L, int numArg) {
    return (float)luaL_checknumber_double(L, numArg);
}
static inline float (luaL_optnumber) (lua_State *L, int nArg, float def) {
    return (float)luaL_optnumber_double(L, nArg, (lua_Number)def);
}

#ifdef __cplusplus
}
#endif  