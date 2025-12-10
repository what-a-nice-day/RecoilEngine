/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "SerializeLuaState.h"

#include "VarTypes.h"

#ifndef UNIT_TEST
#include "Sim/Features/FeatureDefHandler.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Weapons/WeaponDefHandler.h"
#include "System/TimeProfiler.h"
#endif

// #include "lib/lua/src/ltable.h"
#include "System/UnorderedMap.hpp"
#include "System/StringUtil.h"
#include "System/Log/ILog.h"
#include <deque>

struct creg_lua_State;
struct creg_Proto;
struct creg_UpVal;
struct creg_Node;
struct creg_Table;
union creg_GCObject;
struct creg_TString;

// these are copied from lua lib, need to fix/refactor later, this file is a mess
typedef unsigned char lu_byte;
typedef unsigned int lu_int32;
typedef lu_int32 Instruction;
typedef size_t lu_mem;
#define LUAI_USER_ALIGNMENT_T	union { double u; void *s; long l; }
typedef LUAI_USER_ALIGNMENT_T L_Umaxalign;
typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;
#define NUM_TAGS	9
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_MOD,
  TM_POW,
  TM_UNM,
  TM_LEN,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;
typedef TValue *StkId;
typedef struct CallInfo {
//   StkId base;  /* base for this function */
//   StkId func;  /* function index in the stack */
//   StkId	top;  /* top for this function */
  const Instruction *savedpc;
  int nresults;  /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
} CallInfo;
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked

// typedef struct GCheader {
//   CommonHeader;
// } GCheader;

// #define ASSERT_SIZE(structName) static_assert(sizeof(creg_ ## structName) == sizeof(structName), #structName " Size mismatch");

// static_assert(LUAI_EXTRASPACE == 0, "LUAI_EXTRASPACE isn't 0");

class LuaContext{
public:
	LuaContext() = default;
	void* alloc(size_t n) const {
		assert(context != nullptr && frealloc != nullptr);
		return frealloc(context, nullptr, 0, n);
	}
	void SetContext(void* newLcd, lua_Alloc newfrealloc, lua_CFunction newPanic) { context = newLcd; frealloc = newfrealloc; panic = newPanic; }
	void SetMainthread(lua_State* L) { mainthread = L; }
	lua_CFunction GetPanic() const { return panic; }
	void* GetContext() const { return context; }
	lua_Alloc Getfrealloc() const { return frealloc; }
	lua_State* GetMainthread() const { return mainthread; }
private:
	void* context = nullptr;
	lua_Alloc frealloc = nullptr;
	lua_CFunction panic = nullptr;
	lua_State* mainthread;
};

void freeProtector(void *m) {
	assert(false);
}

void* allocProtector(size_t size) {
	assert(false);
	return nullptr;
}


static LuaContext luaContext;

// Used for hackish heuristic for deciphering lightuserdata
static bool inClosure = false;

// C functions in lua have to be specially registered in order to
// be serialized correctly
static spring::unsynced_map<std::string, lua_CFunction> nameToFunc;
static spring::unsynced_map<lua_CFunction, std::string> funcToName;


/*
 * Copied from lfunc.h
 */


#define sizeCclosure(n)	(lua_cast(int, sizeof(CClosure)) + \
                         lua_cast(int, sizeof(TValue)*((n)-1)))

#define sizeLclosure(n)	(lua_cast(int, sizeof(LClosure)) + \
                         lua_cast(int, sizeof(TValue *)*((n)-1)))


/*
 * Converted from lobject.h
 */

#define creg_CommonHeader creg_GCObject* next; lu_byte tt; lu_byte marked
#define CR_COMMON_HEADER() CR_MEMBER(next), CR_MEMBER(tt), CR_MEMBER(marked)

union creg_Value{
	creg_GCObject *gc;
	void *p;
	lua_Number n;
	int b;
};

