
#include "pch.h"
// lua
#include "lua.hpp"

#include "timeProviderInternal.h"

#include <chrono>
#include <deque>
#include <numeric>
#include <algorithm>
#include <vector>


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
static DWORD timeAfterGameTicks{};
static DWORD timeUsedForGameTicks{};

void __stdcall FakeSaveTimeBeforeGameTicks()
{
  timeBeforeGameTicks = GetMicrosecondsTime();
}

DWORD __stdcall FakeGetTimeUsedForGameTicks()
{
  timeAfterGameTicks = GetMicrosecondsTime();
  timeUsedForGameTicks = timeAfterGameTicks - timeBeforeGameTicks;
  return timeUsedForGameTicks;
}


/* make slowdown stable */

template<typename T, size_t _Capacity, bool _RestrictToCapacity = true>
class HeuristicHelper
{
private:
  int currentArrayIndex;
  std::vector<T> values;

public:

  HeuristicHelper()
  {
    clear();
  }

  void clear()
  {
    values.clear();
    values.reserve(_Capacity);

    if constexpr (_RestrictToCapacity)
    {
      currentArrayIndex = 0;
    }
  }

  void pushValue(const int value)
  {
    if constexpr (!_RestrictToCapacity)
    {
      values.push_back(value);
      return;
    }

    if (values.size() < _Capacity)
    {
      values.push_back(value);
    }
    else
    {
      values[currentArrayIndex] = value;
    }
    ++currentArrayIndex;
    if (currentArrayIndex >= _Capacity)
    {
      currentArrayIndex = 0;
    }
  }

  T getAverage()
  {
    if (values.empty())
    {
      return static_cast<T>(0);
    }
    return std::accumulate(values.begin(), values.end(), static_cast<T>(0)) / values.size();
  }

  // source: https://stackoverflow.com/a/39487448
  T getMedian()
  {
    if (values.empty())
    {
      return static_cast<T>(0);
    }

    std::vector<T> tmpVector{ values };
    const size_t n = tmpVector.size() / 2;
    std::nth_element(tmpVector.begin(), tmpVector.begin() + n, tmpVector.end());
    if (tmpVector.size() % 2) {
      return tmpVector[n];
    }
    else
    {
      // even sized vector -> average the two middle values
      auto maxIt{ std::max_element(tmpVector.begin(), tmpVector.begin() + n) };
      return (*maxIt + tmpVector[n]) / 2;
    }
  }

  T getMin()
  {
    const auto res{ std::min(values.begin(), values.end()) };
    return res != values.end() ? *res : 0;
  }

  T getMax()
  {
    const auto res{ std::max(values.begin(), values.end()) };
    return res != values.end() ? *res : 0;
  }
};

static const int MAX_VANILLA_SPEED{ 90 };

// for dev
static LimiterType limiterType{ LimiterType::FIXED_FLOOR };
static int maxNumerOfActionsPerLoop{ 11 }; // vanilla value
static int minMicrosecondsLoopDuration{ 1000000 / 33 }; // to keep 30 frames as good as possible 
static bool isVanillaLimiterGamespeed{};

static FakeGameCoreTimeSubStruct* gameCoreTimeSubStruct{ nullptr };
static int timeCarry{};
static CrusaderStopwatch* gameLoopStopwatch{ nullptr };

static int tickLoopCarry{};
static bool lastLoopFinished{ true };
static int allowedTickTime{};
static HeuristicHelper<DWORD, 11> renderOffsetCollector{};
static DWORD actualLastAverageTimePerTick{};

int __thiscall FakeGameSynchronyState::detouredDetermineGameTicksToPerform(int currentPlayerSlotID)
{
  // setup loop values:

  if (gameCoreTimeSubStruct->gameTicksLastLoop)
  {
    if (limiterType == LimiterType::FIXED_FLOOR)
    {
      if (!lastLoopFinished)
      {
        int lastRenderTime{ static_cast<int>(gameLoopStopwatch->stopTime - timeAfterGameTicks) - tickLoopCarry };
        renderOffsetCollector.pushValue(lastRenderTime < 0 ? 0 : static_cast<int>(lastRenderTime));
      }
      else
      {
        renderOffsetCollector.pushValue(0);
      }
    }

    actualLastAverageTimePerTick = timeUsedForGameTicks / gameCoreTimeSubStruct->performedGameTicksThisLoop;
  }

  // end setup loop values

  // will do other side effects and check if ticks should be performed, non 0 (actual game speed) if yes
  const int currentGameSpeed{ (*this.*actualDetermineGameTicksToPerform)(currentPlayerSlotID) };
  if (!currentGameSpeed)
  {
    return 0;
  }
  isVanillaLimiterGamespeed = currentGameSpeed <= 90;

  const int durationOfOneTick = 1000000 / currentGameSpeed;
  const int relativeTimeCarry{ timeCarry + (static_cast<int>(gameLoopStopwatch->duration) - durationOfOneTick) };
  timeCarry = relativeTimeCarry;

  if (relativeTimeCarry < 0)
  {
    timeCarry = relativeTimeCarry + durationOfOneTick;
    return 0;
  }

  switch (isVanillaLimiterGamespeed ? LimiterType::VANILLA : limiterType)
  {
    // roughly, it is not a perfect copy of the algorithm
    case LimiterType::VANILLA:
      if (durationOfOneTick * 2 <= relativeTimeCarry)
      {
        if (static_cast<DWORD>(durationOfOneTick) < actualLastAverageTimePerTick)
        {
          timeCarry = 0;
          return 2;
        }
        if (static_cast<DWORD>((durationOfOneTick * 7) / 10) < actualLastAverageTimePerTick)
        {
          timeCarry = 0;
          return 3;
        }
      }
      [[fallthrough]];  // intended, since next also last step of vanilla
    case LimiterType::MAX_ACTIONS:
      if (durationOfOneTick * (maxNumerOfActionsPerLoop - 1) < relativeTimeCarry)
      {
        timeCarry = 0;
        return maxNumerOfActionsPerLoop;
      }
      break;
    case LimiterType::FIXED_FLOOR:
      allowedTickTime = minMicrosecondsLoopDuration - renderOffsetCollector.getAverage();
      break;
  }

  if (durationOfOneTick < relativeTimeCarry) // means more then one tick possible
  {
    timeCarry = relativeTimeCarry - (relativeTimeCarry / durationOfOneTick) * durationOfOneTick;
    return relativeTimeCarry / durationOfOneTick + 1;
  }
  return 1;
}

