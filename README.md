# ZeroAllocJSON

**The serialized JSON buffer is the configuration.** Version 0.01 (package version
`0.0.1`) is a small C++11 library for reading a flat JSON object directly from a
caller-owned, read-only, length-bounded character buffer. It builds no document
tree, reserves no document pool, and allocates no parser heap memory.

Designed for configuration that changes infrequently and is read occasionally,
with ESP32 as the first embedded target. The core has no Arduino, filesystem,
exceptions, RTTI, or third-party dependencies. It is portable to platforms with
C++11, `<stdint.h>`, `<type_traits>`, `<limits>`, and the standard math library.
Flash pointers must be normally addressable; AVR PROGMEM access is not supported.

## Installation

### Arduino IDE

Download this repository as a ZIP and choose **Sketch → Include Library → Add
.ZIP Library**. Alternatively place its directory in your sketchbook's
`libraries/ZeroAllocJSON`. Open **File → Examples → ZeroAllocJSON → BasicUsage**.
For ESP32, install Espressif's Arduino board package and select your board.

### PlatformIO

Add to your project's `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = https://github.com/C3POAdmin/ZeroAllocJSON.git
build_flags = -std=gnu++11
```

Pin a commit or tag for reproducible production builds. This repository is
prepared for library installation; it has **not** been published to Arduino
Library Manager or PlatformIO Registry.

## Quick start

```cpp
#include <ZeroAllocJSON.h>

const char json[] = R"({"name":"Front Gate","starttime":1790000000,"enabled":true,"threshold":12.75})";
ZeroAllocJSON cfg(json, sizeof(json) - 1);

void setup() {
  Serial.begin(115200);
  auto start = cfg.get<uint64_t>("starttime");
  auto enabled = cfg.get<bool>("enabled");
  auto voltage = cfg.get<float>("threshold");
  auto name = cfg.getString("name");
  if (!start || !enabled || !voltage || !name) {
    Serial.println("Missing or invalid required configuration");
    return;
  }
  // Result<T> contains .value and .status; check success before using .value.
  Serial.write(reinterpret_cast<const uint8_t*>(name.value.ptr), name.value.length);
  Serial.println(enabled.value ? " enabled" : " disabled");
  cfg.showJSON(Serial);
  Serial.println();
  cfg.showVariable("starttime", Serial);
}
void loop() {}
```

Use the complete [BasicUsage example](examples/BasicUsage/BasicUsage.ino), which
also samples ESP32 free heap around repeated getters. Background tasks can affect
that measurement; host allocation instrumentation gives a more controlled check.

The source buffer must stay alive and unchanged for every operation and for the
lifetime of every returned string view. A terminating NUL is unnecessary. Supply
the actual byte length, excluding any terminator; trailing NULs are not whitespace.
Do not modify the buffer concurrently with a lookup. Independent reads on an
unchanging buffer have no mutable parser state.

## Strings

`getString(key)` returns `Result<StringView>`. The successful view has
`const char* ptr` and `size_t length`. It points **inside the original buffer**,
excluding the surrounding quotes. It is not necessarily NUL-terminated. Never
pass it to `strlen`, print it as a C string, or retain it after replacing the
buffer. Empty strings have length zero and are successful results.

Values remain JSON-encoded: `"a\nb"` produces the four-byte view `a\nb`, not a
three-byte string with a newline. Unicode escapes similarly remain encoded.
There is no automatic decoding or copy-and-unescape helper in this version.
Keys, however, are compared by decoded Unicode value: `"\u006eame"` matches
`getString("name")`. Caller keys are NUL-terminated UTF-8 strings; keys containing
an escaped NUL cannot be addressed through this API. UTF-8 is validated, but
Unicode normalization is not performed.

## Typed getters and errors

`get<T>(key)` supports `bool`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`,
their signed equivalents, `float`, and `double` (also other arithmetic types
supported by the compiler). The explicit type determines the conversion; no
schema is stored. Booleans accept only JSON `true` or `false`; numbers and
strings are never coerced to booleans or other types.

Integer getters accept only integer-form JSON numbers. `1.0` and `1e0` return
`TypeMismatch`, even though they are mathematically integral. Integer boundaries
are checked before conversion. Unsigned targets reject any minus sign, including
`-0`. Float getters accept integer, decimal and exponent forms, with range checks
for the requested type. Nonzero values rounding to zero report `OutOfRange`;
representable subnormals are allowed. Ordinary rounding to the target floating
precision occurs. Conversion retains up to 19 significant decimal digits and is
not a promise of correctly rounded conversion for every halfway decimal case.
`double` may have the same precision as `float` on some embedded platforms.

```cpp
auto port = cfg.get<uint16_t>("port");
if (!port) {
  Serial.println(ZeroAllocJSON::statusName(port.status));
  // Reject this configuration or explicitly apply your application's fallback.
} else {
  // Use port.value.
}
```

| Status | Meaning |
| --- | --- |
| `Ok` | Successful validation or access |
| `InvalidJSON` | Malformed flat-object syntax, encoding, truncation or trailing data |
| `Unsupported` | Encountered an object/array value; nested input is outside V0.01 |
| `DuplicateKey` | Two keys decode to the same sequence of Unicode scalars |
| `MissingKey` | Valid supported document, requested key absent |
| `NullValue` | Typed/string getter requested a null field |
| `TypeMismatch` | Value's JSON type/form cannot be converted as requested |
| `OutOfRange` | Integer overflow, negative unsigned value, or floating range failure |
| `InvalidArgument` | Null source pointer or null requested key |

