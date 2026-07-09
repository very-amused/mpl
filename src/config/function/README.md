# Config Functions

## What are Config Functions / Config Macros

A config function is a function that controls MPL's behavior (e.g play_toggle, seek, quit). A config macro is a function that *mutates* MPL's configuration state, and ensures that those mutations are immediately reflected.

### Remarks: Config Macros

Config macros are a fairly abstract topic right now, and won't really make sense until MPL's shell is implemented. When we get there, it will be objectively impossible to guarantee that any config variable can be mutated from the shell while MPL is running. Can we really let the user toggle between a CLI interface and a Qt interface in the middle of a song? Probably not. However, can we let them toggle between a CLI and ncurses TUI interface in the middle of a song. That sounds achievable. Thus, "config mutations we can perform at any point in MPL's runtime" is a subset of "config mutations we can perform when parsing mpl.conf". To expose mutations we can perform at any point, we will use config macros. Simply, we *cannot* expose *settings* via the shell, but we *can* expose *macros*.

We want to be able to change color themes with a keybind. We want to be able to toggle between CLI/TUI with a keybind. These are incredibly powerful goals, but are incredibly achievable with config macros.

Right now, there's only one config macro defined and used, which is `include_default_keybinds()`, which has allowed us to make MPL's default keybinds opt-in for any user who creates their own mpl.conf.

## How Can I Create a Config Function or Config Macro

1. Define the function / macro in `function_definitions.h` or `macro_definitions.h` respectively.
2. Implement the function / macro in `function_definitions.c` or `macro_definitions.c` respectively. You will find a static `state` variable in both of these that these functions can use to interface with MPL.
3. Register the function / macro in `register.cpp`. This will require the function's name, routine, and types for its argument(s) and return value. Your config function or config macro should now work with MPL! Try binding it to a key or calling it in the shell to test it out.


## Where Can I Find Existing Config Functions and Config Macros

- Config functions are defined in `function_definitions.h` and implemented in `function_definitions.c`
- Config macros are defined in `macro_definitions.h` and implemented in `macro_definitions.c`
