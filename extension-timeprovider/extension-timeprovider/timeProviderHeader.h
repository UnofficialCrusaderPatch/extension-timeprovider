
#ifndef TIME_PROVIDER_HEADER
#define TIME_PROVIDER_HEADER

#include <cstdint>

// also needs windows.h, although the way of including can be handled by the user
#include "lua.hpp"

#include "ucp3.h"

namespace TimeProviderHeader
{
  using FuncGetFullNanosecondsTime = uint64_t(__stdcall*)();
  using FuncGetNanosecondsTime = DWORD(__stdcall*)();
  using FuncGetMillisecondsTime = DWORD(__stdcall*)();

  inline constexpr char const* NAME_VERSION{ "0.0.1" };
  inline constexpr char const* NAME_MODULE{ "timeprovider" };
  inline constexpr char const* NAME_LIBRARY{ "timeprovider.dll" };

  inline constexpr char const* NAME_GET_FULL_NANOSECONDS_TIME{ "_GetFullNanosecondsTime@0" };
  inline constexpr char const* NAME_GET_NANOSECONDS_TIME{ "_GetNanosecondsTime@0" };
  inline constexpr char const* NAME_GET_MILLISECONDS_TIME{ "_GetMillisecondsTime@0" };

  inline FuncGetFullNanosecondsTime GetFullNanosecondsTime{ nullptr };
  inline FuncGetNanosecondsTime GetNanosecondsTime{ nullptr };
  inline FuncGetMillisecondsTime GetMillisecondsTime{ nullptr };

  inline bool initModuleFunctions()
  {
    GetFullNanosecondsTime = (FuncGetFullNanosecondsTime) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_GET_FULL_NANOSECONDS_TIME);
    GetNanosecondsTime = (FuncGetNanosecondsTime) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_GET_NANOSECONDS_TIME);
    GetMillisecondsTime = (FuncGetMillisecondsTime) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_GET_MILLISECONDS_TIME);
    return GetFullNanosecondsTime && GetNanosecondsTime && GetMillisecondsTime;
  }
}

#endif //TIME_PROVIDER_HEADER