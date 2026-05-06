<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR: Call-based LovyanGFX port protocol

## Status

Accepted

This ADR remains the active decision for the v2 scalar call protocol.

Render-batching details were superseded by [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](2026-05-03-binary-batch-as-render-transaction-api.md). Current v2 uses `submitBinaryBatch` as an explicit binary frame-script entry point, not the earlier tuple/list batch sketch.

## Context

This project is an AtomVM ESP-IDF component that exposes LovyanGFX functionality to Elixir code running on AtomVM.

The existing `atomlgfx` implementation proves that the concept works. It can call LovyanGFX from AtomVM and render graphics on ESP32 devices. However, the current native implementation has become larger than desirable for what is fundamentally a thin wrapper around the upstream LovyanGFX library.

The main source of complexity is that each LovyanGFX operation tends to require repeated native plumbing:

- request decoding
- argument validation
- command dispatch
- target resolution
- LovyanGFX method invocation
- reply encoding
- optional batch handling

This creates a large C/C++ surface area even for simple drawing operations.

The goal of the rewrite is to simplify the AtomVM port interface and reduce native code by using a single generic call-based protocol.

The Elixir-facing API should be idiomatic and performance-conscious:

```elixir
AtomLGFX.call(port, :draw_line, [0, 0, 120, 80, 0xFFFF])
AtomLGFX.call(port, :fill_rect, [10, 10, 80, 40, 0x07E0])
AtomLGFX.call(port, :set_rotation, [1])
```

Convenience wrappers may be provided on top:

```elixir
AtomLGFX.draw_line(port, 0, 0, 120, 80, 0xFFFF)
AtomLGFX.fill_rect(port, 10, 10, 80, 40, 0x07E0)
AtomLGFX.set_rotation(port, 1)
```

LovyanGFX itself uses C++-style method names such as `drawLine`, `fillRect`, and `setRotation`. These names should not be exposed directly as the normal Elixir API. The Elixir layer should use `snake_case` names and map them to generated numeric operation codes before crossing the AtomVM port boundary.

The boundary is intentionally strict:

- Elixir validates API correctness and protocol policy.
- Native validates crash safety, device reality, and payload ownership.

The Elixir layer is responsible for constructing valid protocol payloads. The native layer should remain intentionally thin.

## Decision

Use a call-based LovyanGFX port protocol.

The Elixir public API uses `snake_case` operation names as atoms:

```elixir
AtomLGFX.call(port, :fill_rect, [10, 10, 80, 40, color])
```

Before sending the request to native code, the Elixir layer maps the operation atom to a numeric operation code.

The native wire protocol uses this generic request shape:

```erlang
{lgfx, ProtocolVersion, call, OpCode, Target, Flags, Args}
```

Example:

```erlang
{lgfx, 2, call, 1, 0, 0, [10, 10, 80, 40, 2016]}
```

In this example, operation code `1` may represent `:fill_rect`.

The Elixir layer owns the authoritative operation schema, argument normalization, public/raw policy, and opcode mapping. The native layer performs minimal defensive checks, validates native-only state, and dispatches to LovyanGFX.

This gives us:

- idiomatic Elixir API names
- compact wire payloads
- efficient native dispatch
- minimal native protocol knowledge
- a single request shape for normal calls and batch execution

## Responsibilities

### Elixir responsibilities

The Elixir layer is responsible for:

- exposing the public API
- owning the authoritative operation schema
- using idiomatic `snake_case` operation names
- deciding operation policy such as public, raw-only, internal-only, tuple-batchable, or binary-only
- normalizing user input
- validating argument count and argument shape
- converting colors into the expected native representation
- constructing operation flags from normalized inputs
- mapping operation names to numeric operation codes
- constructing the port request tuple
- constructing batch payloads
- rejecting invalid public batch usage before it crosses the port boundary
- rejecting unsafe public API usage patterns
- providing friendly errors
- generating or deriving repetitive wrappers and metadata from the shared schema

Example low-level API:

