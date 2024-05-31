#pragma once

#include "timeProviderHeader.h"

/* game classes and internal functions */

struct alignas(alignof(int)) CrusaderStopwatch
{
  int duration;
  bool running;
  DWORD startTime;
  DWORD stopTime;
};

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
void __stdcall FakeSaveTimeBeforeGameTicks();

/* other classes and internal functions */

enum class LimiterType : int
{
  VANILLA = 0,
  MAX_ACTIONS = 1,
  FIXED_FLOOR = 2,
  DYNAMIC = 3,
  NO_LIMITER = 4,
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
