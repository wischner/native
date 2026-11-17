# Persistent configuration

This chapter expands Architecture Sections 8 and 17. The public settings class
is `native::configuration<T>`, declared in `include/native/configuration.h` and
included by `<native.h>`. Application identity belongs to `native::app`.
The application tutorial is [Chapter 23](../programming-native/23-PERSISTENT-CONFIGURATION.md).

## Binding and type ownership

`app::get_name()` supplies each binding's application namespace. Unless set
explicitly with `app::set_name("org.example.application")`, it captures the
filename from `app::argv[0]` on first use, retaining case and extension. For
example, `/opt/bin/mail` uses `mail`, and `C:\Tools\Mail.exe` on Windows uses
`Mail.exe`. No setup call is required in a normal Native `program()`.

The platform launcher supplies arguments before entering `program()`. Construct
settings there or in objects created there, rather than in global initializers.
If neither an explicit name nor startup arguments are available, `get_name()`
throws `std::logic_error`. Applications defining their own entry point must
populate `app::argc`/`app::argv` or explicitly set the name. Name access is
thread-safe once startup arguments have been initialized.

An explicit name is a nonempty UTF-8 filename component, at most 255 bytes,
without control characters, path separators, or the special names `.` and `..`.
Spaces and uppercase letters are retained. Names inherit the platform store's
case sensitivity; do not distinguish applications only by case. A stable
reverse-domain name avoids changes when an executable is renamed. Setting the
name does not change window titles. Existing bindings keep their captured
backend if the application name changes; future bindings use the new name.
There is no separate public store class, and no window or event loop is needed.

`configuration name("db.name", "contacts")` deduces `std::string`, including
when the default is a string literal. Omitting the default selects an empty
string. `configuration<int> port("db.port", 5432)` binds an integer. Reading
implicitly as `T`, calling `get_value()`, or streaming the binding reads a
value by copy. Assignment and `set_value()` persist immediately. Construction
and destruction do not read or write settings; a missing key returns the
constructor's default without writing it.

Supported types are `std::string`, `bool`, `short`, `int`, `long`, `long long`,
their unsigned counterparts, `float`, and `double`. Integer storage is signed
64-bit on every adapter; unsigned inputs above `INT64_MAX` are rejected.
Float storage uses a finite double. Reads check the stored category and the
destination's range, rejecting narrowing overflow and floating underflow.
Ordinary float rounding within range follows C++ conversion semantics. Strings
are valid UTF-8, at most 64 KiB, without embedded NUL. Native strings never
escape as borrowed pointers into a platform object or cache.

Setting keys are at most 255 bytes and consist of at most 16 nonempty dot-separated
segments of lowercase ASCII letters, digits, hyphens and underscores. This
keeps case-sensitive stores and the case-insensitive registry consistent and
prevents names from becoming filesystem paths. Dots name a hierarchy; they
are not directory traversal. There is no application-defined schema object.

Copy construction aliases a binding, including its cache and timeout.
Assignment from another binding reads the source and writes the destination;
it never changes the destination's store or key. These semantics also apply
when the source is a temporary. There is no conversion to a mutable reference:
application code cannot accidentally modify a cache without persisting.

## Read timeout

The third constructor argument is a `std::chrono::milliseconds` duration. The default is zero:
every access calls the platform adapter. A positive duration reuses the last
successful value until the elapsed monotonic time reaches the timeout. Missing
keys and their fallback values are cached under the same rule.

The timestamp is recorded after a successful read or write. It is not extended
by cache hits. `reload()` forces a read. `set_timeout()` rejects negative
durations and invalidates the existing cache. `reset()` removes the user's
override and invalidates this binding; the next read sees system defaults, if
available, or the constructor fallback. No background thread or timer is used.

A copied handle shares cache updates; separately constructed bindings have
independent caches even for the same key. Another process or binding's write
becomes visible after expiry or explicit reload. A failed read invalidates
the cache and throws; stale data is not returned as a successful read. A
failed write does not update the binding's cache.

## Platform storage

