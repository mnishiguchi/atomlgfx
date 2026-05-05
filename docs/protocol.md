<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# LovyanGFX AtomVM Port Protocol

This document defines the tuple protocol between an AtomVM host application and the native `lgfx_port` driver.

See [the architecture overview](architecture.md) for the repository map,
[the `lgfx_port` README](../lgfx_port/README.md) for port-layer details,
and [the protocol reference](protocol-reference.md) for generated operation,
capability, and error tables.

## Scope

This document covers:

- request and response shapes
- protocol-visible validation rules
- data encodings
- flags and capability bits
- operation semantics that are part of the external contract

This document does not define:

- open-time config passed through `open_port/2`
- internal port-layer or device-layer implementation details
- non-contract execution structure inside the native component
- maintainer-only testing or sync checklists

## Source of truth

The protocol contract is defined by these sources:

- `lgfx_port/include_internal/lgfx_port/ops.def`
  - operation surface
  - numeric opcode order
  - arity
  - allowed flags
  - target policy
  - state policy
  - capability linkage

- `lgfx_port/include_internal/lgfx_port/protocol.h`
  - protocol constants
  - capability bits
  - wire-level limits

- build-generated `lgfx_port/lgfx_port_config.h`
  - generated from `lgfx_port/cmake/lgfx_port_config.h.in`
  - build-derived gates used by the component

- this document
  - human-readable contract

- `docs/protocol-reference.md`
  - generated reference tables synchronized from source metadata

- `lib/atom_lgfx/generated.ex`
  - generated Elixir operation-name to opcode mapping
  - generated public/raw/batch exposure metadata
  - generated arity, flag, target, state, and capability metadata consumed by `AtomLGFX.OpSchema`

Important invariants:

- if an op is not declared in `ops.def`, it is not part of the protocol
- `getCaps` derives `FeatureBits` from metadata plus the active dispatch surface
- `FeatureBits` contains protocol bits only
- touch is advertised only when touch ops are both compiled in and effectively attached
- generated reference tables and implementation must agree
- canonical Elixir operation names are `snake_case` atoms
- LovyanGFX-style `camelCase` atoms are not part of the Elixir-facing protocol
- existing numeric opcode order is stable within one protocol version
- new operations must be appended unless the protocol version is bumped

## Operation naming contract

Protocol operations have one Elixir-facing name:

- canonical Elixir name
  - `snake_case` atom
  - used by `AtomLGFX.OpSchema`, `AtomLGFX.Protocol`, public wrapper code, and raw calls

`ops.def` may keep LovyanGFX-style identifiers internally for handler-table generation, but the request tuple carries a numeric opcode. Callers do not pass those internal identifiers over the Elixir-facing API.

Rules:

- public and raw Elixir-facing calls must use canonical `snake_case` names
- LovyanGFX-style `camelCase` atoms such as `:fillRect` are unknown operations
- opcode values are derived only from `ops.def` order
- generated files must be refreshed with `elixir scripts/sync_lgfx_protocol_doc.exs`
- generated drift must be caught by `mix lgfx.generated.check`
- protocol freeze checks must pass before changing operation names or opcode order

## Request and response model

All requests use one tuple shape:

```erlang
{lgfx, ProtoVer, call, OpCode, Target, Flags, Args}
```

Field meanings:

- `lgfx`
  - tag atom

- `ProtoVer`
  - integer
  - must equal `LGFX_PORT_PROTO_VER`

- `call`
  - request kind atom

- `OpCode`
  - numeric operation code generated from a known Elixir operation atom

- `Target`
  - `0` => LCD
  - `1..254` => sprite handle
  - `255` => reserved and invalid

- `Flags`
  - integer bitset
  - `0` when unused

- `Args`
  - proper list of operation arguments
  - empty list when unused

Responses are always:

```erlang
{ok, Result}
{error, Reason}
```

Conventions:

- void operations return `{ok, ok}`
- getters return `{ok, Value}`
- structured returns use tuples
- `Reason` is an atom or detail tuple

## Execution model at the protocol boundary

The protocol exposes two operation styles:

- ordinary operations
  - execute immediately
  - return real success or failure immediately

- binary batch submission through `submitBinaryBatch`
  - command construction is explicit and caller-owned
  - one binary contains one frame script
  - execution is synchronous
  - success means the script was fully decoded and executed

This distinction is part of the external contract for `AtomLGFX.submit_binary_batch/2`.
Internal execution timing and runtime structure are implementation details.

