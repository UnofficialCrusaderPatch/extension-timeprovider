# Extension Time Provider

This repository contains a module for the "Unofficial Crusader Patch Version 3" (UCP3), a modification for Stronghold Crusader.  
The module increases the time resolution of the main game loop from milliseconds to microseconds, while also being a time provider for other modules.

### Motivation and Plan

Crusader works mostly with milliseconds, but while these resolution was fine for its time, modern cores, despite only one being used, can run the game at much higher speeds. The issue was that certain computations would become very imprecise on such speeds.  
The module tackles this by providing game loop relevant parts with a resolution of microseconds while the computations are changed to accommodate it. Additionally it allows to configure how the game should handle game speed settings that the game can not keep up with. Note that the limiters only apply over 90 game speed. Vanilla speeds will always use the vanilla limiters to stay compatible for multiplayer, so there can be a harsh transition when the game speed is increased over 90.

### Usage

The module is part of the UCP3. Certain commits of the main branch of this repository are included as a git submodule.
It is therefore not needed to download additional content.

However, should issues or suggestions arise that are related to this module, feel free to add a new GitHub issue.
Support is currently only guaranteed for the western versions of Crusader 1.41 and Crusader Extreme 1.41.1-E.

### Options

The modules options deal with limiters. The following list will go through every option and explain it brief. More detail is found the the localizations. The structure is taken from the configuration:

* **limiterType** - The type of limiter to use.
    * *vanilla*  
    Caps at 11 game ticks per frame. Should the actual game tick time be bigger then actually needed by the speed setting, then it will slow down to 2 or 3 actions.
    * *maxActionsPerLoop*  
    Similar to vanilla a setting that allows to configure the maximum game ticks per frame. It also does not possess the slow down cases of vanilla.
    * *fixedFpsFloor*  
    Will allow the requested game ticks as good as possible. If the resulting frame rate sinks bellow the set minimum, the game ticks are interrupted to keep the FPS up. Due to averaging, short game tick slow downs despite enough computing resources might by encountered when reaching the limit.
    * *noLimiter (NOT RECOMMENDED)*  
    The game always tries to perform all actions requested by the game speed and will try to catch up. If the computer is not fast enough, the situation will escalate and the game will essentially freeze.

* **maxNumberOfActionsPerLoop** - Maximum number of game ticks to perform during one frame if limiter is *maxActionsPerLoop*.

* **minFramesPerSecond** - Minimum number of frames allowed by *fixedFpsFloor* before the game tries to reduce the game ticks per frame to keep them up. Should not exceed the possible FPS, since the game would then set the possible game ticks to 1 in an attempt to raise the frame rate once the limiter springs into action.

### C++-Exports

The API is provided via exported C functions and can be accessed via the `ucp.dll`-API.  
To make using the module easier, the header [timeProviderHeader.h](extension-timeprovider/extension-timeprovider/timeProviderHeader.h) can be copied into your project.  
It is used by calling the function *initModuleFunctions()* in your module once after it was loaded, for example in the lua require-call. It tries to receive the provided functions and returns *true* if successful. For this to work, the *timeprovider* needs to be a dependency of your module.
The provided functions are the following:

* `uint64_t GetFullNanosecondsTime()`  
  Returns a full game intern timestamp in nanoseconds. Should only be used in relation to other timestamps.

* `DWORD GetNanosecondsTime()`  
  Returns the nanosecond part of the current timestamp. Should only be used in relation to other timestamps. Overflows very fast due to the nature of nanoseconds.

* `DWORD GetMicrosecondsTime()`  
  Returns the microsecond part of the current timestamp. Should only be used in relation to other timestamps.

* `DWORD GetMillisecondsTime()`  
  Returns the millisecond part of the current timestamp. Should only be used in relation to other timestamps.

### Lua-Exports

The Lua exports are parameters and functions accessible through the module object.

* `int GetFullNanosecondsTime()`  
  Identical to C++ version.

* `int GetNanosecondsTime()`  
  Identical to C++ version.

* `int GetMicrosecondsTime()`  
  Identical to C++ version.

* `int GetMillisecondsTime()`  
  Identical to C++ version.

### Special Thanks

To all of the UCP Team, the [Ghidra project](https://github.com/NationalSecurityAgency/ghidra) and
of course to [Firefly Studios](https://fireflyworlds.com/), the creators of Stronghold Crusader.