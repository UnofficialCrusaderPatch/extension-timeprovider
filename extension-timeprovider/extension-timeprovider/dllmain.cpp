
#include "pch.h"
// lua
#include "lua.hpp"

#include "timeProviderInternal.h"

#include <chrono>
#include <deque>
#include <numeric>
#include <algorithm>


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

static std::deque<DWORD> maxTickQueue{};

static void pushMaxTick(const int duration)
{
  if (maxTickQueue.size() >= 100)
  {
    maxTickQueue.pop_back();
  }
  maxTickQueue.push_front(duration);
}

static DWORD getAverageMaxTick()
{
  if (maxTickQueue.size() < 1)
  {
    return 0;
  }
  return std::accumulate(maxTickQueue.begin(), maxTickQueue.end(), 0) / maxTickQueue.size();
}

static std::deque<DWORD> averageFPSQueue{};

static void pushFPS(const int duration)
{
  if (averageFPSQueue.size() >= 100)
  {
    averageFPSQueue.pop_back();
  }
  averageFPSQueue.push_front(duration);
}

static DWORD getAverageFPS()
{
  if (averageFPSQueue.size() < 1)
  {
    return 0;
  }
  return std::accumulate(averageFPSQueue.begin(), averageFPSQueue.end(), 0) / averageFPSQueue.size();
}

static std::deque<DWORD> averageLoopDurationQueue{};

static void pushLoopDuration(const int duration)
{
  if (averageLoopDurationQueue.size() >= 100)
  {
    averageLoopDurationQueue.pop_back();
  }
  averageLoopDurationQueue.push_front(duration);
}

static DWORD getAverageLoopDuration()
{
  if (averageLoopDurationQueue.size() < 1)
  {
    return 0;
  }
  return std::accumulate(averageLoopDurationQueue.begin(), averageLoopDurationQueue.end(), 0) / averageLoopDurationQueue.size();
}

static std::deque<DWORD> averageTickDurationQueue{};

static void pushAverageTickDuration(const int duration)
{
  if (averageTickDurationQueue.size() >= 100)
  {
    averageTickDurationQueue.pop_back();
  }
  averageTickDurationQueue.push_front(duration);
}

static DWORD getAverageTickDuration()
{
  if (averageTickDurationQueue.size() < 1)
  {
    return 0;
  }
  return std::accumulate(averageTickDurationQueue.begin(), averageTickDurationQueue.end(), 0) / averageTickDurationQueue.size();
}

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


DWORD __thiscall FakeGameSynchronyState::detouredDetermineGameTicksToPerform(int currentPlayerSlotID)
{
  // at this point, even the duration of one tick is the one from the last loop
 /* if (lastNumberOfTicks > 0 && *averageTickDurationLastLoop > durationOfOneTick)
  {
    *tickDurationCarry -= (*averageTickDurationLastLoop - durationOfOneTick) * lastNumberOfTicks;
  }*/

  //if (*durationLastLoop)
  //{
  //  lastFPS = 1000000 / *durationLastLoop;
  //}

  // TODO try lowering actual speed computation

  DWORD numberOfTicks{ (*this.*actualDetermineGameTicksToPerform)(currentPlayerSlotID) };

 /* if (*averageTickDurationLastLoop && *durationLastLoop > 16700)
  {
    *tickDurationCarry = 0;
    numberOfTicks = (16700 * timeUsedForGameTicks / *durationLastLoop) / durationOfOneTick;
  }*/

  if (numberOfTicks > 1)
  {
    const DWORD averageMaxTick{ getAverageMaxTick() };
    if (*averageTickDurationLastLoop && *averageTickDurationLastLoop > durationOfOneTick)
    {
      *tickDurationCarry = 0;
      const DWORD altTickRate{ ((numberOfTicks * durationOfOneTick) / (*averageTickDurationLastLoop * 2)) * (*durationLastLoop / timeUsedForGameTicks) };
      
      numberOfTicks = altTickRate < averageMaxTick ? altTickRate : averageMaxTick;
      pushMaxTick(altTickRate);
    }

    if (averageMaxTick)
    {
      if (averageMaxTick < numberOfTicks)
      {
        *tickDurationCarry = 0;
        numberOfTicks = averageMaxTick;
      }
      pushMaxTick(averageMaxTick + 1);
    }

    /*const DWORD averageFPS{ getAverageFPS() };
    if (averageFPS > lastFPS + 3)
    {
      *tickDurationCarry = 0;
      numberOfTicks = numberOfTicks * (lastFPS * 1000000) / (averageFPS * 1000000);
    }*/

    /*if (*averageTickDurationLastLoop && averageLoopDurationQueue.size() && averageLoopDurationQueue.front() < *durationLastLoop)
    {
      DWORD intendedTicks{ averageLoopDurationQueue.front() / *averageTickDurationLastLoop };
      if (intendedTicks < 1)
      {
        intendedTicks = 1;
      }

      if (maxTicks > intendedTicks)
      {
        maxTicks = intendedTicks;
      }
      else
      {
        --maxTicks;
      }

      if (maxTicks < 1)
      {
        maxTicks = 1;
      }
      
      stableRoundCounter = 0;
      *tickDurationCarry = 0;
      numberOfTicks = maxTicks;
    }
    else if (numberOfTicks > maxTicks)
    {
      stableRoundCounter = 0;
      *tickDurationCarry = 0;
      numberOfTicks = maxTicks;
    }
    else if (stableRoundCounter >= 1000 && numberOfTicks <= maxTicks && maxTicks < MAXDWORD)
    {
      ++maxTicks;
    }
    else if (stableRoundCounter < 1000)
    {
      ++stableRoundCounter;
    }*/

    /*if (*durationLastLoop && lastNumberOfTicks)
    {
      const DWORD relationDurationTicks{ *durationLastLoop / lastNumberOfTicks };
      if (bestRelation > relationDurationTicks)
      {
        bestRelation = relationDurationTicks;
        maxStableTicks = lastNumberOfTicks;
      }
      else
      {
        *tickDurationCarry = 0;
        ++bestRelation;
        numberOfTicks = ++maxStableTicks;
      }
    }*/
  }

 /* pushFPS(lastFPS);
  pushAverageTickDuration(*averageTickDurationLastLoop);
  pushLoopDuration(*durationLastLoop);*/
  lastNumberOfTicks = numberOfTicks;
  return numberOfTicks;
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