## Validation model

Common failure mapping:

- wrong tuple tag or protocol version => `bad_proto`
- unknown opcode => `bad_op`
- wrong arity or types => `bad_args`
- value out of wire range => `bad_args`
- invalid target => `bad_target`
- invalid non-zero flags => `bad_flags`

Validation is layered:

- port-level validation handles request envelope and op metadata
- handlers perform op-specific wire decode
- device-layer code is authoritative for device-facing semantics

Examples of device-facing semantic checks:

- source or destination sprite existence
- palette-backed sprite requirements for indexed scalar colors
- `push_image` stride normalization and required byte count
- `drawJpg` decode and render behavior
- rotate and zoom semantic validity
- deterministic sprite allocation rules

Binary-batch submission adds one more validation layer:

- `submit_binary_batch` validates the outer request envelope
- each render command is decoded and validated by the binary batch decoder before execution

## Binary payload lifetime

Raw pointers into caller binaries are request-scoped.

Rule:

- the driver must not retain pointers into caller binaries past the request boundary unless lifetime is explicitly managed

Current ordinary-operation model:

- handlers borrow request binary pointers and pass them directly to `lgfx_device_*` within the same request
- text and image device calls are synchronous in the current design
- device code must fully consume those bytes before returning and must not retain the pointer after the call

That matters especially for `draw_string`, `print`, `println`, `draw_jpg`, and `push_image`.

For explicit batching:

- `submitBinaryBatch` borrows its binary only for the synchronous request boundary
- the render decoder must not retain pointers into the caller binary
- payload-bearing binary-batch commands are decoded and consumed synchronously
  within the render path
- callers should not assume that any payload-bearing ordinary op is automatically
  batchable; only documented `AtomLGFX.BinaryBatch` builders are in scope

## Common data and encodings

### Integer ranges

Driver-enforced ranges:

- `i16`: `-32768..32767`
- `i32`: `-2147483648..2147483647`
- `u16`: `0..65535`
- `u32`: `0..4294967295`

Common usage:

- `x`, `y` => `i16`
- `w`, `h` => `u16`

### LovyanGFX-like numeric values

Some numeric arguments use LovyanGFX-like float semantics on the wire.

Current paths:

- `setTextSize`
- `drawJpg` extended scaling
- `pushRotateZoom`

Rules:

- integer and float terms are both accepted on the wire
- values are decoded to native `float` in the handler layer
- values must be finite
- scale values must be positive

Examples:

- `1` => `1.0`
- `1.5` => `1.5`
- `90` => `90.0`

### Strings

- text arguments are UTF-8 binaries
- no trailing NUL is required
- embedded NUL may be rejected for ops that call C-string APIs

### Colors

This protocol distinguishes four related color domains:

- display colors used by primitive and text operations
- palette lifecycle colors
- palette indices
- `push_image` pixel blobs

#### Display colors used by primitive, text, and non-index transparent sprite operations

Non-index display colors use RGB565 on the wire.

- wire format is RGB565 in `u16`
- the handler forwards that value as the display color used by the device-facing primitive or text path
- this contract is the same regardless of target color depth
- `setColorDepth(Target, 24)` changes the destination target depth but does not change the display-color wire format
- `setColorDepth(Target, 24)` does not by itself imply palette-index semantics

Indexed palette mode:

- enabled only by op-specific flags
- the corresponding scalar argument is interpreted as a palette index
- the palette index is carried in the low 8 bits of the decoded scalar value
- indexed mode is invalid on LCD target for primitive and text color arguments
- indexed mode on a sprite target requires actual palette backing
- target color depth alone does not implicitly enable indexed semantics

This applies to non-index display-color arguments used by operations such as:

- `fillScreen`
- `clear`
- `drawPixel`
- `drawFastVLine`
- `drawFastHLine`
- `drawLine`
- `drawRect`
- `fillRect`
- `drawRoundRect`
- `fillRoundRect`
- `drawCircle`
- `fillCircle`
- `drawEllipse`
- `fillEllipse`
- `drawArc`
- `fillArc`
- `drawBezier`
- `drawTriangle`
- `fillTriangle`
- `setTextColor`
- `pushSprite` optional transparent value
- `pushRotateZoom` optional transparent value

#### Palette lifecycle colors

Palette lifecycle operations use RGB888 directly on the wire.

