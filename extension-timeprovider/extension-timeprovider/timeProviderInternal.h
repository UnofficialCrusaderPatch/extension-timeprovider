#pragma once

#include "timeProviderHeader.h"

/* classes and internal functions */

struct FakeGameSynchronyState
{
  using ActualDetermineGameTicksToPerform = int (FakeGameSynchronyState::*)(int currentPlayerSlotID);

  inline static ActualDetermineGameTicksToPerform actualDetermineGameTicksToPerform{ nullptr };

  // funcs

  int __thiscall detouredDetermineGameTicksToPerform(int currentPlayerSlotID);
};

struct FakeGameCoreTimeSubStruct
{
  int gameSpeedMultiplicator; // will not be used
  int performedGameTicksThisLoop;
  int gameTicksThisLoop;
  int gameTicksLastLoop;
  int averageTimePerGameTick;
  int timeBeforeRunningGameTicksThisLoop;
  int other[2]; // not interesting for here
  int singlePlayerGameSpeed;
};

void __stdcall FakeSaveTimeBeforeGameTicks();
DWORD __stdcall FakeGetTimeUsedForGameTicks();
BOOL FakeLoopControl();

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
