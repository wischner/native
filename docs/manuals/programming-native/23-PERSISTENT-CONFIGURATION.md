# 23. Persistent configuration

Use `configuration<T>` for small application preferences. It behaves as a typed
value when read and writes to the platform's preference store when assigned.
`settings_sidebar` builds settings UI; `configuration` stores the values.

## Declare a setting

No application setup is required: configuration takes its namespace from
`app::get_name()`, which defaults to the filename in `argv[0]` (including the
extension). Inside `program()`, simply declare the values you need:

```cpp
native::configuration name("db.name", "contacts");
native::configuration<int> port("db.port", 5432);
native::configuration<bool> enabled("db.enabled", true);
native::configuration<double> scale("ui.scale", 1.0);

std::string database_name = name; // Read as an owning string.
int database_port = port;        // Read as an int.
if (enabled) {
    port = 6000;                // Persist immediately.
    name = "archive";           // Persist a UTF-8 string.
}
```

The string type is the default, so `native::configuration name("db.name")`
uses an empty string when the key is absent. A literal default deduces an
owning `std::string`. Nothing is read or written just by declaring a binding.
Missing settings use the default; they are not automatically written.

Keys use lowercase ASCII letters, digits, hyphens and underscores in nonempty
dot-separated segments (at most 16 segments and 255 bytes). Invalid keys throw.

To keep settings under a stable name even when the executable is renamed, set
the name once before constructing settings:

```cpp
native::app::set_name("org.example.mail"); // Optional override.
native::configuration name("db.name", "contacts");
```

`app::get_name()` returns the chosen name. Application names can contain UTF-8,
spaces and uppercase letters; they must be nonempty filename components of at
most 255 bytes, without path separators, control characters, or `.`/`..`.
Use a stable name and do not distinguish applications only by case. Changing
the name affects future bindings; existing bindings keep their original store.
It does not change any window title. Construct settings in `program()` or in
objects created there, after the launcher has supplied the arguments.

## Control the read cache

```cpp
using namespace std::chrono_literals;

native::configuration<int> port("db.port", 5432, 5s);
int first = port;       // Read storage and start the timeout.
int next = port;        // Use the cache if five seconds have not elapsed.

port = 6000;            // Always write immediately; refresh this cache.
int fresh = port.reload(); // Force storage access before the timeout expires.
port.set_timeout(0ms);  // Invalidate the cache; every later read hits storage.
port.reset();           // Delete the user override; next read uses a default.
```

Zero is the default timeout. Positive timeouts are checked only when the value
is read, using a monotonic clock. There is no background reload thread. Cache
hits do not extend the timeout. Missing-key defaults are cached too. Negative
timeouts throw. Independent bindings for the same key have independent caches;
use `reload()` when you must observe another binding or process's write now.

`set_value(value)` provides fluent assignment when useful:

```cpp
port.set_timeout(std::chrono::seconds(10)).set_value(6000);
```

## Ordinary C++ values

```cpp
native::configuration name("db.name", "work");
native::configuration<int> port("db.port", 5432);

std::string title = "Database: " + name.get_value();
std::cout << name << ':' << port;
```

`get_value()` is useful for generic functions that deduce template arguments,
string member functions, formatting and concatenation. Implicit conversion
does not make a wrapper participate in every `std::string` template overload.
Reads return copies: no reference, pointer or iterator exposes a mutable cache.
`auto value = port` copies a configuration handle; `int value = port` takes a
snapshot. A copied handle shares the original cache and timeout. Assigning one
handle to another copies the current value into the destination's persistent
key; it never rebinds that key.

Supported types are `std::string`, `bool`, `short`, `int`, `long`, `long long`,
their unsigned variants, `float` and `double`. Integers must fit signed 64-bit
storage, and reads must fit the requested type. Floating values must be finite;
strings must be valid UTF-8 without embedded NUL and no longer than 64 KiB.
An integer setting is distinct from a string containing digits or a floating
setting. The API does not silently coerce stored categories.

## Handle errors where reads and writes happen

```cpp
try {
    int current_port = port;
    port = current_port + 1;
} catch (const std::exception &error) {
    std::cerr << "Could not access preferences: " << error.what() << '\n';
}
```

The default applies only when a key is missing. Wrong types, corrupted files,
out-of-range values and storage failures throw. A failed write does not update
the cache; a failed read does not return stale cached data as success.
Destruction never writes. Reads and writes can block; perform potentially slow
work on an application-owned worker when necessary. The example increment is
not an atomic cross-process operation, and separate assignments are not a
multi-key transaction. Do not use preference storage for passwords or tokens.

## A complete persistent counter

This program increments a saved launch count and displays it in the window
title. Run it twice to see the value survive process exit. The failure path
reports an error rather than overwriting an unreadable store.

```cpp
#include <native.h>
#include <iostream>
#include <limits>
#include <string>

int program(int, char **) {
    try {
        native::app::set_name("org.example.launch-counter"); // Optional stable name.
        native::configuration<int> launches("usage.launches", 0);
        const int previous = launches;
        if (previous < std::numeric_limits<int>::max())
            launches = previous + 1;
        native::app_wnd window(
            "Launch " + std::to_string(launches.get_value()), 80, 80, 420, 180);
        return native::app::run(window);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

## Where the values live

| Platform | Store |
| --- | --- |
| Linux, including GEMix | `$XDG_CONFIG_HOME/<application>/settings.ini`, or `~/.config/<application>/settings.ini` |
| Haiku | `~/config/settings/<application>/settings`, located through the native directory API and encoded as a `BMessage` |
| Windows | Per-user registry under `HKEY_CURRENT_USER\Software\<application>` |
| macOS | Native preference domain named by `<application>` |

Linux also reads missing keys from `XDG_CONFIG_DIRS` in priority order, normally
`/etc/xdg`. Reset removes your override, which may reveal one of those defaults.
Each setting uses the OS adapter regardless of which UI toolkit is selected.
For arbitrary application-owned files, continue to use `special_directory`
and standard C++ streams. The implementation and persistence limits are in
[Persistent configuration](../book-of-native/CONFIGURATION.md).