```elixir
def call(port, op_name, args \\ [], opts \\ []) when is_atom(op_name) do
  opcode = Protocol.opcode!(op_name)
  target = Keyword.get(opts, :target, 0)
  flags = Keyword.get(opts, :flags, 0)

  Protocol.call(port, opcode, target, flags, args)
end
```

Example optional wrapper:

```elixir
def draw_line(port, x0, y0, x1, y1, color, opts \\ []) do
  call(port, :draw_line, [x0, y0, x1, y1, color], opts)
end
```

Operation names should be known atoms. The implementation must not create atoms dynamically from user-provided strings.

Good:

```elixir
Protocol.opcode!(:fill_rect)
```

Bad:

```elixir
String.to_atom(user_input)
```

### Native responsibilities

The native layer is responsible for:

- decoding the common request envelope
- checking the protocol marker and version
- checking opcode bounds and target value representability
- safely extracting scalar terms and borrowed binary pointers
- rejecting oversized binaries and malformed request terms
- resolving the target display or sprite
- validating native state such as initialized/not initialized
- validating device reality such as target existence, sprite existence, and payload size against live target state
- enforcing payload ownership and request-lifetime rules
- dispatching by numeric operation code
- calling the corresponding LovyanGFX method
- encoding the reply
- preventing native crashes from malformed payloads

The native layer should not own public API policy, friendly validation, or duplicate the full protocol schema. It should only keep the guardrails needed to avoid undefined behavior and to account for live native state that Elixir cannot know.

Example native dispatch shape:

```cpp
esp_err_t lgfx_dispatch(
    lgfx_context_t *ctx,
    uint16_t opcode,
    uint8_t target,
    uint32_t flags,
    const lgfx_arg_list_t *args,
    lgfx_reply_t *reply)
{
    auto *gfx = lgfx_resolve_target(ctx, target);

    switch (opcode) {
        case LGFX_OP_DRAW_LINE:
            gfx->drawLine(
                args->i32(0),
                args->i32(1),
                args->i32(2),
                args->i32(3),
                args->u16(4));
            return ESP_OK;

        case LGFX_OP_FILL_RECT:
            gfx->fillRect(
                args->i32(0),
                args->i32(1),
                args->i32(2),
                args->i32(3),
                args->u16(4));
            return ESP_OK;

        case LGFX_OP_SET_ROTATION:
            gfx->setRotation(args->u8(0));
            return ESP_OK;

        default:
            return ESP_ERR_NOT_FOUND;
    }
}
```

This is still a native dispatcher, but it is intentionally thin. It does not introduce one handler file or one device wrapper for every LovyanGFX operation.

## Protocol shape

### Direct call

```erlang
{lgfx, 2, call, OpCode, Target, Flags, Args}
```

Fields:

- `lgfx`: protocol marker
- `2`: protocol version
- `call`: request kind
- `OpCode`: numeric operation code generated from a known Elixir operation name
- `Target`: target display or sprite identifier
- `Flags`: operation flags
- `Args`: list of arguments

Example Elixir call:

```elixir
AtomLGFX.call(port, :fill_screen, [0])
```

Example native wire payload:

```erlang
{lgfx, 2, call, 4, 0, 0, [0]}
```

### Batch call

The implemented v2 batch path is a normal call-shaped operation whose argument
is a packed binary scalar command stream.

Example Elixir-side representation:

```elixir
batch =
  IO.iodata_to_binary([
    AtomLGFX.BinaryBatch.fill_screen(0),
    AtomLGFX.BinaryBatch.draw_line(0, 0, 120, 80, 65535),
    AtomLGFX.BinaryBatch.fill_rect(10, 10, 80, 40, 2016)
  ])

AtomLGFX.submit_binary_batch(port, batch, 0)
```

Example native wire payload:

```erlang
{lgfx, 2, call, SubmitBinaryBatchOpCode, 0, 0, [CommandBinary]}
```

