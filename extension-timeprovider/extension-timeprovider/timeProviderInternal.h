#pragma once

#include "timeProviderHeader.h"

/* objects */

struct FakeGameSynchronyState
{
  using ActualDetermineGameTicksToPerform = DWORD (FakeGameSynchronyState::*)(int currentPlayerSlotID);

  inline static ActualDetermineGameTicksToPerform actualDetermineGameTicksToPerform{ nullptr };

  // funcs

  DWORD __thiscall detouredDetermineGameTicksToPerform(int currentPlayerSlotID);
};

struct FakeGameCoreTimeSubStruct
{
  DWORD gameSpeedMultiplicator; // will not be used
  DWORD performedGameTicksThisLoop;
  DWORD gameTicksThisLoop;
  DWORD gameTicksLastLoop;
  DWORD averageTimePerGameTick;
  DWORD timeBeforeRunningGameTicksThisLoop;
  DWORD other[2]; // not interesting for here
  DWORD gameSpeed;
};

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