- `setPaletteColor` takes `0x00RRGGBB` packed RGB888 in `u32`
- palette lifecycle arguments are not reinterpreted as RGB565 display colors
- `createPalette` establishes palette backing for an existing paletted sprite target
- `setPaletteColor` writes one palette entry on that palette-backed sprite

#### Palette indices

Palette indices are explicit, flag-selected argument interpretations.

- indexed primitive color uses `LGFX_F_COLOR_INDEX`
- indexed text foreground uses `LGFX_F_TEXT_FG_INDEX`
- indexed text background uses `LGFX_F_TEXT_BG_INDEX`
- indexed transparent sprite color uses `LGFX_F_TRANSPARENT_INDEX`
- indexed semantics require actual palette backing where documented
- indexed semantics are never implied by color depth alone

#### `push_image` pixel blobs

- RGB565 only
- little-endian per pixel (`lo hi`) as ordinary 16-bit RGB565 words
- unaffected by `setColorDepth`
- target-side byte swapping remains controlled separately by `setSwapBytes`

## Error reasons

Canonical protocol error atoms and detail tags are listed in
[the generated error reference](protocol-reference.md#generated-error-reasons).

Optional detail forms:

- `{error, {bad_args, Detail}}`
- `{error, {internal, EspErr}}`

Client rule:

- match `{error, Reason}` and treat `Reason` as opaque

## Operation policy notation

This notation mirrors `ops.def`.

### Target rule

- `T0/bad_target`
  - require `Target == 0`, else `{error, bad_target}`

- `T0/unsupported`
  - require `Target == 0`, else `{error, unsupported}`

- `LGFX_OP_TARGET_ANY`
  - accept LCD or sprite targets
  - `255` remains invalid

- `LGFX_OP_TARGET_SPRITE_ONLY`
  - require sprite target `1..254`

### Flags rule

- `F0`
  - require `Flags == 0`

- `Fmask(X)`
  - require `(Flags & ~X) == 0`

### State rule

- `any`
  - callable before `init`

- `requires_init`
  - requires initialized display state

## Implemented operation matrix

The generated implemented operation matrix lives in
[the protocol reference](protocol-reference.md#implemented-operation-matrix).

If an operation is not listed there, it is not implemented and must return `{error, bad_op}`.

## Capabilities

### `getCaps()`

Request:

- `getCaps()` with `Target == 0`

Response:

```erlang
{ok, {caps, ProtoVer, MaxBinaryBytes, MaxSprites, FeatureBits}}
```

Fields:

- `ProtoVer`
  - protocol version returned by the driver

- `MaxBinaryBytes`
  - maximum accepted size for any binary argument

- `MaxSprites`
  - maximum concurrently allocated sprites
  - must be `0` if sprite support is absent

- `FeatureBits`
  - protocol feature bitset only

Derivation rules:

- start from `0`
- walk operations declared in `ops.def`
- if an op has a non-zero `feature_cap_bit` and is enabled in the built dispatch surface, OR that bit into `FeatureBits`
- apply real build and runtime gates
- mask to known protocol bits before returning

Generated capability vocabulary is listed in
[the protocol reference](protocol-reference.md#generated-capability-vocabulary).

Meaning:

- `CAP_SPRITE`
  - sprite operations are available

- `CAP_PUSHIMAGE`
  - `push_image` is available

- `CAP_LAST_ERROR`
  - `getLastError` is available

- `CAP_TOUCH`
  - touch operations are available

- `CAP_PALETTE`
  - palette lifecycle operations are available
  - specifically `createPalette` and `setPaletteColor`

- `CAP_BATCH`
  - binary batch submission is available
  - specifically `submitBinaryBatch` / `AtomLGFX.submit_binary_batch/2`

Touch note:

- `CAP_TOUCH` is advertised only when touch support is enabled in the build and touch is attached
- compiling touch support with `LGFX_PORT_TOUCH_CS_GPIO = -1` keeps touch unattached and unadvertised

## Render batching

`submitBinaryBatch` is the v2 batch submission path.

It is not a scheduler, queue, or general tuple/list batch runtime. It is an explicit binary frame-script entry point for hot rendering work.

Elixir callers should build frame scripts with `AtomLGFX.BinaryBatch` and submit them with `AtomLGFX.submit_binary_batch/2` or `AtomLGFX.BinaryBatch.render/2`.

For generated or experimental frame scripts, `AtomLGFX.BinaryBatch.validate/1` can preflight the stream without calling native code, and `AtomLGFX.BinaryBatch.render_checked/2` validates before submitting. This gives callers an opt-in no-partial-render safety path when native `LGFX_PORT_RENDER_BATCH_PREVALIDATE` is disabled. `AtomLGFX.BinaryBatch.summary/1` reports diagnostic counts such as batch bytes, render-private command count, dynamic payload bytes, fixed overhead bytes, packed-list record bytes, packed-list command count, packed-list instance count, and integer x1000 wire-efficiency ratios without calling native code. `AtomLGFX.BinaryBatch.diagnose/1` returns the same summary information for valid streams and partial context for invalid streams, including failing command index, opcode, best-effort operation name, and last successfully decoded command. `AtomLGFX.BinaryBatch.compare/2` compares a baseline frame script with a candidate frame script using the same summary metrics, which is useful when replacing repeated scalar commands with compact list commands. `AtomLGFX.BinaryBatch.check_budget/2` validates the same diagnostic metrics against caller-provided limits, which is useful for CI and generated-frame guardrails.

### Wire shape

It uses the normal protocol request envelope:

```erlang
{lgfx, ProtoVer, call, SubmitBinaryBatchOpCode, 0, 0, [CommandBinary]}
```

Rules:

- `Flags` must be `0`
- `CommandBinary` must be non-empty
- `CommandBinary` must not exceed `LGFX_PORT_MAX_BINARY_BYTES`
- target and color interpretation are command-local through binary-batch state
- execution is synchronous and stops at the first malformed or failed command
- success returns `{ok, ok}`
- failure returns `{error, Reason}`

Each command is:

```text
opcode u8 + opcode-specific payload
```

Malformed render commands return `bad_args`. Unsupported render command opcodes return `bad_op`.

### Command-local render state

Binary render batches keep small command-local state while the frame script executes:

- current target
  - selected by `target`
  - defaults to LCD target `0`

- current color mode
  - selected by `colorMode`
  - RGB565 mode interprets scalar color fields as RGB565
  - palette-index mode interprets scalar color fields as palette indices where supported

- native presentation strip state
  - selected by `beginStrip` / `presentStrip`
  - when a strip is active, logical target `0` resolves to the active native strip

### Representative command layouts

```text
target:
  op u8
  target u8

colorMode:
  op u8
  mode u8

beginStrip:
  op u8
  y0 u16le

presentStrip:
  op u8

fillScreen / clear:
  op u8
  color u16le

fillRect:
  op u8
  x i16le
  y i16le
  w u16le
  h u16le
  color u16le

drawString:
  op u8
  x i16le
  y i16le
  byte_len u16le
  utf8 bytes[byte_len]

pushSprite:
  op u8
  source_target u8
  x i16le
  y i16le

pushSpriteList:
  op u8
  flags u16le
  transparent u16le
  instance_count u16le
  InstanceRecord instance_count * 6 bytes

PushSpriteList InstanceRecord:
  source_target u8
  reserved u8 = 0
  x i16le
  y i16le

pushSpriteRegionList:
  op u8
  flags u16le
  transparent u16le
  instance_count u16le
  InstanceRecord instance_count * 14 bytes

PushSpriteRegionList InstanceRecord:
  source_target u8
  reserved u8 = 0
  src_x u16le
  src_y u16le
  width u16le
  height u16le
  dst_x i16le
  dst_y i16le

pushRotateZoomList:
  normal operation opcode with one PRZL payload binary

pushRotateZoomFrameStrips:
  extended op u8 = 0xFF
  subop u8 = 0x01
  flags u16le
  PRZF payload

display:
  op u8
```

The generated protocol reference remains the source for numeric operation codes. Render-private command opcodes are internal to the binary render-batch command stream.

### Native transformed-sprite frame command

`pushRotateZoomFrameStrips` is an extended render-private command for hot animation loops that repeatedly draw transformed source sprites through native presentation strips. It is a frame-level command: native code owns the strip loop, strip clearing, strip presentation, and final transformed sprite draw calls. Elixir still owns object state and builds the frame-state payload.

Wire layout:

```text
extended opcode:
  op u8 = 0xFF
  subop u8 = 0x01
  flags u16le

PRZF payload:
  magic bytes[4] = "PRZF"
  version u8 = 1
  options u8
  transparent u16le
  frame_height u16le
  background u16le
  instance_count u16le
  InstanceRecord instance_count * 12 bytes

InstanceRecord:
  source_target u8
  reserved u8 = 0
  x i16le
  y i16le
  angle_cdeg u16le
  zoom_x1024 u16le
  zoom_y1024 u16le
```

Rules:

- `options & 0x01` means `transparent` is present
- `options & 0x02` requests approximate native culling
- unknown option bits are invalid
- `flags` may only use `LGFX_F_TRANSPARENT_INDEX`
- if `LGFX_F_TRANSPARENT_INDEX` is set, `transparent` is a palette index in the low byte
- `LGFX_F_TRANSPARENT_INDEX` without the transparent option is invalid
- `frame_height` must be greater than `0`
- `instance_count` must be greater than `0`
- `source_target` must be a sprite target
- `reserved` must be `0`
- `angle_cdeg` must be `0..35999`
- `zoom_x1024` and `zoom_y1024` must be positive fixed-point scales where `1024 == 1.0x`
- this command must not be nested inside an active `beginStrip` / `presentStrip` section

### Native presentation strips

`beginStrip` starts drawing into the native presentation strip at LCD y-coordinate `y0`.

While a strip is active:

- `target(0)` resolves to the active strip buffer for drawing
- sprite targets still resolve normally
- callers should clear or fully redraw the strip contents before presenting

`presentStrip` presents the active strip to the live LCD.

The Elixir strip loop must use the native-negotiated strip height. The native presentation layer may allocate a smaller strip than the preferred height when memory is constrained.

## Diagnostics

### `getLastError()`

Request:

- `getLastError()` with `Target == 0`

Response:

```erlang
{ok, {last_error, LastOp, Reason, LastFlags, LastTarget, EspErr}}
```

Fields:

- `LastOp`
  - last failing op atom, or `none`

- `Reason`
  - last error reason, or `none`

- `LastFlags`
  - flags from the failing request

- `LastTarget`
  - target from the failing request

- `EspErr`
  - `esp_err_t` integer, or `0`

Behavior:

- the driver snapshots the last-error state and returns it
- on success, the last-error state is cleared after the response payload is encoded

Batch note:

- there is currently no dedicated protocol operation in the implemented surface for batch status lookup
- `submit_binary_batch` is synchronous; no later batch-status polling is required

## Fonts

Stable protocol-owned font selection is exposed through:

- `setTextFontPreset(PresetIdU8)`

Font preset and text size are separate concerns:

- `setTextFontPreset` chooses the glyph source
- `setTextSize` controls the rendered size
- selecting a preset normalizes text scale to `1.0x`

### `setTextSize`

Args:

- `setTextSize(ScaleF32)`
- `setTextSize(ScaleXF32, ScaleYF32)`

Rules:

- integer and float terms are both accepted on the wire
- one-argument form applies the same scale to both axes
- two-argument form sets both axes explicitly
- scale values must be positive
- handler decode normalizes the wire value to native `float`
- device code validates and forwards the final value to the pinned LovyanGFX call surface

Errors:

- zero, negative, non-finite, or wrong-type value => `{error, bad_args}`

### `setTextDatum`

Args:

- `setTextDatum(DatumU8)`

Rules:

- `DatumU8` must be an integer in `0..255`
- forwarded as a raw numeric passthrough to the pinned LovyanGFX text-datum API

Errors:

- out-of-range value => `{error, bad_args}`

### `setTextWrap`

Args:

- `setTextWrap(WrapXBool)`
- `setTextWrap(WrapXBool, WrapYBool)`

Rules:

- booleans are accepted as atom `true` / `false`
- numeric `0` / `1` are also accepted by the handler decode path
- one-argument form means `wrap_x = WrapXBool`, `wrap_y = false`
- two-argument form sets both axes explicitly

### `setTextFontPreset`

Preset IDs:

- `0` = `ascii`
  - selects the pinned default ASCII font internally
  - normalizes text scale to `1.0`

- `1` = `jp`
  - selects the built-in Japanese-capable preset internally
  - normalizes text scale to `1.0`

Errors:

- unknown preset => `{error, bad_args}`
- preset compiled out => `{error, unsupported}`

## Flags

`Flags` is op-specific unless documented otherwise.

Defined protocol flags:

- `LGFX_F_TEXT_HAS_BG = 1 bsl 0`
  - `setTextColor` includes a background scalar argument

- `LGFX_F_COLOR_INDEX = 1 bsl 1`
  - primitive op color argument is interpreted as a palette index instead of a non-index display color

- `LGFX_F_TEXT_FG_INDEX = 1 bsl 2`
  - `setTextColor` foreground scalar argument is interpreted as a palette index

- `LGFX_F_TEXT_BG_INDEX = 1 bsl 3`
  - `setTextColor` background scalar argument is interpreted as a palette index

- `LGFX_F_TRANSPARENT_INDEX = 1 bsl 4`
  - `pushSprite` or `pushRotateZoom` transparent scalar argument is interpreted as a palette index

General rules:

- a flag is valid only for operations whose `ops.def` mask allows it
- indexed color flags select argument interpretation; they do not create palette backing
- `LGFX_F_TEXT_BG_INDEX` is invalid unless `LGFX_F_TEXT_HAS_BG` is also set

### `setTextColor`

Args:

- `setTextColor(FgColor)`
- `setTextColor(FgColor, BgColor)` when `LGFX_F_TEXT_HAS_BG` is set

Semantics:

- foreground and background scalar colors independently support non-index display-color mode or indexed palette mode
- foreground indexed mode is selected by `LGFX_F_TEXT_FG_INDEX`
- background indexed mode is selected by `LGFX_F_TEXT_BG_INDEX`
- background presence is selected by `LGFX_F_TEXT_HAS_BG`
- indexed scalar mode on LCD is invalid
- indexed scalar mode on a sprite target requires a palette-backed sprite target

## Important op semantics

### `setColorDepth`

Args:

- `setColorDepth(DepthU8)`

Allowed values:

- `1`
- `2`
- `4`
- `8`
- `16`
- `24`

Semantics:

- changes the destination target color depth
- does not change the wire format used by non-index display colors
- does not by itself enable indexed scalar-color semantics
- does not by itself create palette backing for a sprite
- `push_image` remains RGB565-only regardless of target color depth

### `drawJpg`

Request args:

- `drawJpg(Xi16, Yi16, JpegBinary)`
- `drawJpg(Xi16, Yi16, MaxWu16, MaxHu16, OffXi16, OffYi16, ScaleXF32, ScaleYF32, JpegBinary)`

Rules:

- the final argument must be a binary
- the short form implies `MaxW = 0`, `MaxH = 0`, `OffX = 0`, `OffY = 0`, `ScaleX = 1.0`, `ScaleY = 1.0`
- integer and float terms are both accepted for extended-form scale values
- extended-form scale values must be finite and positive
- the selected target may be LCD `0` or sprite `1..254`

### `push_image`

Request args:

- `push_image(Xi16, Yi16, Wu16, Hu16, StridePixelsU16, DataRgb565Binary)`

Rules:

- `W > 0`
- `H > 0`
- the final argument must be a binary
- if `StridePixelsU16 == 0`, effective stride becomes `W`
- effective stride must be `>= W`
- payload byte size must be even
- payload must be large enough for the requested image
- trailing bytes beyond the required minimum are ignored

### `createSprite`

Request-header `Target` is the sprite handle to allocate:

- `1..254` => candidate sprite handle
- `0` => invalid

Args:

- `createSprite(Wu16, Hu16)`
- `createSprite(Wu16, Hu16, ColorDepthU8)`

Rules:

- allocation happens at the requested handle
- `W` and `H` must be non-zero
- optional color depth must be valid when provided
- creation fails if the handle is already in use
- creation fails if the configured maximum concurrent sprite count is exhausted
- paletted depths are `1`, `2`, `4`, and `8`
- true-color depths are `16` and `24`
- paletted depth alone does not create palette backing

### `createPalette` and `setPaletteColor`

These operations manage palette backing for a sprite target.

Request-header `Target` is the sprite handle:

- `1..254` => candidate sprite handle
- `0` => invalid

`createPalette` args:

- none

`setPaletteColor` args:

- `setPaletteColor(PaletteIndexU8, Rgb888U32)`

Rules:

- both operations are sprite-only
- the target sprite must already exist
- `createPalette` requires paletted depth `1`, `2`, `4`, or `8`
- `createPalette` establishes palette backing for that sprite
- indexed scalar-color semantics require actual palette backing
- `setPaletteColor` requires an existing palette-backed sprite
- `Rgb888U32` uses `0x00RRGGBB`
- valid palette index range depends on sprite depth

### `pushSprite`

This is a destination-aware whole-sprite blit.

Request-header `Target` is the source sprite handle:

- `1..254` => valid source sprite domain
- `0` => invalid

Args:

- `pushSprite(DstTargetU8, DstXi16, DstYi16)`
- `pushSprite(DstTargetU8, DstXi16, DstYi16, TransparentValue)`

Rules:

- `DstTargetU8 == 0` => LCD destination
- `DstTargetU8 in 1..254` => destination sprite
- source and destination existence are resolved in the device layer
- optional transparent scalar uses the non-index display-color contract by default
- `LGFX_F_TRANSPARENT_INDEX` interprets the transparent scalar as a palette index
- indexed transparent mode requires palette backing on the source sprite
- edge clipping is allowed

There is no region-based sprite blit op in this protocol.

### `pushRotateZoom`

This draws a source sprite to a destination target with rotation and scaling.

Request-header `Target` is the source sprite handle.

Args:

- `pushRotateZoom(DstTargetU8, DstXi16, DstYi16, AngleDegF32, ZoomXF32, ZoomYF32)`
- `pushRotateZoom(DstTargetU8, DstXi16, DstYi16, AngleDegF32, ZoomXF32, ZoomYF32, TransparentValue)`

Rules:

- destination target rules are the same as `pushSprite`
- rotation uses the source sprite pivot set by `setPivot`
- source and destination existence rules are resolved in the device layer
- integer and float terms are both accepted for angle and zoom values
- angle and zoom values must be finite
- zoom values must be positive
- optional transparent scalar uses the non-index display-color contract by default
- `LGFX_F_TRANSPARENT_INDEX` interprets the transparent scalar as a palette index
- indexed transparent mode requires palette backing on the source sprite
- edge clipping is allowed

### `pushRotateZoomList`

This is the compact binary hot path for many transformed sprite blits to one
destination target.

Request-header `Target` is the destination target:

- `0` => LCD destination
- `1..254` => destination sprite

Args:

- `pushRotateZoomList(PayloadBinary)`

Payload layout is little-endian:

```text
magic             bytes[4] = "PRZL"
version           u8 = 1
options           u8
transparent       u16
y_offset          i16
instance_count    u16
InstanceRecord    instance_count * 12 bytes

InstanceRecord:
  src_target      u8
  reserved        u8 = 0
  x               i16
  y               i16
  angle_cdeg      u16
  zoom_x1024      u16
  zoom_y1024      u16
```

Rules:

- `options & 0x01` means `transparent` is present
- unknown option bits are invalid
- if `LGFX_F_TRANSPARENT_INDEX` is set, `transparent` is a palette index in the low byte
- `LGFX_F_TRANSPARENT_INDEX` without the transparent option is invalid
- `x` and `y` are destination coordinates before native `y_offset` adjustment
- native code subtracts `y_offset` from each instance `y`
- `angle_cdeg` is centidegrees and must be `0..35999`
- `zoom_x1024` and `zoom_y1024` are positive fixed-point scales where `1024 == 1.0x`
- `reserved` must be `0`
- payload length must exactly match `12 + instance_count * 12`
- source and destination existence rules are resolved in the device layer

### `presentationStripHeight`

This reports the current native presentation strip height used by the device layer.

Request args:

- `presentationStripHeight()`

Rules:

- target must be LCD target `0`
- return value is a non-negative integer
- `0` means native strip presentation is unavailable
- positive values should be used by Elixir strip loops instead of hard-coded strip heights

This operation exists so application-level strip orchestration can match the actual native allocation after adaptive strip-buffer fallback.

## Compatibility rules

Treat these changes as protocol-affecting and bump `LGFX_PORT_PROTO_VER` when they occur:

- changing request tuple shape
- changing response shape
- changing operation meaning
- changing numeric opcode order for existing operations
- changing argument order
- changing argument interpretation
- changing flag meaning
- changing accepted wire encoding
- changing canonical error reason for an existing contract violation
- removing an implemented op from the protocol surface

Changes that normally do not require a protocol bump:

- internal refactors that preserve the external contract
- implementation changes behind an unchanged request and response surface
- documentation clarifications that do not alter semantics
- adding new operations guarded by normal capability discovery
- adding new internal detail while preserving existing opaque error matching
- `lib/atom_lgfx/generated.ex`
  - Elixir `snake_case` operation names
  - Elixir operation-name to opcode mapping
  - public/raw/batch exposure policy