The packed command stream reuses ordinary numeric opcodes for the supported
subset, but it is not a general tuple/list batch runtime. Native batch handling
is limited to malformed payload guards, unsupported opcode rejection, device
state validation, payload lifetime, and efficient synchronous command execution.

## Operation naming

The Elixir-facing API uses `snake_case` operation names.

LovyanGFX itself uses C++-style method names such as `drawLine`, `fillRect`, and `setRotation`. These names should not be exposed directly as the normal Elixir API.

Instead, the Elixir layer uses idiomatic operation names such as:

- `:draw_line`
- `:fill_rect`
- `:set_rotation`
- `:set_text_color`
- `:push_image`

These operation names are mapped to generated numeric operation codes before crossing the AtomVM port boundary.

Example:

```elixir
AtomLGFX.call(port, :draw_line, [0, 0, 120, 80, color])
```

is encoded as a numeric operation code and eventually dispatched to:

```cpp
gfx->drawLine(...)
```

This keeps the public API idiomatic for Elixir while preserving a direct mapping to LovyanGFX internally.

## Public API safety policy

The public Elixir API should not expose every LovyanGFX method directly.

Although the native protocol is call-based, the public API must prevent known bad usage patterns. In particular, operations that are too fine-grained for the AtomVM port boundary should not be exposed as normal public helpers.

Per-pixel operations such as `drawPixel` and `writePixel` are intentionally excluded from the main API because they invite inefficient loops from Elixir code.

Avoid exposing public helpers such as:

```elixir
AtomLGFX.draw_pixel(port, x, y, color)
```

This kind of API makes it too easy to write code like:

```elixir
for x <- 0..319, y <- 0..239 do
  AtomLGFX.draw_pixel(port, x, y, color)
end
```

That would cross the AtomVM port boundary once per pixel and is not appropriate for this API.

Repeated small drawing operations should use one of the following instead:

- coarse drawing primitives such as `fill_rect` or `draw_line`
- batch execution
- sprites
- image buffers
- native-side helper operations

The low-level raw API may exist as an explicit escape hatch, but it should be separated from the normal public API.

Example:

```elixir
AtomLGFX.fill_rect(port, 0, 0, 320, 240, color)
```

Preferred.

```elixir
AtomLGFX.Raw.call(port, :draw_pixel, [x, y, color])
```

Allowed only through the raw API, if enabled.

The batch API should also reject unsafe operations by default. A batch containing many single-pixel operations should fail validation and guide the caller toward `push_image` or another buffer-oriented API.

## Performance considerations

The call-based protocol is expected to be acceptable for normal LovyanGFX operations where display I/O dominates total cost. Operations such as `fill_screen`, `fill_rect`, `draw_line`, `draw_string`, and `push_image` usually spend more time in display transfer or LovyanGFX processing than in protocol dispatch.

However, the protocol should not be used as a per-pixel hot path from Elixir. Repeated small operations are expensive because each call crosses the AtomVM port boundary and requires term decoding.

The public Elixir API may use operation names as atoms:

```elixir
AtomLGFX.call(port, :draw_line, [0, 0, 120, 80, 0xFFFF])
```

For native performance, the wire protocol uses generated numeric operation codes instead of strings:

```erlang
{lgfx, 2, call, OpCode, Target, Flags, Args}
```

The Elixir layer is responsible for mapping operation names to operation codes. The native layer can then dispatch with a compact `switch` statement instead of repeated string comparisons.

Packed binary batch execution is the primary control-plane strategy for reducing
call overhead across grouped scalar operations. Repeated homogeneous hot-path
rendering should prefer fixed-layout binary operations, sprites, image buffers,
or native-side presentation helpers instead of growing a tuple/list batch
runtime.

Initial performance rules:

- direct calls are acceptable for coarse drawing operations
- repeated tiny scalar calls should be grouped through packed binary batch or replaced with buffer-oriented operations
- per-pixel loops from Elixir should be avoided by API design
- unsafe pixel-level operations should not be exposed in the main public API
- payload-bearing operations must define clear memory ownership
- generated opcodes are preferred over string dispatch
- repeated homogeneous animation data should prefer fixed-layout binary payloads

