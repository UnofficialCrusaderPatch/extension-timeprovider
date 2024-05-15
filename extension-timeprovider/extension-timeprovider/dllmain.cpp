
#include "pch.h"
// lua
#include "lua.hpp"

#include "timeProviderInternal.h"

#include <chrono>
#include <deque>
#include <numeric>
#include <algorithm>
#include <array>


/* export C */

extern "C" __declspec(dllexport) uint64_t __stdcall GetFullNanosecondsTime()
{
  // source: https://stackoverflow.com/a/63506393
  const std::chrono::time_point currentDuration{ std::chrono::steady_clock::now() };
  const std::chrono::time_point currentDurationNano{ std::chrono::time_point_cast<std::chrono::nanoseconds>(currentDuration) };

  // this will only overflow in 200+ years, so we should be fine, but in hope to make it theoretical work, we cast it to uint
  return static_cast<uint64_t>(currentDurationNano.time_since_epoch().count());
}

extern "C" __declspec(dllexport) DWORD __stdcall GetNanosecondsTime()
{
  return static_cast<DWORD>(GetFullNanosecondsTime());
}

extern "C" __declspec(dllexport) DWORD __stdcall GetMicrosecondsTime()
{
  return static_cast<DWORD>(GetFullNanosecondsTime() / 1000);
}

extern "C" __declspec(dllexport) DWORD __stdcall GetMillisecondsTime()
{
  return static_cast<DWORD>(GetFullNanosecondsTime() / 1000000);
}


/* export LUA */

extern "C" __declspec(dllexport) int __cdecl lua_GetFullNanosecondsTime(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 0)
  {
    luaL_error(L, "[timeprovider]: lua_GetFullNanosecondsTime: Invalid number of args.");
  }

  lua_pushinteger(L, GetFullNanosecondsTime());
  return 1;
}

extern "C" __declspec(dllexport) int __cdecl lua_GetNanosecondsTime(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 0)
  {
    luaL_error(L, "[timeprovider]: lua_GetNanosecondsTime: Invalid number of args.");
  }

  lua_pushinteger(L, GetNanosecondsTime());
  return 1;
}

extern "C" __declspec(dllexport) int __cdecl lua_GetMicrosecondsTime(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 0)
  {
    luaL_error(L, "[timeprovider]: lua_GetMicrosecondsTime: Invalid number of args.");
  }

  lua_pushinteger(L, GetMicrosecondsTime());
  return 1;
}

extern "C" __declspec(dllexport) int __cdecl lua_GetMillisecondsTime(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 0)
  {
    luaL_error(L, "[timeprovider]: lua_GetMillisecondsTime: Invalid number of args.");
  }

  lua_pushinteger(L, GetMillisecondsTime());
  return 1;
}


/* time resolution increase helpers */

static FakeGameCoreTimeSubStruct* gameCoreTimeSubStruct { nullptr };
static DWORD timeBeforeGameTicks{};
static DWORD timeUsedForGameTicks{};

static void __stdcall FakeSaveTimeBeforeGameTicks()
{
  timeBeforeGameTicks = GetMicrosecondsTime();
}

static DWORD __stdcall FakeGetTimeUsedForGameTicks()
{
  timeUsedForGameTicks = GetMicrosecondsTime() - timeBeforeGameTicks;
  return timeUsedForGameTicks;
}


/* make slowdown stable */

template<typename T, size_t _Size>
class HeuristicHelper
{
private:
  int currentArrayIndex{};
  std::array<T, _Size> values{};

public:

  void pushValue(const int value)
  {
    values[currentArrayIndex++] = value;
    if (currentArrayIndex >= values.size())
    {
      currentArrayIndex = 0;
    }
  }

  T getAverage()
  {
    if (values.empty())
    {
      return 0.0;
    }
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
  }

  // source: https://stackoverflow.com/a/39487448
  T getMedian()
  {
    if (values.empty())
    {
      return 0.0;
    }

    std::array<T, _Size> tmpArray{ values };
    const size_t n = tmpArray.size() / 2;
    std::nth_element(tmpArray.begin(), tmpArray.begin() + n, tmpArray.end());
    if (tmpArray.size() % 2) {
      return tmpArray[n];
    }
    else
    {
      // even sized vector -> average the two middle values
      auto maxIt{ std::max_element(tmpArray.begin(), tmpArray.begin() + n) };
      return (*maxIt + tmpArray[n]) / 2;
    }
  }
};