struct creg_TValue {
	CR_DECLARE_STRUCT(creg_TValue)
	creg_Value value;
	int tt;
	void Serialize(creg::ISerializer* s);
};

// ASSERT_SIZE(TValue)

union creg_TKey {
	struct {
		creg_Value value;
		int tt;
		creg_Node *next;  /* for chaining */
	} nk;
	creg_TValue tvk;
};


struct creg_Node {
	CR_DECLARE_STRUCT(creg_Node)
	creg_TValue i_val;
	creg_TKey i_key;
	void Serialize(creg::ISerializer* s);
};

// ASSERT_SIZE(Node)


struct creg_Table {
	CR_DECLARE_STRUCT(creg_Table)
	creg_CommonHeader;
	lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
	lu_byte lsizenode;  /* log2 of size of `node' array */
	creg_Table *metatable;
	creg_TValue *array;  /* array part */
	creg_Node *node;
	creg_Node *lastfree;  /* any free position is before this position */
	creg_GCObject *gclist;
	int sizearray;  /* size of `array' array */
	void Serialize(creg::ISerializer* s);
	void PostLoad();
};

// ASSERT_SIZE(Table)


struct creg_LocVar {
	CR_DECLARE_STRUCT(creg_LocVar)
	creg_TString *varname;
	int startpc;  /* first point where variable is active */
	int endpc;    /* first point where variable is dead */
};

// ASSERT_SIZE(LocVar)


struct creg_Proto {
	CR_DECLARE_STRUCT(creg_Proto)
	creg_CommonHeader;
	creg_TValue *k;  /* constants used by the function */
	Instruction *code;
	creg_Proto **p;  /* functions defined inside the function */
	int *lineinfo;  /* map from opcodes to source lines */
	creg_LocVar *locvars;  /* information about local variables */
	creg_TString **upvalues;  /* upvalue names */
	creg_TString *source;
	int sizeupvalues;
	int sizek;  /* size of `k' */
	int sizecode;
	int sizelineinfo;
	int sizep;  /* size of `p' */
	int sizelocvars;
	int linedefined;
	int lastlinedefined;
	creg_GCObject *gclist;
	lu_byte nups;  /* number of upvalues */
	lu_byte numparams;
	lu_byte is_vararg;
	lu_byte maxstacksize;
	void Serialize(creg::ISerializer* s);
};


// ASSERT_SIZE(Proto)


struct creg_UpVal {
	CR_DECLARE_STRUCT(creg_UpVal)
	creg_CommonHeader;
	creg_TValue *v;  /* points to stack or to its own value */
	union {
		creg_TValue value;  /* the value (when closed) */
		struct {  /* double linked list (when open) */
			struct creg_UpVal *prev;
			struct creg_UpVal *next;
		} l;
	} u;
	void Serialize(creg::ISerializer* s);
};
// 
// ASSERT_SIZE(UpVal)


struct creg_TString {
	CR_DECLARE_STRUCT(creg_TString)
	union {
		L_Umaxalign dummy;  /* ensures maximum alignment for strings */
		struct {
			creg_CommonHeader;
			lu_byte reserved;
			unsigned int hash;
			size_t len;
		} tsv;
	} u;
	void Serialize(creg::ISerializer* s);
	size_t GetSize();
};

// ASSERT_SIZE(TString)

#define creg_ClosureHeader \
	creg_CommonHeader; lu_byte isC; lu_byte nupvalues; creg_GCObject *gclist; \
	creg_Table *env

#define CR_CLOSURE_HEADER() CR_COMMON_HEADER(), CR_MEMBER(isC), CR_MEMBER(nupvalues), CR_IGNORED(gclist), \
	CR_MEMBER(env)

struct creg_CClosure {
	CR_DECLARE_STRUCT(creg_CClosure)
	creg_ClosureHeader;
	lua_CFunction f;
	creg_TValue upvalue[1];
	void Serialize(creg::ISerializer* s);
	size_t GetSize();
};

// ASSERT_SIZE(CClosure)