## Payload operations

Some operations carry string or binary payloads, for example:

- `draw_string`
- `print`
- `println`
- `push_image`
- image rendering functions

For the first implementation, payload-bearing operations may be allowed for direct calls only.

Batch support for payload-bearing operations should be added only after payload ownership is explicit. The native side must not keep borrowed pointers to AtomVM binary data beyond the lifetime of the current request.

Initial rule:

```text
Direct call:
  payload-bearing operations are allowed.

Batch call:
  payload-bearing operations are rejected unless the payload is copied into runtime-owned memory.
```

## Raw API

A raw API may be provided as an explicit escape hatch.

Example:

```elixir
AtomLGFX.Raw.call(port, :draw_pixel, [x, y, color])
```

The raw API is not the normal application API. It exists for experimentation, debugging, and advanced use cases.

The raw API may expose operations that are intentionally absent from the main API, but it should make the tradeoff obvious through module naming, documentation, and validation behavior.

The raw API may be disabled or omitted in minimal builds.

## Code generation

The preferred long-term implementation is to keep the operation schema in one source of truth and generate repetitive code from it.

Example source:

```elixir
@ops [
  fill_screen: [
    opcode: 1,
    native: :fillScreen,
    c_function: :lgfx_device_fill_screen,
    args: [:rgb565],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: true
  ],
  draw_line: [
    opcode: 2,
    native: :drawLine,
    c_function: :lgfx_device_draw_line,
    args: [:i16, :i16, :i16, :i16, :rgb565],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: true
  ],
  fill_rect: [
    opcode: 3,
    native: :fillRect,
    c_function: :lgfx_device_fill_rect,
    args: [:i16, :i16, :u16, :u16, :rgb565],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: true
  ],
  set_rotation: [
    opcode: 4,
    native: :setRotation,
    c_function: :lgfx_device_set_rotation,
    args: [:u8],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: false
  ],
  draw_pixel: [
    opcode: 100,
    native: :drawPixel,
    c_function: :lgfx_device_draw_pixel,
    args: [:i16, :i16, :rgb565],
    public: false,
    raw: true,
    direct: true,
    tuple_batchable: false
  ]
]
```

Generated outputs may include:

```text
lib/atom_lgfx/generated/ops.ex
lib/atom_lgfx/generated/wrappers.ex
lgfx_port/generated/opcodes.h
lgfx_port/generated/simple_dispatch.cpp
```

Simple scalar operations should be generated or collapsed into mechanical dispatch. Handwritten native code should remain for stateful or payload-bearing operations such as lifecycle, sprites, images, text payloads, batch runtime, and native presentation helpers.

This keeps the handwritten native code focused on runtime concerns instead of API surface plumbing.

## Consequences

### Positive

- The AtomVM port interface becomes simpler.
- The native C/C++ surface area is reduced.
- The Elixir layer becomes the protocol authority.
- The public Elixir API remains idiomatic.
- Operation names use `snake_case` in Elixir.
- The native wire protocol remains compact by using numeric operation codes.
- Native dispatch can use `switch` instead of string comparison.
- Adding a LovyanGFX operation requires less native boilerplate.
- Simple scalar direct calls and tuple-batch commands can be generated from the same schema.
- Public Elixir wrappers can remain friendly without forcing native API duplication.
- Known performance foot-guns can be banned from the main API.
- The implementation stays close to LovyanGFX instead of creating a second graphics abstraction.

### Negative

- The native side still needs a dispatcher because C++ methods cannot be invoked dynamically by name.
- The operation code mapping must be generated or maintained carefully.
- Invalid payloads may produce lower-level native errors if Elixir validation is bypassed.
- Some LovyanGFX overloads need explicit protocol choices.
- Payload ownership must be handled carefully for strings, binaries, and image buffers.
- Code generation may become necessary as the supported LovyanGFX API surface grows.
- Until generation is in place, operation metadata can still drift across Elixir and native code.
- The raw API can still be misused if exposed without care.

