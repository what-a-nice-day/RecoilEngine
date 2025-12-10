#include "./lua.h"
#include "./lauxlib.h"
#include "./lua_privileges.h"

int (luaL_loadbuffer_privileged) (lua_State *L, const char *buff, size_t sz,
                                  const char *name, bool privileged)
{
    return luaL_loadbuffer(L, buff, sz, name);
}

int   (lua_load_privileged) (lua_State *L, lua_Reader reader, void *dt,
                                        const char *chunkname)
{
    return lua_load(L, reader, dt, chunkname);
}