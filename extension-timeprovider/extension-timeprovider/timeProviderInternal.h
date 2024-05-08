#pragma once

#include "timeProviderHeader.h"

/* exports */

extern "C" __declspec(dllexport) uint64_t __stdcall GetFullNanosecondsTime();
extern "C" __declspec(dllexport) DWORD __stdcall GetNanosecondsTime();
extern "C" __declspec(dllexport) DWORD __stdcall GetMicrosecondsTime();
extern "C" __declspec(dllexport) DWORD __stdcall GetMillisecondsTime();

/* LUA */

extern "C" __declspec(dllexport) int __cdecl lua_GetFullNanosecondsTime(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_GetNanosecondsTime(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_GetMicrosecondsTime(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_GetMillisecondsTime(lua_State * L);