### Neutral

- This design intentionally favors a thin bridge over a fully typed native binding.
- Native validation is defensive, not authoritative.
- The protocol is closer to a remote procedure call interface than a traditional Elixir wrapper.
- The public API is curated and does not mirror the full LovyanGFX API one-to-one.

## Alternatives considered

### One native function per LovyanGFX operation

This is the approach used by the earlier implementation.

It is explicit and easy to reason about for a small number of operations, but it creates too much repeated native code as the API surface grows.

Rejected because the rewrite aims to reduce native boilerplate.

### String-based operation names on the wire

The wire protocol could send operation names as strings or atoms:

```erlang
{lgfx, 2, call, fillRect, 0, 0, [10, 10, 80, 40, 2016]}
```

This is readable, but it pushes name handling closer to native code and encourages string or atom comparisons in the dispatcher.

Rejected because the operation set is fixed and known. Numeric operation codes are more compact and easier to dispatch efficiently.

### LovyanGFX camelCase names in Elixir

The Elixir API could mirror LovyanGFX names directly:

```elixir
AtomLGFX.call(port, :fillRect, [10, 10, 80, 40, color])
```

This makes the mapping to LovyanGFX obvious, but it exposes C++ naming style in Elixir.

Rejected because the public Elixir API should use idiomatic `snake_case` names.

### Fully typed native operation registry

A typed native registry can describe every operation, argument type, target policy, and reply type.

This is safer, but it duplicates protocol knowledge that already belongs in the Elixir layer.

Rejected for the initial rewrite because it keeps too much schema responsibility in C/C++.

### Raw dynamic C++ method invocation

Calling arbitrary LovyanGFX methods dynamically by name would be ideal in theory, but C++ does not provide practical runtime method reflection for this use case.

Rejected as an implementation strategy.

The accepted design is a raw dynamic protocol bridge with a thin handwritten or generated dispatcher.

### Exposing every LovyanGFX method publicly

The public API could expose every supported LovyanGFX method as a normal Elixir helper.

Rejected because some operations are inappropriate across the AtomVM port boundary. Per-pixel operations and write-transaction-style APIs can easily lead to poor performance when called repeatedly from Elixir.

The accepted design is a curated public API with an optional raw escape hatch.

## Initial v2 scope

The first implementation should support a small but useful subset of LovyanGFX operations.

Main public API:

- `init`
- `close`
- `width`
- `height`
- `set_rotation`
- `set_brightness`
- `fill_screen`
- `draw_line`
- `draw_rect`
- `fill_rect`
- `draw_circle`
- `fill_circle`
- `set_cursor`
- `set_text_color`
- `set_text_size`
- `draw_string`
- `push_image`
- `batch`

Raw or internal-only API:

- `draw_pixel`
- `write_pixel`
- `write_color`
- `write_fast_hline`
- `write_fast_vline`
- `start_write`
- `end_write`

The success criterion is not the number of supported methods. The success criterion is that adding one more LovyanGFX method requires only a small dispatch addition or a generated operation entry.

## Decision summary

Use one call-based AtomVM port protocol:

```erlang
{lgfx, 2, call, OpCode, Target, Flags, Args}
```

The Elixir public API uses `snake_case` operation atoms.

The Elixir layer maps operation atoms to generated numeric operation codes.

The native layer remains a thin bridge that decodes the common envelope, resolves the target, dispatches by operation code, calls LovyanGFX, and returns a reply.

The preferred implementation direction is one operation schema that drives both Elixir metadata and generated native simple dispatch. Handwritten native code remains reserved for stateful, payload-bearing, or presentation-oriented operations.

The main public API is intentionally curated. It should not expose known performance foot-guns such as per-pixel operations. Those operations may exist only in a clearly separated raw API, if needed.

This provides the simplicity of a raw dynamic protocol while avoiding the impracticality of raw dynamic C++ method invocation and the performance cost of string-based native dispatch.