static DWORD durationOfOneTick{};
static DWORD lastNumberOfTicks{};

static DWORD* durationLastLoop{ nullptr };
static int* tickDurationCarry{ nullptr };
static DWORD* averageTickDurationLastLoop{ nullptr };

static DWORD maxTicks = { MAXDWORD };
static DWORD stableCounter = {};
static DWORD instableCounter = {};

static DWORD bestRelation{ MAXDWORD };
static DWORD maxStableTicks{ 1 };

static DWORD lastFPS{};


static DWORD overheadCarry{};

DWORD __thiscall FakeGameSynchronyState::detouredDetermineGameTicksToPerform(int currentPlayerSlotID)
{
  // will do other side effects and check if ticks should be performed, 1 if yes
  // 1 will not mean that it needs to be more then 0!
  bool computeTicks{ static_cast<bool>((*this.*actualDetermineGameTicksToPerform)(currentPlayerSlotID)) };

  // TOD0: start replicating the game logic, then clean the cod here, then test some stuff with full control again
  // if nothing helps, take control of the executing tick loop, to break early if needed!

  return computeTicks;
}


// lua module load
extern "C" __declspec(dllexport) int __cdecl luaopen_timeprovider(lua_State * L)
{
  lua_newtable(L); // push a new table on the stack

  // address
  lua_pushinteger(L, (DWORD) &FakeGameSynchronyState::actualDetermineGameTicksToPerform);
  lua_setfield(L, -2, "address_ActualDetermineGameTicksToPerform");
  lua_pushinteger(L, (DWORD) &durationOfOneTick);
  lua_setfield(L, -2, "address_DurationOfOneTickReceiver");
  lua_pushinteger(L, (DWORD) &tickDurationCarry);
  lua_setfield(L, -2, "address_TickDurationCarry");
  lua_pushinteger(L, (DWORD) &durationLastLoop);
  lua_setfield(L, -2, "address_DurationLastLoop");
  lua_pushinteger(L, (DWORD) &averageTickDurationLastLoop);
  lua_setfield(L, -2, "address_AverageTickDurationLastLoop");
  lua_pushinteger(L, (DWORD) &gameCoreTimeSubStruct);
  lua_setfield(L, -2, "address_GameCoreTimeSubStruct");

  // simple replace
  auto memberFuncPtr{ &FakeGameSynchronyState::detouredDetermineGameTicksToPerform };
  lua_pushinteger(L, *(DWORD*) &memberFuncPtr);
  lua_setfield(L, -2, "funcAddress_DetouredDetermineGameTicksToPerform");
  lua_pushinteger(L, (DWORD) GetMillisecondsTime);
  lua_setfield(L, -2, "funcAddress_GetMillisecondsTime");

  // call addresses to use
  lua_pushinteger(L, (DWORD) GetMicrosecondsTime);
  lua_setfield(L, -2, "funcAddress_GetMicrosecondsTime");
  lua_pushinteger(L, (DWORD) FakeSaveTimeBeforeGameTicks);
  lua_setfield(L, -2, "funcAddress_FakeSaveTimeBeforeGameTicks");
  lua_pushinteger(L, (DWORD) FakeGetTimeUsedForGameTicks);
  lua_setfield(L, -2, "funcAddress_FakeGetTimeUsedForGameTicks");

  // return lua funcs

  lua_pushcfunction(L, lua_GetFullNanosecondsTime);
  lua_setfield(L, -2, "lua_GetFullNanosecondsTime");
  lua_pushcfunction(L, lua_GetNanosecondsTime);
  lua_setfield(L, -2, "lua_GetNanosecondsTime");
  lua_pushcfunction(L, lua_GetMicrosecondsTime);
  lua_setfield(L, -2, "lua_GetMicrosecondsTime");
  lua_pushcfunction(L, lua_GetMillisecondsTime);
  lua_setfield(L, -2, "lua_GetMillisecondsTime");

  return 1;
}