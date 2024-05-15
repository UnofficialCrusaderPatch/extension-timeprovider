local exports = {}

local function getAddress(aob, errorMsg, modifierFunc)
  local address = core.AOBScan(aob, 0x400000)
  if address == nil then
    log(ERROR, errorMsg)
    error("'timeProvider' can not be initialized.")
  end
  if modifierFunc == nil then
    return address;
  end
  return modifierFunc(address)
end

local function createOffsetForRelativeCall(callAddress, funcAddress)
  return funcAddress - callAddress - 5
end

local function getNumerator1000Addresses()
  local numberOfExpectedAddresses = 6
  local aob = "B8 E8 03 00 00 99"
  local aobSize = 6
  
  local foundAddresses = {}
  local currentMinAddress = 0x400000
  for i = 1, numberOfExpectedAddresses do 
    local foundAddress = core.AOBScan(aob, currentMinAddress) -- throws if not found
    foundAddresses[i] = foundAddress + 1 -- plus 1 to get number address
    currentMinAddress = foundAddress + aobSize
  end

  return foundAddresses
end

exports.enable = function(self, moduleConfig, globalConfig)

  local addrOfGetTime = getAddress(
    "FF ? ? ? ? ? 89 44 BE 48",
    "'timeProvider' was unable to find the address for the 'timeGetTime' function.",
    function(foundAddress) return core.readInteger(foundAddress + 2) end
  )
  
  local addrOfStopwatchStartGetTime = getAddress(
    "FF ? ? ? ? ? 89 46 08 C7 46 04 01 00 00 00",
    "'timeProvider' was unable to find the address for 'timeGetTime' in the Stopwatch 'start'-function."
  )
  
  local addrOfStopwatchStopGetTime = getAddress(
    "FF ? ? ? ? ? 89 46 0C 2B 46 08",
    "'timeProvider' was unable to find the address for 'timeGetTime' in the Stopwatch 'stop'-function."
  )
  
  local addrOfBeforeGameTicksGetTime = getAddress(
    "FF D7 A3 CC 7D FE 01",
    "'timeProvider' was unable to find the address for 'timeGetTime' before the game ticks loop."
  )
  
  local addrOfAfterGameTicksGetTime = getAddress(
    "FF D7 2B ? ? ? ? ? 33 D2",
    "'timeProvider' was unable to find the address for 'timeGetTime' after the game ticks loop."
  )
  
  local numerator1000Addresses = getNumerator1000Addresses()
  
  local addrOfMinus1000Numerator = getAddress(
    "B8 18 FC FF FF 99",
    "'timeProvider' was unable to find the address for the -1000 numerator.",
    function(foundAddress) return foundAddress + 1 end
  )
  
  local addrOfCallToDetermineTickFunc = getAddress(
    "E8 ? ? ? ? B9 ? ? ? ? A3 C0 7D FE 01",
    "'timeProvider' was unable to find the address for the call to the determine tick number function."
  )
  
  local addrOfDetermineTickFunc = getAddress(
    "53 55 56 8B ? ? ? ? ? 33 ED 3B F5",
    "'timeProvider' was unable to find the address of the determine tick number function."
  )
  
  local addrToUseForSavingValuesAndSkippingSlowdowns = getAddress(
    "8B ? ? ? ? ? 3B D9 7E 25",
    "'timeProvider' was unable to find the address to skip slow downs from."
  )
  
  local addrToJumpToSkipSlowdowns = getAddress(
    "3B F9 7E 1C",
    "'timeProvider' was unable to find the address to jump to to skip the games tick slow downs."
  )
  
  local addrOfTickDurationCarry = getAddress(
    "8B ? ? ? ? ? 2B D1 03 FA ",
    "'timeProvider' was unable to find the address of the tick duration carry.",
    function(foundAddress) return core.readInteger(foundAddress + 2) end
  )
  
  local addrOfLastLoopDuration = getAddress(
    "8B ? ? ? ? ? 8B ? ? ? ? ? 2B D1 03 FA",
    "'timeProvider' was unable to find the address of the last game loop duration.",
    function(foundAddress) return core.readInteger(foundAddress + 2) end
  )
  
  local addrOfAverageDurationPerTick = getAddress(
    "F7 ? ? ? ? ? A3 C8 7D FE 01",
    "'timeProvider' was unable to find the address of the average duration per tick.",
    function(foundAddress) return core.readInteger(foundAddress + 2) end
  )
  
  local addrToPlaceTickComputeSkip = getAddress(
    "57 8B ? ? ? ? ? 3B FD 7D 0F",
    "'timeProvider' was unable to find the address to early return from the real determine tick function."
  )
  
  local addrToTimeRelatedGameCoreSubStruct = core.readInteger(addrToPlaceTickComputeSkip + 3)
  
  
  --[[ load module ]]--
  
  local requireTable = require("timeprovider.dll") -- loads the dll in memory and runs luaopen_timeprovider
  
  self.GetFullNanosecondsTime = function(self, ...) return requireTable.lua_GetFullNanosecondsTime(...) end
  self.GetNanosecondsTime = function(self, ...) return requireTable.lua_GetNanosecondsTime(...) end
  self.GetMicrosecondsTime = function(self, ...) return requireTable.lua_GetMicrosecondsTime(...) end
  self.GetMillisecondsTime = function(self, ...) return requireTable.lua_GetMillisecondsTime(...) end
  
  --[[ modify code ]]--
  
  -- replaces the function ptr of timeGetTime with our own version
  core.writeCode(
    addrOfGetTime, -- normal 1.41 address: 0x0059e228
    {requireTable.funcAddress_GetMillisecondsTime}
  )
  
  core.writeCode(
    addrOfStopwatchStartGetTime,
    {
      0xe8, createOffsetForRelativeCall(addrOfStopwatchStartGetTime, requireTable.funcAddress_GetMicrosecondsTime),
      0x90
    }
  )

  core.writeCode(
    addrOfStopwatchStopGetTime,
    {
      0xe8, createOffsetForRelativeCall(addrOfStopwatchStopGetTime, requireTable.funcAddress_GetMicrosecondsTime),
      0x90
    }
  )
  
  core.writeCode(
    addrOfBeforeGameTicksGetTime,
    {
      0xe8, createOffsetForRelativeCall(addrOfBeforeGameTicksGetTime, requireTable.funcAddress_FakeSaveTimeBeforeGameTicks),
      0x90, 0x90
    }
  )

  core.writeCode(
    addrOfAfterGameTicksGetTime,
    {
      0xe8, createOffsetForRelativeCall(addrOfAfterGameTicksGetTime, requireTable.funcAddress_FakeGetTimeUsedForGameTicks),
      0x90, 0x90, 0x90
    }
  )
  
  for _, address in pairs(numerator1000Addresses) do
    core.writeCode(
      address,
      {1000000}
    )
  end
  
  core.writeCode(
    addrOfMinus1000Numerator,
    {-1000000}
  )
  
  core.writeCode(
    addrOfCallToDetermineTickFunc,
    {0xe8, createOffsetForRelativeCall(addrOfCallToDetermineTickFunc, requireTable.funcAddress_DetouredDetermineGameTicksToPerform)}
  )
  
  core.writeCode(
    requireTable.address_ActualDetermineGameTicksToPerform,
    {addrOfDetermineTickFunc}
  )
  
  core.writeCode(
    addrToUseForSavingValuesAndSkippingSlowdowns,
    {
      0x89, 0x0D, requireTable.address_DurationOfOneTickReceiver,
      0xe9, core.itob(addrToJumpToSkipSlowdowns - addrToUseForSavingValuesAndSkippingSlowdowns - 11)
    }
  )
  
  core.writeCode(
    requireTable.address_TickDurationCarry,
    {addrOfTickDurationCarry}
  )
  
  core.writeCode(
    requireTable.address_DurationLastLoop,
    {addrOfLastLoopDuration}
  )
  
  core.writeCode(
    requireTable.address_AverageTickDurationLastLoop,
    {addrOfAverageDurationPerTick}
  )
  
  core.writeCode(
    addrToPlaceTickComputeSkip,
    -- pop registers, set return to 1 and return
    -- 1 indicates, that ticks should be computed
    {
      0x5e,
      0x5d, 
      0xb8, 0x01, 0x00, 0x00, 0x00,
      0x5b,
      0xc2, 0x04, 00
    }
  )
  
  core.writeCode(
    requireTable.address_GameCoreTimeSubStruct,
    {addrToTimeRelatedGameCoreSubStruct}
  )
  
end

exports.disable = function(self, moduleConfig, globalConfig) error("not implemented") end

return exports