struct creg_LClosure {
	CR_DECLARE_STRUCT(creg_LClosure)
	creg_ClosureHeader;
	creg_Proto *p;
	creg_UpVal *upvals[1];
	void Serialize(creg::ISerializer* s);
	size_t GetSize();
};

// ASSERT_SIZE(LClosure)

union creg_Closure {
	creg_CClosure c;
	creg_LClosure l;
};

// ASSERT_SIZE(Closure)


struct creg_Udata {
	CR_DECLARE_STRUCT(creg_Udata)
	union {
		L_Umaxalign dummy;  /* ensures maximum alignment for strings */
		struct {
			creg_CommonHeader;
			creg_Table *metatable;
			creg_Table *env;
			size_t len;
		} uv;
	} u;
	void Serialize(creg::ISerializer* s);
	size_t GetSize();
};

// ASSERT_SIZE(Udata)


creg_Node* GetDummyNode()
{
	// static creg_Node* dummyNode = nullptr;
	// if (dummyNode != nullptr)
	// 	return dummyNode;

	// lua_State* L = lua_open();
	// lua_newtable(L);
	// creg_Table* t = (creg_Table*) lua_topointer(L, -1);
	// dummyNode = t->node;
	// lua_close(L);

	// return dummyNode;
	return nullptr;
}


/*
 * Converted from lstate.h
 */

struct creg_stringtable {
	CR_DECLARE_STRUCT(creg_stringtable)
	creg_GCObject **hash;
	lu_int32 nuse;  /* number of elements */
	int size;
	void Serialize(creg::ISerializer* s);
};

// ASSERT_SIZE(stringtable)


struct creg_global_State {
	CR_DECLARE_STRUCT(creg_global_State)
	creg_stringtable strt;  /* hash table for strings */
	lua_Alloc frealloc;  /* function to reallocate memory */
	void *ud;         /* auxiliary data to `frealloc' */
	lu_byte currentwhite;
	lu_byte gcstate;  /* state of garbage collector */
	int sweepstrgc;  /* position of sweep in `strt' */
	creg_GCObject *rootgc;  /* list of all collectable objects */
	creg_GCObject **sweepgc;  /* position of sweep in `rootgc' */
	creg_GCObject *gray;  /* list of gray objects */
	creg_GCObject *grayagain;  /* list of objects to be traversed atomically */
	creg_GCObject *weak;  /* list of weak tables (to be cleared) */
	creg_GCObject *tmudata;  /* last element of list of userdata to be GC */
	Mbuffer buff;  /* temporary buffer for string concatenation */
	lu_mem GCthreshold;
	lu_mem totalbytes;  /* number of bytes currently allocated */
	lu_mem estimate;  /* an estimate of number of bytes actually in use */
	lu_mem gcdept;  /* how much GC is `behind schedule' */
	int gcpause;  /* size of pause between successive GCs */
	int gcstepmul;  /* GC `granularity' */
	lua_CFunction panic;  /* to be called in unprotected errors */
	creg_TValue l_registry;
	creg_lua_State *mainthread;
	creg_UpVal uvhead;  /* head of double-linked list of all open upvalues */
	creg_Table *mt[NUM_TAGS];  /* metatables for basic types */
	creg_TString *tmname[TM_N];  /* array with tag-method names */

	//SPRING additions
	lua_Func_fopen  fopen_func;
	lua_Func_popen  popen_func;
	lua_Func_pclose pclose_func;
	lua_Func_system system_func;
	lua_Func_remove remove_func;
	lua_Func_rename rename_func;
	void Serialize(creg::ISerializer* s);
};

// ASSERT_SIZE(global_State)


