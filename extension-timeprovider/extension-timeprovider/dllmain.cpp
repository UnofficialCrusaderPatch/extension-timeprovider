
#include "pch.h"
// lua
#include "lua.hpp"

#include "timeProviderInternal.h"

#include <chrono>
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


/* time resolution increase helpers and loop controls */

static LoopControlSettings loopControlSettings;
static LoopControlValues loopControlValues;

static FakeGameCoreTimeSubStruct* gameCoreTimeSubStruct{ nullptr };
static CrusaderStopwatch* gameLoopStopwatch{ nullptr };

void __stdcall FakeSaveTimeBeforeGameTicks()
{
  loopControlValues.timeBeforeGameTicks = GetMicrosecondsTime();
}

DWORD __stdcall FakeGetTimeUsedForGameTicks()
{
  loopControlValues.timeAfterGameTicks = GetMicrosecondsTime();
  loopControlValues.timeUsedForGameTicks = loopControlValues.timeAfterGameTicks - loopControlValues.timeBeforeGameTicks;
  return loopControlValues.timeUsedForGameTicks;
}

int __thiscall FakeGameSynchronyState::detouredDetermineGameTicksToPerform(int currentPlayerSlotID)
{
  // setup loop values:

  if (gameCoreTimeSubStruct->gameTicksLastLoop)
  {
    if (loopControlSettings.limiterType == LimiterType::FIXED_FLOOR)
    {
      if (!loopControlValues.lastTickLoopFinished)
      {
        int lastRenderTime{ static_cast<int>(gameLoopStopwatch->stopTime - loopControlValues.timeAfterGameTicks) - loopControlValues.tickLoopCarry };
        loopControlValues.renderOffsetCollector.pushValue(lastRenderTime < 0 ? 0 : static_cast<int>(lastRenderTime));
      }
      else
      {
        loopControlValues.renderOffsetCollector.pushValue(0);
      }
    }

    loopControlValues.actualLastAverageTimePerTick = loopControlValues.timeUsedForGameTicks / gameCoreTimeSubStruct->performedGameTicksThisLoop;
  }

  // end setup loop values

  // will do other side effects and check if ticks should be performed, non 0 (actual game speed) if yes
  const int currentGameSpeed{ (*this.*actualDetermineGameTicksToPerform)(currentPlayerSlotID) };
  if (!currentGameSpeed)
  {
    return 0;
  }
  loopControlValues.isVanillaLimiterGamespeed = currentGameSpeed <= 90;

  const int durationOfOneTick = 1000000 / currentGameSpeed;
  const int relativeTimeCarry{ loopControlValues.timeLoopCarry + (static_cast<int>(gameLoopStopwatch->duration) - durationOfOneTick) };
  loopControlValues.timeLoopCarry = relativeTimeCarry;

  if (relativeTimeCarry < 0)
  {
    loopControlValues.timeLoopCarry = relativeTimeCarry + durationOfOneTick;
    return 0;
  }

  switch (loopControlValues.isVanillaLimiterGamespeed ? LimiterType::VANILLA : loopControlSettings.limiterType)
  {
    // roughly, it is not a perfect copy of the algorithm
    case LimiterType::VANILLA:
      if (durationOfOneTick * 2 <= relativeTimeCarry)
      {
        if (static_cast<DWORD>(durationOfOneTick) < loopControlValues.actualLastAverageTimePerTick)
        {
          loopControlValues.timeLoopCarry = 0;
          return 2;
        }
        if (static_cast<DWORD>((durationOfOneTick * 7) / 10) < loopControlValues.actualLastAverageTimePerTick)
        {
          loopControlValues.timeLoopCarry = 0;
          return 3;
        }
      }
      [[fallthrough]];  // intended, since next also last step of vanilla
    case LimiterType::MAX_ACTIONS:
      if (durationOfOneTick * (loopControlSettings.maxNumerOfActionsPerLoop - 1) < relativeTimeCarry)
      {
        loopControlValues.timeLoopCarry = 0;
        return loopControlSettings.maxNumerOfActionsPerLoop;
      }
      break;
    case LimiterType::FIXED_FLOOR:
      loopControlValues.allowedTickTime = loopControlSettings.minMicrosecondsLoopDuration - loopControlValues.renderOffsetCollector.getAverage();
      break;
  }

  if (durationOfOneTick < relativeTimeCarry) // means more then one tick possible
  {
    loopControlValues.timeLoopCarry = relativeTimeCarry - (relativeTimeCarry / durationOfOneTick) * durationOfOneTick;
    return relativeTimeCarry / durationOfOneTick + 1;
  }
  return 1;
}

