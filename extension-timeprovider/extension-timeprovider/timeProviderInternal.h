#pragma once

#include "timeProviderHeader.h"
#include "heuristicHelper.h"

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

const int MAX_VANILLA_SPEED{ 90 };

enum class LimiterType : int
{
  VANILLA = 0,
  MAX_ACTIONS = 1,
  FIXED_FLOOR = 2,
  DYNAMIC = 3,
  NO_LIMITER = 4,
};

// set to defaults
struct LoopControlSettings
{
  LimiterType limiterType{ LimiterType::VANILLA };
  int maxNumerOfActionsPerLoop{ 11 }; // vanilla value
  int minMicrosecondsLoopDuration{ 1000000 / 33 }; // to keep 30 frames as good as possible 
};

struct LoopControlValues
{
  bool isVanillaLimiterGamespeed{};
  DWORD timeBeforeGameTicks{};
  DWORD timeAfterGameTicks{};
  DWORD timeUsedForGameTicks{};
  DWORD actualLastAverageTimePerTick{};
  int timeLoopCarry{};
  int tickLoopCarry{};
  bool lastTickLoopFinished{ true };
  int allowedTickTime{};
  HeuristicHelper<DWORD, 11> renderOffsetCollector{};
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

extern "C" __declspec(dllexport) int __cdecl lua_SetLimiterType(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_SetMaxNumberOfActionsPerLoop(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_SetMinFramesPerSecond(lua_State * L);