Failed results contain value-initialized storage, but this is **not a fallback**.
Always check `.status` or the explicit boolean conversion before reading `.value`.
A false boolean setting is a successful result whose `.value` is false.

`isNull(key)` returns `Result<bool>`: success/true for null, success/false for a
non-null value, and an error for absent keys or invalid input. `validate()` checks
supported document syntax, encoding, and duplicates. It does not check that your
application's required fields exist or that their ranges are appropriate. Check
required getters and application-specific constraints before accepting an update.

## Supported input and limits

One flat JSON object, including `{}`, with strings, signed/unsigned integer-form
numbers, decimal/exponent numbers, booleans and null. JSON whitespace, backslash
escapes, valid UTF-8 and paired UTF-16 surrogate escapes are recognized. Strings
cannot contain unescaped control bytes. Leading-zero numbers, NaN, Infinity,
comments, trailing commas and trailing data are rejected.

Every getter validates the **whole document** before returning success, so a valid
field followed by malformed input never appears successful. Duplicate keys are
rejected, including escape-equivalent spellings. Encountering nesting returns
`Unsupported` immediately; this is not a general validator for nested JSON.
There is no recursion, mutation, array API, path traversal, field iterator, or
cached index. Dots in keys are literal dots. A null source pointer is invalid,
even with zero length. The caller must supply a readable range of the stated size.

## Debugging

`showJSON(Serial)` writes the original serialized bytes after validation.
`showVariable("enabled", Serial)` writes `enabled: true`, using the requested
key and original raw value (including quotes for strings). Neither appends a
newline. Both return `Status`. They accept any Print-compatible object with
`write(const uint8_t*, size_t)`; the core never includes Arduino headers. The
status describes JSON/key validity, not output transport success or short writes.
The sink itself can allocate or block independently of the parser.

## Flash configuration

[ESP32FlashConfig](examples/ESP32FlashConfig/ESP32FlashConfig.ino) demonstrates
saving validated bytes to LittleFS, loading into a fixed 1024-byte RAM buffer,
checking file size/short reads, and reading fields. It saves defaults only when
no configuration exists. Provision LittleFS first; the example deliberately does
not format automatically after a mount failure. Filesystem operations can allocate.

The workflow is server JSON → flash bytes → caller's RAM buffer → getters. The
parser has no filesystem dependency. Saving directly to the example file is a
minimal demonstration, **not a power-loss-safe update protocol**. Transactional
updates, checksums, authentication of received configuration, application version
fields, migration policy, and flash wear management belong to the surrounding
system. Validate required fields before committing a real configuration update.

## Memory and performance

The parser holds only a source pointer and a length: typically 8 bytes on a
32-bit target and 16 bytes on a 64-bit host (verify `sizeof(ZeroAllocJSON)` for
your ABI). Tokens and counters are automatic locals. Strings use pointer/length
views; numeric results are native values plus status and ABI padding. No node
pool, field index, persistent struct, or copied value strings is required.

A concrete storage comparison, excluding allocator overhead and application
variables, for:

```json
{"name":"Front Gate","starttime":1790000000,"endtime":1790003600,"enabled":true,"threshold":12.75}
```

| Representation | Assumptions | Bytes |
| --- | --- | ---: |
| Serialized JSON | ASCII, no terminator | 98 |
| Caller RAM buffer | Above JSON plus optional terminator | 99 |
| Parser instance | 32-bit pointer and size_t | 8 |
| Example struct | `char name[32]`, two uint64_t, bool, float, 8-byte alignment | 56 |
| Example struct with name[64] | Same ABI assumptions | 88 |

The JSON plus parser can exceed either struct. Avoiding a *second* document tree
or oversized string reservations is the potential saving. For another concrete
comparison, `{"counter":18446744073709551615}` is 32 bytes of text versus 8 bytes
for a native uint64_t alone. A document-library comparison depends on library
version, string ownership, allocator and capacity; no universal ratio is claimed.
Loading a JSON buffer or using an Arduino output/filesystem may allocate. The
zero-allocation claim concerns this parser's operations, not the whole firmware.
Stack size and math-library code size depend on the target; floating conversion
uses `long double` and `powl`, which can add flash cost.

Lookups scan without building a tree. Duplicate detection rescans earlier members,
so worst-case work is **O(n²)** in document bytes per validation/getter, rather than
a strictly linear lookup. Scratch memory is O(1). Repeated access repeats that
work; retrieve a few frequently needed scalars once in application code. Large
untrusted documents need caller-enforced size limits. No stack recursion occurs.

## Testing

Linux/POSIX host tests (GCC or Clang):

```sh
./test/run.sh
# Or CMake:
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The script enables C++11, warnings as errors, disabled exceptions/RTTI, ASan and
UBSan. Tests exercise all requested integer widths and limits, floating values,
null/bools, strings/escapes/Unicode, duplicates, missing keys, malformed JSON,
read-only buffer edges, repeated access, Print-style output, and deterministic
arbitrary-byte cases. Guard-page tests check every truncation of a representative
document. An instrumented C++ allocator counts allocations during the suite;
there are no dynamic standard containers in the tests or library.

See [VALIDATION.md](VALIDATION.md) for checks actually run for this implementation.
CI repeats host checks and builds both examples for an ESP32 with PlatformIO.

## Roadmap

- V0.02: arrays and indexed access, final API undecided.
- V0.03: nested object paths using bounded traversal.
- Future: memory-mapped flash access where appropriate, selective/schema
  validation, caller-provided decoding buffers, field iteration and optional
  lookup optimizations.

None of those features is implemented in V0.01. See [CONCEPT.md](CONCEPT.md) for
design tradeoffs. MIT licensed; see [LICENSE](LICENSE).
