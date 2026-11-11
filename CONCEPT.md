# Why the JSON buffer can be the configuration

## The engineering problem

This library grew out of configuration work for an ESP32-based 4G gate
controller. A Node.js server backed by MongoDB naturally stores a flexible
JavaScript document. The device receives configuration over TCP. On the server,
adding a threshold or a label can be a small document change. On firmware that
stores the same data in a fixed C struct, that change can cross several layers.

For example, a struct might contain a 32-byte name, two uint64_t timestamps and
a boolean. Binary serialization must define field widths, byte order, packing,
alignment and version rules independently of the compiler. Sending `sizeof`
bytes from a native struct does not define a portable protocol. Padding can vary
between targets and fields can move after source changes. Persisting that binary
layout then makes firmware upgrades a migration problem as well as a transport
problem. JSON does not remove schema design, but it separates the wire/storage
format from native memory layout.

Fixed character arrays introduce another cost: capacity is paid for even when a
name is short. `char name[64]` consumes 64 bytes for a ten-character label. Making
all fields dynamic instead shifts the problem to allocation, ownership and
fragmentation. Application developers can spend substantial effort maintaining
representations that differ mainly in how the same configuration is laid out.

## Retaining the serialized source

The central choice is to keep the received JSON buffer and treat that buffer as
the persistent in-memory configuration. A lookup scans the bounded byte range,
recognizes member names and values, and interprets only the requested value. It
does not allocate nodes or deserialize the document into a second configuration
object. The parser instance stores a pointer and a length; the application owns
the bytes and their lifetime.

Document-based libraries solve a broader problem: convenient traversal,
modification, serialization, arrays and nesting. Their parsed representation can
consume memory in addition to the input. Exact costs depend strongly on library
version and ownership mode. ArduinoJson offers mature facilities and different
memory-management models across versions, including dynamic allocation in modern
versions. The choice here is about the workload, not a claim that those libraries
are incorrectly designed or always less efficient.

A device that receives small configuration infrequently, saves it as text, and
occasionally asks for a few values can avoid maintaining a parsed object model.
It can store the bytes as received, load them into one working buffer at startup,
and use the same access API. Filesystem buffers and loading can allocate; parser
operations do not. The library has no persistence or networking policy.

## Types belong at the call site

JSON numbers are text and do not carry C++ integer widths. A timestamp may need
uint64_t, while a GPIO number might require uint8_t. The caller states that choice
with `get<T>`. Conversion checks the range before assigning. Integer getters
reject decimal/exponent notation to keep the integer contract exact and simple;
applications can choose floating access when that representation is intentional.
Booleans accept only actual JSON booleans.

Syntax validity is separate from application validity. An empty object is valid
input but may lack every field required to run the device. A successful uint16_t
port conversion still does not establish that a particular port is allowed.
Each getter returns a status alongside its value. Missing fields, null, wrong
types and range failures must be handled explicitly instead of silently turning
into zero. Applications decide when a fallback is appropriate.

## Zero-copy strings and encoding

A string already occupies a contiguous range in the input. Returning a pointer
and a length allows an application to print or compare those bytes without a
second buffer. There is no fixed maximum label capacity per field. The lifetime
contract is correspondingly explicit: the source must remain alive and unchanged,
and a view cannot be treated as a NUL-terminated C string.

JSON escaping complicates the meaning of a view. `"a\nb"` stores an escape
sequence, not a literal newline. This version returns the encoded bytes and
makes no implicit copy. Displaying decoded strings would require a streaming
decoder or a caller-supplied output buffer; neither is part of V0.01. Key matching
does decode escapes without allocation so escape-equivalent keys are treated
consistently. String validation checks UTF-8 and UTF-16 surrogate escape pairs.
Unicode normalization is deliberately outside the scope.

## Safety and scanning costs

The parser always has both pointer and length. Each scanner checks its position
before reading, so a buffer need not have a terminator or accessible trailing
padding. Scanning strings recognizes quotes, escapes and encoded characters;
punctuation inside a string cannot become an object delimiter. The number scanner
checks JSON grammar independently of numeric conversion.

A getter scans through the end rather than returning as soon as a key matches.
Otherwise an apparently valid required field could be followed by malformed
input, and different lookups could disagree about whether the configuration is
usable. Duplicate keys are errors, including differently escaped keys that decode
to the same text. This avoids first-wins/last-wins disagreements with other systems.

Rejecting arbitrary duplicate keys without storing a key set means comparing each
new member against earlier members. This implementation rescans, giving O(n²)
worst-case validation/lookup time with O(1) scratch space. A simple scan without
duplicate detection would be linear, but weaker ambiguity handling was not chosen.
For a handful of configuration fields this can be a reasonable memory/time trade.
It is unsuitable for high-frequency lookups over large documents. Caller limits
on received document size are necessary to bound CPU time as well as RAM.

The implementation stores no mutable validation cache. This keeps the object small
and avoids returning a stale validation result after the caller changes a buffer.
The source still must not be changed during an operation. Frequently used native
scalars can be extracted once by the application; the library does not prohibit
application variables simply because it avoids a mandatory full configuration struct.

## Memory is a workload-dependent tradeoff

JSON is not intrinsically a compact binary representation. A uint64_t can occupy
20 decimal digits, before including its key and punctuation, instead of eight
native bytes. Each repeated field name costs space. On the other hand, a short
string pays for its actual serialized length rather than a worst-case array
capacity. Avoiding an additional tree can matter when the input must already
remain available for saving or retransmission.

The README gives byte counts for a specific JSON example and explicit struct ABI
assumptions. Those figures show that a packed or sensibly aligned native struct
can be smaller. The meaningful comparison includes all simultaneously retained
buffers, struct string capacities, document-library ownership and allocator
metadata. This library claims no universal RAM reduction. Floating conversion also
has a code-size cost from the platform math library, even though it allocates no
parser storage.

## Deliberate first-version boundaries

V0.01 supports one flat object. Arrays, nesting, mutation and recursion would add
API and traversal decisions before the basic lifetime/error model is established.
Nested values are reported as unsupported rather than skipped or partially used.
Scanner functions isolate strings and scalar values so later bounded traversal
can reuse them without requiring a tree.

Arrays are a possible V0.02 feature and nested paths a possible V0.03 feature.
No proposed future path syntax is a commitment; a key with dots is just a literal
key today. A selective validator, optional index, field iterator, or explicit
string decoder could be added later while preserving caller-owned storage, but
none is required to retrieve a few scalar configuration settings now.

## Choosing a representation

This approach suits small, read-mostly settings received as JSON, especially
variable-length labels and configurations already persisted or retransmitted as
text. It can reduce ownership complexity when a single serialized buffer can
serve communication, storage and occasional access.

Native structs suit fixed schemas, strict timing, compact binary protocols and
very frequent field access. ArduinoJson or another mature JSON library suits
nested documents, flexible traversal, updates, filtering, serialization and broad
interoperability requirements. Hard real-time tasks should not perform repeated
full scans. Large or adversarial documents require stronger workload controls.

Persistence remains a separate subsystem. Real deployments need a plan for
interrupted writes, integrity checks, authentic updates, schema versions and
rollback. The LittleFS example demonstrates basic save/load mechanics rather
than implementing that deployment policy. The original idea remains narrow:
retain one serialized buffer and interpret individual values without constructing
an object model that the application does not need.