| Platform | User store | Representation |
| --- | --- | --- |
| Linux, including GEMix and every Linux toolkit | `$XDG_CONFIG_HOME/<application>/settings.ini`, defaulting to `$HOME/.config/<application>/settings.ini` | UTF-8 dotted-key text |
| Haiku | `find_directory(B_USER_SETTINGS_DIRECTORY)/<application>/settings` | Flattened native `BMessage` |
| Windows | `HKEY_CURRENT_USER\Software\<application>` | Registry values; dotted prefixes become subkeys |
| macOS | Current-user, any-host preference domain named by the application identifier | Core Foundation preferences |

Linux ignores relative XDG paths. If `XDG_CONFIG_HOME` is unusable, an absolute
`HOME` is required. On a missing user key, absolute entries in
`XDG_CONFIG_DIRS` are searched in order, defaulting to `/etc/xdg`. Writes go
only to the user store; reset can reveal a system value. Paths are resolved
when the binding is constructed. The XDG specification standardizes locations,
not a universal settings format. Native's text format is deliberately small:

```ini
# Native per-user settings
db.name = "contacts"
db.port = 5432
ui.enabled = true
ui.scale = 1.25000000000000000e+00
```

Strings use quotes and `\\`, `\"`, `\n`, `\r`, `\t`, or `\xHH` escapes.
Integer and floating values remain distinct; decimals or exponents identify
floating values. Empty lines and whole-line `#`/`;` comments are accepted.
Unknown syntax, duplicate keys, malformed UTF-8 and invalid values fail the
read. Successful rewrites preserve keys and values, not comments or ordering.

Haiku stores named string, bool, int64 and double fields, with one value per
key. `BMemoryIO` bounds unflattening; standard streams handle file contents.
Windows maps strings to `REG_SZ`, bools to `REG_DWORD` 0/1, signed integers to
`REG_QWORD`, and doubles to eight-byte `REG_BINARY`. A registry value of an
unsupported type is an error; integer values are not coerced into booleans.
macOS uses native CF strings, booleans and numbers and synchronizes the explicit
domain on actual reads and writes. No private plist writer bypasses the
preferences service.

The choices follow the [XDG base-directory specification](https://specifications.freedesktop.org/basedir/),
[Haiku settings layout](https://i18n.haiku-os.org/userguide/data/export/docs/userguide/en/filesystem-layout.html)
and [BMessage serialization](https://www.haiku-os.org/docs/api/classBMessage.html),
[Windows per-user registry guidance](https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-functions),
and [Core Foundation preferences](https://developer.apple.com/documentation/corefoundation/preferences-utilities).

## Failure and concurrency boundaries

Invalid identifiers/defaults/timeouts throw `std::invalid_argument`; checked
numeric range failures throw `std::out_of_range`. Malformed settings, type
mismatch and storage failures throw standard runtime/system/filesystem
exceptions. Only absence uses a fallback. A corrupt file is never silently
replaced with defaults by a subsequent write.

One binding mutex protects cache state, and a process mutex serializes backend
operations. Linux and Haiku writers additionally acquire a POSIX advisory lock
on a stable `settings.lock` file. After locking, they reload and merge the
current file, preserving other writers' keys. Each candidate is written with
standard streams in an exclusive sibling staging directory and published with
`std::filesystem::rename`. New application directories are user-only, and
published settings files have owner read/write permissions. The OS releases
the writer lock on process exit; the empty lock file is retained. A process
crash may leave a harmless staging directory.

File stores accept at most 1024 keys and one MiB of encoded data. Native
services own their own storage limits. Persistence means the platform accepted
the write; it does not promise survival of sudden power loss. Registry writes
use normal Windows flushing policy. The API provides no multi-key transaction,
atomic read-modify-write operator, cross-process cache invalidation, encryption,
or credential-vault service. Slow reads/writes can block; a timeout cache reduces
reads but does not make storage asynchronous.

## Verification

`native_configuration_tests` links the shared implementation to an instrumented
backend. It checks read counts, defaults, type/range errors, copy versus
assignment semantics, argv filename fallback, application-name capture, failed reads/writes, expiry, reload,
and multithreaded use of a shared binding. `native_configuration_storage_tests`
uses a unique real application namespace, verifies native representation,
launches concurrent writer processes and checks that no unrelated keys were
lost. Linux/Haiku also test corrupt-file preservation; Linux tests failed
publication, XDG precedence and relative-path rejection. Both tests run without
a desktop or window. Actual target results are recorded in the
[feature matrix](FEATURE-MATRIX.md).