struct creg_lua_State {
	CR_DECLARE_STRUCT(creg_lua_State)
	creg_CommonHeader;
	lu_byte status;
	StkId top;  /* first free slot in the stack */
	StkId base;  /* base of current function */
	creg_global_State *l_G;
	CallInfo *ci;  /* call info for current function */
	const Instruction *savedpc;  /* `savedpc' of current function */
	StkId stack_last;  /* last free slot in the stack */
	StkId stack;  /* stack base */
	CallInfo *end_ci;  /* points after end of ci array*/
	CallInfo *base_ci;  /* array of CallInfo's */
	int stacksize;
	int size_ci;  /* size of array `base_ci' */
	unsigned short nCcalls;  /* number of nested C calls */
	unsigned short baseCcalls;  /* nested C calls when resuming coroutine */
	lu_byte hookmask;
	lu_byte allowhook;
	int basehookcount;
	int hookcount;
	lua_Hook hook;
	creg_TValue l_gt;  /* table of globals */
	creg_TValue env;  /* temporary place for environments */
	creg_GCObject *openupval;  /* list of open upvalues in this stack */
	creg_GCObject *gclist;
	struct lua_longjmp *errorJmp;  /* current error recover point */
	ptrdiff_t errfunc;  /* current error handling function (stack index) */
	void Serialize(creg::ISerializer* s);
	void PostLoad();
};

// ASSERT_SIZE(lua_State)


union creg_GCObject {
	// GCheader gch;
	creg_TString ts;
	creg_Udata u;
	creg_Closure cl;
	creg_Table h;
	creg_Proto p;
	creg_UpVal uv;
	creg_lua_State th;
};


// Specialization because we have to figure the real class and not
// serialize GCObject* pointers.
namespace creg {
template<>
class ObjectPointerType<creg_GCObject> : public IType
{
public:
	ObjectPointerType() : IType(sizeof(creg_GCObject*)) { }
	void Serialize(ISerializer *s, void *instance) override{
		return;
	}
	std::string GetName() const override {
		return "creg_GCObject*";
	}
};
}


struct creg_LG {
	CR_DECLARE_STRUCT(creg_LG)
	creg_lua_State l;
	creg_global_State g;
};


CR_BIND_POOL(creg_TValue, , allocProtector, freeProtector)
CR_REG_METADATA(creg_TValue, (
	CR_IGNORED(value), //union
	CR_MEMBER(tt),
	CR_SERIALIZER(Serialize)
))


CR_BIND_POOL(creg_Node, , allocProtector, freeProtector)
CR_REG_METADATA(creg_Node, (
	CR_MEMBER(i_val),
	CR_IGNORED(i_key),
	CR_SERIALIZER(Serialize)
))


CR_BIND_POOL(creg_Table, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_Table, (
	CR_COMMON_HEADER(),
	CR_MEMBER(flags),
	CR_MEMBER(lsizenode),
	CR_MEMBER(metatable),
	CR_IGNORED(array), //vector
	CR_IGNORED(node), //vector
	CR_IGNORED(lastfree), //serialized separately
	CR_IGNORED(gclist), //probably unneeded
	CR_MEMBER(sizearray),
	CR_SERIALIZER(Serialize),
	CR_POSTLOAD(PostLoad)
))


CR_BIND_POOL(creg_LocVar, , allocProtector, freeProtector)
CR_REG_METADATA(creg_LocVar, (
	CR_MEMBER(varname),
	CR_MEMBER(startpc),
	CR_MEMBER(endpc)
))


CR_BIND_POOL(creg_Proto, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_Proto, (
	CR_COMMON_HEADER(),
	CR_IGNORED(k), // vector
	CR_IGNORED(code), // vector
	CR_IGNORED(p), // vector
	CR_IGNORED(lineinfo), // vector
	CR_IGNORED(locvars), // vector
	CR_IGNORED(upvalues), // vector
	CR_MEMBER(source),
	CR_MEMBER(sizeupvalues),
	CR_MEMBER(sizek),
	CR_MEMBER(sizecode),
	CR_MEMBER(sizelineinfo),
	CR_MEMBER(sizep),
	CR_MEMBER(sizelocvars),
	CR_MEMBER(linedefined),
	CR_MEMBER(lastlinedefined),
	CR_IGNORED(gclist), //probably unneeded
	CR_MEMBER(nups),
	CR_MEMBER(numparams),
	CR_MEMBER(is_vararg),
	CR_MEMBER(maxstacksize),
	CR_SERIALIZER(Serialize)
))


