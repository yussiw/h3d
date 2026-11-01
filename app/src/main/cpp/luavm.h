#pragma once
// Ponte entre Lua 5.4 e as funcoes da engine (as mesmas usadas pelo interpretador Haxe).
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include "script.h"

class LuaVM {
public:
    lua_State* L = nullptr;

    ~LuaVM() { close(); }
    void close() { if (L) { lua_close(L); L = nullptr; } }

    // Registra cada funcao nativa como global do Lua (nomes com '.' como Math.sin sao ignorados:
    // use a biblioteca math do proprio Lua).
    bool init(std::map<std::string, hx::NativeFn>& natives) {
        L = luaL_newstate();
        if (!L) return false;
        struct Lib { const char* name; lua_CFunction fn; };
        const Lib libs[] = {{"_G", luaopen_base}, {LUA_TABLIBNAME, luaopen_table}, {LUA_STRLIBNAME, luaopen_string},
                            {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine}};
        for (const Lib& l : libs) { luaL_requiref(L, l.name, l.fn, 1); lua_pop(L, 1); }
        for (auto& kv : natives) {
            if (kv.first.find('.') != std::string::npos) continue;
            lua_pushlightuserdata(L, &kv.second);
            lua_pushcclosure(L, thunk, 1);
            lua_setglobal(L, kv.first.c_str());
        }
        return true;
    }

    bool load(const std::string& src, const std::string& name, std::string& err) {
        if (!L) return false;
        int r = luaL_loadbuffer(L, src.data(), src.size(), ("@" + name).c_str());
        if (r == LUA_OK) { armHook(); r = lua_pcall(L, 0, 0, 0); }
        if (r != LUA_OK) { takeError(err); return false; }
        return true;
    }

    // chama uma funcao global (se existir) com argumentos numericos
    bool call(const std::string& fn, const hx::Args& args, std::string& err) {
        if (!L) return true;
        lua_getglobal(L, fn.c_str());
        if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return true; }
        for (const hx::Value& a : args) pushValue(L, a);
        armHook();
        if (lua_pcall(L, (int)args.size(), 0, 0) != LUA_OK) { takeError(err); return false; }
        return true;
    }

    // so verifica a sintaxe
    static bool check(const std::string& src, const std::string& name, std::string& err) {
        lua_State* T = luaL_newstate();
        if (!T) return false;
        int r = luaL_loadbuffer(T, src.data(), src.size(), ("@" + name).c_str());
        bool ok = r == LUA_OK;
        if (!ok) { const char* m = lua_tostring(T, -1); err = m ? m : "erro de sintaxe"; }
        lua_close(T);
        return ok;
    }

private:
    static void hook(lua_State* S, lua_Debug*) {
        luaL_error(S, "o script demorou demais (loop infinito?)");
    }
    void armHook() { lua_sethook(L, hook, LUA_MASKCOUNT, 5000000); }
    void takeError(std::string& err) {
        const char* m = lua_tostring(L, -1);
        err = m ? m : "erro desconhecido";
        lua_pop(L, 1);
    }

    static hx::Value toHx(lua_State* S, int i, int depth = 0) {
        switch (lua_type(S, i)) {
            case LUA_TTABLE: {
                if (depth > 4) return hx::Value();
                int abs = lua_absindex(S, i);
                hx::Value o = hx::Value::obj();
                lua_pushnil(S);
                while (lua_next(S, abs) != 0) {
                    if (lua_type(S, -2) == LUA_TSTRING) (*o.o)[lua_tostring(S, -2)] = toHx(S, -1, depth + 1);
                    lua_pop(S, 1);
                }
                return o;
            }
            case LUA_TBOOLEAN: return hx::Value::boolean(lua_toboolean(S, i) != 0);
            case LUA_TNUMBER: return hx::Value::num((double)lua_tonumber(S, i));
            case LUA_TSTRING: return hx::Value::str(lua_tostring(S, i));
            default: return hx::Value();
        }
    }

    static void pushValue(lua_State* S, const hx::Value& v) {
        switch (v.t) {
            case hx::Value::Null: lua_pushnil(S); break;
            case hx::Value::Bool: lua_pushboolean(S, v.b ? 1 : 0); break;
            case hx::Value::Str: lua_pushlstring(S, v.s.data(), v.s.size()); break;
            case hx::Value::Num:
                if (std::isfinite(v.n) && v.n == std::floor(v.n) && std::fabs(v.n) < 1e15) lua_pushinteger(S, (lua_Integer)v.n);
                else lua_pushnumber(S, v.n);
                break;
            case hx::Value::Arr:
                lua_createtable(S, (int)v.a->size(), 0);
                for (size_t k = 0; k < v.a->size(); k++) { pushValue(S, (*v.a)[k]); lua_rawseti(S, -2, (lua_Integer)k + 1); }
                break;
            case hx::Value::Obj:
                lua_createtable(S, 0, (int)v.o->size());
                for (auto& kv : *v.o) { pushValue(S, kv.second); lua_setfield(S, -2, kv.first.c_str()); }
                break;
        }
    }

    static int thunk(lua_State* S) {
        hx::NativeFn* f = (hx::NativeFn*)lua_touserdata(S, lua_upvalueindex(1));
        int n = lua_gettop(S);
        hx::Args args;
        args.reserve((size_t)n);
        for (int i = 1; i <= n; i++) args.push_back(toHx(S, i));
        hx::Value r = (*f)(args);
        pushValue(S, r);
        return 1;
    }
};
