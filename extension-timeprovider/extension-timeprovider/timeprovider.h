
#ifndef TIME_PROVIDER_HEADER
#define TIME_PROVIDER_HEADER

// also needs windows.h, although the way of including can be handled by the user
#include "lua.hpp"

#include "ucp3.h"

namespace TimeProviderHeader
{
  using FuncGetTime = DWORD(__stdcall*)();

  inline constexpr char const* NAME_VERSION{ "0.0.1" };
  inline constexpr char const* NAME_MODULE{ "timeprovider" };
  inline constexpr char const* NAME_LIBRARY{ "timeprovider.dll" };

  inline constexpr char const* NAME_GET_TIME{ "_GetTime@0" };

  inline FuncGetTime GetTime{ nullptr };

  inline bool initModuleFunctions()
  {
    GetTime = (FuncGetTime) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_GET_TIME);
    return GetTime;
  }
}

#endif //TIME_PROVIDER_HEADER