CR_BIND_POOL(creg_UpVal, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_UpVal, (
	CR_COMMON_HEADER(),
	CR_MEMBER(v),
	CR_IGNORED(u), //union
	CR_SERIALIZER(Serialize)
))


CR_BIND_POOL(creg_TString, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_TString, (
	CR_IGNORED(u), //union
	CR_SERIALIZER(Serialize),
	CR_GETSIZE(GetSize)
))


CR_BIND_POOL(creg_CClosure, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_CClosure, (
	CR_CLOSURE_HEADER(),
	CR_IGNORED(f),
	CR_IGNORED(upvalue),
	CR_SERIALIZER(Serialize),
	CR_GETSIZE(GetSize)
))


CR_BIND_POOL(creg_LClosure, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_LClosure, (
	CR_CLOSURE_HEADER(),
	CR_MEMBER(p),
	CR_IGNORED(upvals),
	CR_SERIALIZER(Serialize),
	CR_GETSIZE(GetSize)
))


CR_BIND_POOL(creg_Udata, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_Udata, (
	CR_IGNORED(u),
	CR_SERIALIZER(Serialize),
	CR_GETSIZE(GetSize)
))



CR_BIND_POOL(creg_stringtable, , allocProtector, freeProtector)
CR_REG_METADATA(creg_stringtable, (
	CR_IGNORED(hash), //vector
	CR_MEMBER(nuse),
	CR_MEMBER(size),
	CR_SERIALIZER(Serialize)
))


CR_BIND_POOL(creg_global_State, , allocProtector, freeProtector)
CR_REG_METADATA(creg_global_State, (
	CR_MEMBER(strt),
	CR_IGNORED(frealloc),
	CR_IGNORED(ud),
	CR_MEMBER(currentwhite),
	CR_MEMBER(gcstate),
	CR_MEMBER(sweepstrgc),
	CR_MEMBER(rootgc),
	CR_IGNORED(sweepgc),
	CR_MEMBER(gray),
	CR_MEMBER(grayagain),
	CR_MEMBER(weak),
	CR_MEMBER(tmudata),
	CR_IGNORED(buff), // this is a temporary buffer, no need to store
	CR_MEMBER(GCthreshold),
	CR_MEMBER(totalbytes),
	CR_MEMBER(estimate),
	CR_MEMBER(gcdept),
	CR_MEMBER(gcpause),
	CR_MEMBER(gcstepmul),
	CR_IGNORED(panic),
	CR_MEMBER(l_registry),
	CR_MEMBER(mainthread),
	CR_MEMBER(uvhead),
	CR_MEMBER(mt),
	CR_MEMBER(tmname),
	CR_IGNORED(fopen_func),
	CR_IGNORED(popen_func),
	CR_IGNORED(pclose_func),
	CR_IGNORED(system_func),
	CR_IGNORED(remove_func),
	CR_IGNORED(rename_func),
	CR_SERIALIZER(Serialize)
))


CR_BIND_POOL(creg_lua_State, , luaContext.alloc, freeProtector)
CR_REG_METADATA(creg_lua_State, (
	CR_COMMON_HEADER(),
	CR_MEMBER(status),
	CR_IGNORED(top),
	CR_IGNORED(base),
	CR_MEMBER(l_G),
	CR_IGNORED(ci),
	CR_IGNORED(savedpc),
	CR_IGNORED(stack_last),
	CR_IGNORED(stack),
	CR_IGNORED(end_ci),
	CR_IGNORED(base_ci),
	CR_MEMBER(stacksize),
	CR_MEMBER(size_ci),
	CR_MEMBER(nCcalls),
	CR_MEMBER(baseCcalls),
	CR_MEMBER(hookmask),
	CR_MEMBER(allowhook),
	CR_MEMBER(basehookcount),
	CR_MEMBER(hookcount),
	CR_IGNORED(hook),
	CR_MEMBER(l_gt),
	CR_IGNORED(env), // temporary
	CR_MEMBER(openupval),
	CR_IGNORED(gclist), //probably unneeded
	CR_IGNORED(errorJmp),
	CR_MEMBER(errfunc),
	CR_SERIALIZER(Serialize),
	CR_POSTLOAD(PostLoad)
))


