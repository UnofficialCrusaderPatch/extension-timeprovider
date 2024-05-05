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

exports.enable = function(self, moduleConfig, globalConfig)

  local addrOfGetTime = getAddress(
    "FF ? ? ? ? ? 89 44 BE 48",
    "'timeProvider' was unable to find the address for the 'timeGetTime' function.",
    function(foundAddress) return core.readInteger(foundAddress + 2) end
  )

  --[[ load module ]]--
  
  local requireTable = require("timeprovider.dll") -- loads the dll in memory and runs luaopen_timeprovider
  
  self.GetFullNanosecondsTime = function(self, ...) return requireTable.lua_GetFullNanosecondsTime(...) end
  self.GetNanosecondsTime = function(self, ...) return requireTable.lua_GetNanosecondsTime(...) end
  self.GetMillisecondsTime = function(self, ...) return requireTable.lua_GetMillisecondsTime(...) end
  
  --[[ modify code ]]--
  
  -- replaces the function ptr of timeGetTime with our own version
  core.writeCode(
    addrOfGetTime, -- normal 1.41 address: 0x0059e228
    {requireTable.funcAddress_GetTime}
  )

end

exports.disable = function(self, moduleConfig, globalConfig) error("not implemented") end

return exports