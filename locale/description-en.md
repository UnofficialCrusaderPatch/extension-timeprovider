# Time Provider

**Author**: TheRedDaemon  
**Version**: 0.1.0  
**Repository**: [https://github.com/UnofficialCrusaderPatch/extension-timeprovider](https://github.com/UnofficialCrusaderPatch/extension-timeprovider)

This modules passive effect is increasing the general time resolution of the main game loop from milliseconds to microseconds, so the higher game speed settings will transition more smoothly.
Additionally it allows to configure how the game should handle game speed settings that the game can not keep up with. Note that the limiters only apply over 90 game speed. Vanilla speeds will always use the vanilla limiters to stay compatible for multiplayer, so there can be a harsh transition when the game speed is increased over 90. Only one limiter type can be selected.

Support is currently only guaranteed for the western versions of Crusader 1.41 and Crusader Extreme 1.41.1-E.

### Options

The modules options deal with limiters and are explained in detail in the configuration tab.

### Module Functions

The module provides multiple functions to get the current time in different resolutions. All of these are backed by the same underlying function. Despite its influence on the game loop, it is recommended to use this module for timing purposes in other modules to have a common time source.

More details are found in the repositories README.

### Feedback

Should issues or suggestions arise that are related to this module, feel free to add a new GitHub issue.