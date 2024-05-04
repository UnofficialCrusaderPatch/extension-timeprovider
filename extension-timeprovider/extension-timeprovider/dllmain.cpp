
#include "pch.h"
// lua
#include "lua.hpp"

#include "timeprovider.h"

#include <chrono>


extern "C" __declspec(dllexport) DWORD __stdcall GetTime()
{
  // source: https://stackoverflow.com/a/63506393
  const std::chrono::time_point currentDuration{ std::chrono::steady_clock::now() };
  const std::chrono::time_point currentDurationNano{ std::chrono::time_point_cast<std::chrono::nanoseconds>(currentDuration) };

  // this will only overflow in 200+ years, so we should be fine, but in hope to make it theoretical work, we cast it to uint
  const uint64_t unsignedTime{ static_cast<uint64_t>(currentDurationNano.time_since_epoch().count()) };
  return static_cast<DWORD>(unsignedTime / 1000000); // TODO: make configurable
}



// lua module load
extern "C" __declspec(dllexport) int __cdecl luaopen_timeprovider(lua_State * L)
{
  lua_newtable(L); // push a new table on the stack

  // simple replace
  lua_pushinteger(L, (DWORD) GetTime);
  lua_setfield(L, -2, "funcAddress_GetTime");

  return 1;
}