BOOL FakeLoopControl()
{
  const bool loopFinished{ ++gameCoreTimeSubStruct->performedGameTicksThisLoop >= gameCoreTimeSubStruct->gameTicksThisLoop };

  if (loopControlSettings.limiterType != LimiterType::FIXED_FLOOR || loopControlValues.isVanillaLimiterGamespeed)
  {
    loopControlValues.lastTickLoopFinished = loopFinished; // needed to handle vanilla / limiter switching case
    return loopFinished;
  }

  const int lastDuration{ gameLoopStopwatch->duration };
  if (!lastDuration)
  {
    return loopFinished; // should only happen once
  }

  if (loopControlValues.allowedTickTime <= 0)
  {
    loopControlValues.tickLoopCarry = 0;
    loopControlValues.lastTickLoopFinished = false;
    return TRUE;
  }
  const DWORD positiveAllowedTickTime{ static_cast<DWORD>(loopControlValues.allowedTickTime) };

  // break early if the loop takes too long
  const DWORD timeSpendOnTicks{ GetMicrosecondsTime() - loopControlValues.timeBeforeGameTicks };
  if (timeSpendOnTicks > positiveAllowedTickTime)
  {
    const DWORD tickOvertime{ timeSpendOnTicks - positiveAllowedTickTime };
    // heuristic: focus to reduce time, not to add
    loopControlValues.tickLoopCarry = -static_cast<int>(std::min({ tickOvertime, loopControlValues.actualLastAverageTimePerTick, positiveAllowedTickTime }));
    loopControlValues.lastTickLoopFinished = false;
    return TRUE;
  }

  if (!loopFinished)
  {
    const DWORD expectedTimeSpendWithNextTick{ timeSpendOnTicks + loopControlValues.actualLastAverageTimePerTick };
    if (expectedTimeSpendWithNextTick > positiveAllowedTickTime)
    {
      // heuristic: focus to reduce time
      loopControlValues.tickLoopCarry = static_cast<int>(expectedTimeSpendWithNextTick - positiveAllowedTickTime);
      loopControlValues.lastTickLoopFinished = false;
      return TRUE;
    }
  }
  else
  {
    loopControlValues.tickLoopCarry = 0; // actually not really true
    loopControlValues.lastTickLoopFinished = true;
  }
  // false will continue with the next tick, true break the loop
  return loopFinished;
}


/* allow setting the control values */

extern "C" __declspec(dllexport) int __cdecl lua_SetLimiterType(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 1)
  {
    luaL_error(L, "[timeprovider]: lua_SetLimiterType: Invalid number of args.");
  }

  if (!lua_isinteger(L, 1))
  {
    luaL_error(L, "[timeprovider]: lua_SetLimiterType: Wrong input fields.");
  }

  const int limiterValue{ static_cast<int>(lua_tointeger(L, 1)) };
  if (limiterValue < 0 || limiterValue > 4)
  {
    luaL_error(L, "[timeprovider]: lua_SetLimiterType: Invalid limiter type value.");
  }
  loopControlSettings.limiterType = static_cast<LimiterType>(limiterValue);
  return 0;
}

extern "C" __declspec(dllexport) int __cdecl lua_SetMaxNumberOfActionsPerLoop(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 1)
  {
    luaL_error(L, "[timeprovider]: lua_SetMaxNumberOfActionsPerLoop: Invalid number of args.");
  }

  if (!lua_isinteger(L, 1))
  {
    luaL_error(L, "[timeprovider]: lua_SetMaxNumberOfActionsPerLoop: Wrong input fields.");
  }

  const int maxNumberOfActionsValue{ static_cast<int>(lua_tointeger(L, 1)) };
  if (maxNumberOfActionsValue < 1)
  {
    luaL_error(L, "[timeprovider]: lua_SetMaxNumberOfActionsPerLoop: Value smaller then 1.");
  }
  loopControlSettings.maxNumerOfActionsPerLoop = maxNumberOfActionsValue;
  return 0;
}

extern "C" __declspec(dllexport) int __cdecl lua_SetMinFramesPerSecond(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 1)
  {
    luaL_error(L, "[timeprovider]: lua_SetMinFramesPerSecond: Invalid number of args.");
  }

  if (!lua_isinteger(L, 1))
  {
    luaL_error(L, "[timeprovider]: lua_SetMinFramesPerSecond: Wrong input fields.");
  }

  const int minFramesPerSecond{ static_cast<int>(lua_tointeger(L, 1)) };
  if (minFramesPerSecond < 1)
  {
    luaL_error(L, "[timeprovider]: lua_SetMinFramesPerSecond: Value smaller then 1.");
  }
  loopControlSettings.minMicrosecondsLoopDuration = 1000000 / minFramesPerSecond;
  return 0;
}


// lua module load
extern "C" __declspec(dllexport) int __cdecl luaopen_timeprovider(lua_State * L)
{
  lua_newtable(L); // push a new table on the stack

  // address
  lua_pushinteger(L, (DWORD) &FakeGameSynchronyState::actualDetermineGameTicksToPerform);
  lua_setfield(L, -2, "address_ActualDetermineGameTicksToPerform");
  lua_pushinteger(L, (DWORD) &gameLoopStopwatch);
  lua_setfield(L, -2, "address_GameLoopStopwatch");
  lua_pushinteger(L, (DWORD) &gameCoreTimeSubStruct);
  lua_setfield(L, -2, "address_GameCoreTimeSubStruct");

  // simple replace
  auto detouredDetermineGameTicksToPerform{ &FakeGameSynchronyState::detouredDetermineGameTicksToPerform };
  lua_pushinteger(L, *(DWORD*) &detouredDetermineGameTicksToPerform);
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
  lua_pushinteger(L, (DWORD) FakeLoopControl);
  lua_setfield(L, -2, "funcAddress_FakeLoopControl");

  // return lua funcs

  lua_pushcfunction(L, lua_GetFullNanosecondsTime);
  lua_setfield(L, -2, "lua_GetFullNanosecondsTime");
  lua_pushcfunction(L, lua_GetNanosecondsTime);
  lua_setfield(L, -2, "lua_GetNanosecondsTime");
  lua_pushcfunction(L, lua_GetMicrosecondsTime);
  lua_setfield(L, -2, "lua_GetMicrosecondsTime");
  lua_pushcfunction(L, lua_GetMillisecondsTime);
  lua_setfield(L, -2, "lua_GetMillisecondsTime");

  lua_pushcfunction(L, lua_SetLimiterType);
  lua_setfield(L, -2, "lua_SetLimiterType");
  lua_pushcfunction(L, lua_SetMaxNumberOfActionsPerLoop);
  lua_setfield(L, -2, "lua_SetMaxNumberOfActionsPerLoop");
  lua_pushcfunction(L, lua_SetMinFramesPerSecond);
  lua_setfield(L, -2, "lua_SetMinFramesPerSecond");

  return 1;
}
