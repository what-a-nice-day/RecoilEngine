#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "../src/luaconf.h"

#define lua_number2str(s,n)	spring_lua_ftoa((n),(s))
#define lua_number2fmt(s,fmt,n)	spring_lua_format((n), (fmt), (s))

#ifdef __cplusplus
}
#endif