BOOL FakeLoopControl()
{
  const bool loopFinished{ ++gameCoreTimeSubStruct->performedGameTicksThisLoop >= gameCoreTimeSubStruct->gameTicksThisLoop };

  if (limiterType != LimiterType::FIXED_FLOOR || isVanillaLimiterGamespeed)
  {
    lastLoopFinished = loopFinished; // needed to handle vanilla / limiter switching case
    return loopFinished;
  }

  const int lastDuration{ gameLoopStopwatch->duration };
  if (!lastDuration)
  {
    return loopFinished; // should only happen once
  }

  /*
    Uncapped does not work anymore.
    The games loop before was actions based, which in case of uncapped frames lead to as many actions as needed.
    Normally, this would (together with the increasing duration) escalate, but Firefly added some limiters.
    Depending on the situation, 2, 3 or 11 actions were max per frame, so while uncapped would slow down, it could
    not escalate like situations here, the games frames would just slow down a bit and potentially even recover.
    Now, due to it being time based with the change, the only possible thing in this situation will be frame or no frame,
    making the positive carry useless (like when capped fps reached its limit) and the game slower.
    In short, with the change, the focus just went to fps.

    Similar, if the loop becomes to expensive in total (thereddaemon: 1440p zoomed out on my laptop is too much and
    it went under 60fps), the limit will still apply, and unlike before, the game will slow down to one action per loop.
    Previously, it would try to compensate with up to 11 ticks.

    It is not known which effect this would have in multiplayer.
    If not overdone, the old catch up logic would likely prevent any action desync, but I have no idea how if would behave if one player
    really slowed down... worst case, constant desync.
    If this is the case, the simplest logic here would be to skip the loop duration mechanic in a MP case, which might be ok, since
    then the stability of the connection would have more importance then the frames.

    Important missing pieces:
    - Is any heuristic possible to make the frame limit dynamic?
      - Even if possible, this would not help with the multiplayer issue, since it would optimize for frames first.
    - Handling uncapped frames? How would one do it? Maybe another logic similar to the original games one to handle this cases?
    - How would multiplayer behave when one of the players is on constant slow down? How would one fix this?

    Note:
    - Loop logic could also just apply at a certain speed level, which would allow to avoid the multiplayer condition.
    - The since the bltAndFlipDuration is used, the framerate seems more unstable. Maybe the actual flip needs to be used?
      - looks like it
  */


  if (allowedTickTime <= 0)
  {
    tickLoopCarry = 0;
    lastLoopFinished = false;
    return TRUE;
  }
  const DWORD positiveAllowedTickTime{ static_cast<DWORD>(allowedTickTime) };

  // break early if the loop takes too long
  const DWORD timeSpendOnTicks{ GetMicrosecondsTime() - timeBeforeGameTicks };
  if (timeSpendOnTicks > positiveAllowedTickTime)
  {
    const DWORD tickOvertime{ timeSpendOnTicks - positiveAllowedTickTime };
    // heuristic: focus to reduce time, not to add
    tickLoopCarry = -static_cast<int>(std::min({ tickOvertime, actualLastAverageTimePerTick, positiveAllowedTickTime }));
    lastLoopFinished = false;
    return TRUE;
  }

  if (!loopFinished)
  {
    const DWORD expectedTimeSpendWithNextTick{ timeSpendOnTicks + actualLastAverageTimePerTick };
    if (expectedTimeSpendWithNextTick > positiveAllowedTickTime)
    {
      // heuristic: focus to reduce time
      tickLoopCarry = static_cast<int>(expectedTimeSpendWithNextTick - positiveAllowedTickTime);
      lastLoopFinished = false;
      return TRUE;
    }
  }
  else
  {
    tickLoopCarry = 0; // actually not really true
    lastLoopFinished = true;
  }
  // false will continue with the next tick, true break the loop
  return loopFinished;
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

  return 1;
}