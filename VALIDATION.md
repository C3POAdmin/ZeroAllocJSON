# Validation performed

Implementation date: 11 November 2026. Package version: 0.0.1 (V0.01).

## Host

Ran `./test/run.sh` on Linux with GCC, C++11, `-Wall -Wextra -Wconversion
-Werror`, `-fno-exceptions -fno-rtti`, AddressSanitizer and UndefinedBehaviorSanitizer.
Result: all assertions passed with no sanitizer findings. The C++ allocation
counter remained unchanged throughout the tests, including numeric conversion.
The implementation contains no malloc/new calls or dynamic containers. This is
not a measurement of allocations made by a complete embedded application.

Coverage includes all 8/16/32/64-bit signed/unsigned integer limits and adjacent
range errors, floating overflow/underflow/subnormals, bool and null, zero-copy
string addresses, encoded escapes, Unicode keys and surrogate pairs, duplicate
keys, whitespace, invalid type conversions, malformed JSON, missing keys,
Print-style output, and 1,000 repeated lookups.

A document and all its truncated prefixes are placed immediately before a
PROT_NONE page without a terminator. Tests also exercise 20,000 deterministic
arbitrary-byte inputs at that protected boundary and all single-byte substitutions
of three representative valid documents, including UTF-8 and Unicode escapes.
These tests improve confidence in bounded scanning; they are not a formal proof
or a claim of exhaustive JSON conformance.

CMake was not installed in the supplied environment. The direct compiler test
script was used locally; the CMake configuration is included in CI.

## ESP32 compile checks

Installed PlatformIO 6.2.0 in a project-local virtual environment. Built both
examples for `board = esp32dev`, `platform = espressif32@6.10.0`, Arduino framework
2.0.17, using the Xtensa ESP32 GCC 8.4 toolchain.

| Example | Compile/link | Static RAM | Firmware flash |
| --- | --- | ---: | ---: |
| BasicUsage | PASS | 21,476 bytes | 276,741 bytes |
| ESP32FlashConfig | PASS | 22,520 bytes | 312,313 bytes |

These are complete example firmware sizes, including Arduino, serial and (for
the second example) filesystem dependencies. They are not the parser's standalone
RAM/flash cost. No physical device, flash-save cycle, power-loss test, cellular
connection, or serial output was exercised. Other boards were not built locally.

GitHub Actions is configured to repeat host/CMake and both ESP32 builds. Adding
workflow files does not establish that a remote CI run has completed successfully.

## Scope

Flat objects only. No arrays, nested access, mutation, automatic string decoding,
filesystem implementation or library-registry publication. Duplicate detection
requires quadratic worst-case scanning time. Floating conversions retain 19
significant decimal digits and follow the target's native floating precision.