CR_BIND_POOL(creg_LG, , allocProtector, freeProtector)
CR_REG_METADATA(creg_LG, (
	CR_MEMBER(l),
	CR_MEMBER(g)
))


template<typename T, typename C>
inline void SerializeCVector(creg::ISerializer* s, T** vecPtr, C count)
{
	return;
}

template<typename T>
void SerializePtr(creg::ISerializer* s, T** t) {
	creg::ObjectPointerType<T> opt;
	opt.Serialize(s, t);
}

template<typename T>
void SerializeInstance(creg::ISerializer* s, T* t) {
	s->SerializeObjectInstance(t, t->GetClass());
}

void SerializeLightUserData(creg::ISerializer* s, void **p)
{
	return;
}


void creg_TValue::Serialize(creg::ISerializer* s)
{
	return;
}


void creg_Node::Serialize(creg::ISerializer* s)
{
	return;
}


void creg_Table::Serialize(creg::ISerializer* s)
{
	return;
}

void creg_Table::PostLoad()
{
	return;
}


void creg_Proto::Serialize(creg::ISerializer* s)
{
	return;
}


void creg_UpVal::Serialize(creg::ISerializer* s)
{
	return;
}


void creg_TString::Serialize(creg::ISerializer* s)
{
	return;
}


size_t creg_TString::GetSize()
{
	return 0;
}


void creg_CClosure::Serialize(creg::ISerializer* s)
{
	return;
}


size_t creg_CClosure::GetSize()
{
	return 0;
}


void creg_LClosure::Serialize(creg::ISerializer* s)
{
	return;
}


size_t creg_LClosure::GetSize()
{
	return 0;
}


void creg_Udata::Serialize(creg::ISerializer* s)
{
	return;
}


size_t creg_Udata::GetSize()
{
	return 0;
}


void creg_stringtable::Serialize(creg::ISerializer* s)
{
	return;
}

inline creg_Proto* GetProtoFromCallInfo(CallInfo* ci)
{
	return nullptr;
}

inline bool InstructionInCode(const Instruction* inst, CallInfo* ci)
{
	
	return true;
}

void creg_lua_State::Serialize(creg::ISerializer* s)
{
	return;
}


void creg_lua_State::PostLoad()
{
	return;
}


void creg_global_State::Serialize(creg::ISerializer* s)
{
	return;
}


namespace creg {

void SerializeLuaState(creg::ISerializer* s, lua_State** L)
{
	return;
}

void SerializeLuaThread(creg::ISerializer* s, lua_State** L)
{
	return;
}

void RegisterCFunction(const char* name, lua_CFunction f)
{
	return;
}

constexpr int MAX_REC_DEPTH = 7;

void RecursiveAutoRegisterFunction(const std::string& handle, lua_State* L, int depth);
void RecursiveAutoRegisterTable(const std::string& handle, lua_State* L, int depth);


void RecursiveAutoRegisterFunction(const std::string& handle, lua_State* L, int depth)
{
	return;

}


void RecursiveAutoRegisterTable(const std::string& handle, lua_State* L, int depth)
{
	return;
}

void AutoRegisterCFunctions(const std::string& handle, lua_State* L)
{
return;
}

void UnregisterAllCFunctions() {
	return;
}

void CopyLuaContext(lua_State* L)
{
	return;
}
}
