<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# LovyanGFX AtomVM Port Protocol

This document defines the tuple protocol between an AtomVM host application and the native `lgfx_port` driver.

See [the architecture overview](architecture.md) for the repository map and [the `lgfx_port` README](../lgfx_port/README.md) for port-layer and ownership details.

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
- non-contract implementation structure

## Source of truth

The protocol contract is defined by these sources:

- `lgfx_port/include_internal/lgfx_port/ops.def`
  - operation surface
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
  - generated tables synchronized from source metadata

Important invariants:

- if an op is not declared in `ops.def`, it is not part of the protocol
- `getCaps` derives `FeatureBits` from metadata plus the active dispatch surface
- `FeatureBits` contains protocol bits only
- touch is advertised only when touch ops are both compiled in and effectively attached
- generated tables and implementation must agree

## Protocol version history

### v2

`LGFX_PORT_PROTO_VER = 2`.

This version introduces a wire-level breaking change for rotate and scale semantics so they align more directly with LovyanGFX numeric behavior.

Affected paths:

- `setTextSize`
- `drawJpg` extended scaling
- `pushRotateZoom`

What changed:

- fixed-point transport encodings were removed from these paths
- integer and float terms are accepted on the wire for these numeric positions
- handler decode normalizes those values to native `float`
- device code validates and forwards LovyanGFX-like numeric values directly

Examples of removed v1-style encodings:

- centi-degrees
- x256 text scale
- x1024 image and rotate/zoom scale

Compatibility impact:

- v1 and v2 are not wire-compatible for the affected arguments above
- host code using protocol v1 must be updated before sending `ProtoVer = 2`
- unchanged operations keep their existing request shapes and meanings

## Request and response model

All requests use one tuple shape:

```erlang
{lgfx, ProtoVer, Op, Target, Flags, Arg1, Arg2, ...}
```

Field meanings:

- `lgfx`
  - tag atom

- `ProtoVer`
  - integer
  - must equal `LGFX_PORT_PROTO_VER`

- `Op`
  - operation atom

- `Target`
  - `0` => LCD
  - `1..254` => sprite handle
  - `255` => reserved and invalid

- `Flags`
  - integer bitset
  - `0` when unused

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

## Validation model

Common failure mapping:

- wrong tuple tag or protocol version => `bad_proto`
- unknown op => `bad_op`
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
- `pushImage` stride normalization and required byte count
- `drawJpg` decode and render behavior
- rotate and zoom semantic validity
- deterministic sprite allocation rules

## Binary payload lifetime

Raw pointers into caller binaries are request-scoped.

Rule:

- the driver must not retain pointers into caller binaries past the request boundary unless lifetime is explicitly managed

Current model:

- handlers borrow request binary pointers and pass them directly to `lgfx_device_*` within the same request
- text and image device calls are synchronous in the current design
- device code must fully consume those bytes before returning and must not retain the pointer after the call

That matters especially for `drawString`, `print`, `println`, `drawJpg`, and `pushImage`.

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

- scalar colors used by primitive and text operations
- palette lifecycle colors
- sprite transparent scalar colors
- `pushImage` pixel blobs

#### Scalar colors used by primitive and text operations

Primitive and text scalar colors use two modes.

Default RGB mode:

- wire format is `0x00RRGGBB` as packed RGB888 in `u32`
- handler decode quantizes that value to RGB565 before the device-facing primitive or text call
- the native primitive and text path does not preserve the original RGB888 value for those scalar arguments
- this contract is the same regardless of target color depth

Indexed palette mode:

- enabled only by op-specific flags
- the corresponding scalar argument is interpreted as a palette index
- the palette index is carried in the low 8 bits of the decoded scalar value
- indexed mode is invalid on LCD target
- indexed mode on a sprite target requires actual palette backing
- target color depth alone does not implicitly enable indexed semantics

This applies to scalar color arguments used by operations such as:

- `fillScreen`
- `clear`
- `drawPixel`
- `drawFastVLine`
- `drawFastHLine`
- `drawLine`
- `drawRect`
- `fillRect`
- `drawCircle`
- `fillCircle`
- `drawTriangle`
- `fillTriangle`
- `setTextColor`

`setColorDepth(Target, 24)`:

- changes the destination target depth
- does not change default RGB scalar wire encoding
- does not select indexed palette mode
- does not preserve full 24-bit input fidelity for primitive or text operations in default RGB mode

#### Palette lifecycle colors

Palette lifecycle operations use RGB888 directly on the wire.

- `setPaletteColor` takes `0x00RRGGBB` packed RGB888 in `u32`
- palette lifecycle arguments are not reinterpreted as RGB565 scalar colors
- `createPalette` establishes palette backing for an existing paletted sprite target
- `setPaletteColor` writes one palette entry on that palette-backed sprite

#### Sprite transparent scalar colors

`pushSprite` and `pushRotateZoom` use an optional transparent scalar argument.

Default transparent mode:

- the optional transparent argument is RGB565

Indexed transparent mode:

- enabled by `LGFX_F_TRANSPARENT_INDEX`
- the optional transparent argument is interpreted as a palette index
- indexed transparent mode requires the source sprite to have actual palette backing

#### `pushImage` pixel blobs

- RGB565 only
- big-endian per pixel (`hi lo`)
- unaffected by `setColorDepth`

## Error reasons

Canonical protocol error atoms and detail tags:

<!-- BEGIN:generated_error_reasons_table -->
<!-- generated by scripts/sync_lgfx_protocol_doc.exs -->

| C macro | Atom | Kind |
| --- | --- | --- |
| `LGFX_ERR_BAD_PROTO` | `bad_proto` | `canonical` |
| `LGFX_ERR_BAD_OP` | `bad_op` | `canonical` |
| `LGFX_ERR_BAD_FLAGS` | `bad_flags` | `canonical` |
| `LGFX_ERR_BAD_ARGS` | `bad_args` | `canonical` |
| `LGFX_ERR_BAD_TARGET` | `bad_target` | `canonical` |
| `LGFX_ERR_NOT_INITIALIZED` | `not_initialized` | `canonical` |
| `LGFX_ERR_NOT_WRITING` | `not_writing` | `canonical` |
| `LGFX_ERR_NO_MEMORY` | `no_memory` | `canonical` |
| `LGFX_ERR_INTERNAL` | `internal` | `canonical` |
| `LGFX_ERR_UNSUPPORTED` | `unsupported` | `canonical` |
| `LGFX_ERR_BATCH_FAILED` | `batch_failed` | `canonical` |
<!-- END:generated_error_reasons_table -->

Optional detail forms:

- `{error, {bad_args, Detail}}`
- `{error, {internal, EspErr}}`
- `{error, {batch_failed, FailedIndex, FailedOp, Reason}}`

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

This table documents the implemented protocol surface.

`ops.def` is the implementation source of truth. If this table and the built driver disagree, the built driver wins.

<!-- BEGIN:generated_ops_matrix -->
<!-- generated by scripts/sync_lgfx_protocol_doc.exs -->

| Op | Target rule | Flags rule | Arity | State rule | Capability bit |
| --- | --- | --- | --- | --- | --- |
| `ping` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `getCaps` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `getLastError` | `T0/bad_target` | `F0` | `5` | `any` | `LGFX_CAP_LAST_ERROR` |
| `width` | `LGFX_OP_TARGET_ANY` | `F0` | `5` | `requires_init` | - |
| `height` | `LGFX_OP_TARGET_ANY` | `F0` | `5` | `requires_init` | - |
| `init` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `close` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `startWrite` | `T0/bad_target` | `F0` | `5` | `requires_init` | - |
| `endWrite` | `T0/bad_target` | `F0` | `5` | `requires_init` | - |
| `setRotation` | `T0/bad_target` | `F0` | `6` | `requires_init` | - |
| `setBrightness` | `T0/bad_target` | `F0` | `6` | `requires_init` | - |
| `setColorDepth` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `display` | `T0/bad_target` | `F0` | `5` | `requires_init` | - |
| `fillScreen` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `6` | `requires_init` | - |
| `clear` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `6` | `requires_init` | - |
| `drawPixel` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `8` | `requires_init` | - |
| `drawFastVLine` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `9` | `requires_init` | - |
| `drawFastHLine` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `9` | `requires_init` | - |
| `drawLine` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `10` | `requires_init` | - |
| `drawRect` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `10` | `requires_init` | - |
| `fillRect` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `10` | `requires_init` | - |
| `drawCircle` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `9` | `requires_init` | - |
| `fillCircle` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `9` | `requires_init` | - |
| `drawTriangle` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `12` | `requires_init` | - |
| `fillTriangle` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_COLOR_INDEX)` | `12` | `requires_init` | - |
| `setTextSize` | `LGFX_OP_TARGET_ANY` | `F0` | `6/7` | `requires_init` | - |
| `setTextDatum` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextWrap` | `LGFX_OP_TARGET_ANY` | `F0` | `6/7` | `requires_init` | - |
| `setTextFontPreset` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextColor` | `LGFX_OP_TARGET_ANY` | `Fmask((LGFX_F_TEXT_HAS_BG | LGFX_F_TEXT_FG_INDEX | LGFX_F_TEXT_BG_INDEX))` | `6/7` | `requires_init` | - |
| `setCursor` | `LGFX_OP_TARGET_ANY` | `F0` | `7` | `requires_init` | - |
| `getCursor` | `LGFX_OP_TARGET_ANY` | `F0` | `5` | `requires_init` | - |
| `drawString` | `LGFX_OP_TARGET_ANY` | `F0` | `8` | `requires_init` | - |
| `print` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `println` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `drawJpg` | `LGFX_OP_TARGET_ANY` | `F0` | `8/14` | `requires_init` | - |
| `pushImage` | `LGFX_OP_TARGET_ANY` | `F0` | `11` | `requires_init` | `LGFX_CAP_PUSHIMAGE` |
| `setClipRect` | `LGFX_OP_TARGET_ANY` | `F0` | `9` | `requires_init` | - |
| `clearClipRect` | `LGFX_OP_TARGET_ANY` | `F0` | `5` | `requires_init` | - |
| `createSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7/8` | `requires_init` | `LGFX_CAP_SPRITE` |
| `deleteSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `5` | `requires_init` | `LGFX_CAP_SPRITE` |
| `createPalette` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `5` | `requires_init` | `LGFX_CAP_PALETTE` |
| `setPaletteColor` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7` | `requires_init` | `LGFX_CAP_PALETTE` |
| `setPivot` | `LGFX_OP_TARGET_ANY` | `F0` | `7` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `Fmask(LGFX_F_TRANSPARENT_INDEX)` | `8/9` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushRotateZoom` | `LGFX_OP_TARGET_SPRITE_ONLY` | `Fmask(LGFX_F_TRANSPARENT_INDEX)` | `11/12` | `requires_init` | `LGFX_CAP_SPRITE` |
| `getTouch` | `T0/bad_target` | `F0` | `5` | `requires_init` | `LGFX_CAP_TOUCH` |
| `getTouchRaw` | `T0/bad_target` | `F0` | `5` | `requires_init` | `LGFX_CAP_TOUCH` |
| `setTouchCalibrate` | `T0/bad_target` | `F0` | `13` | `requires_init` | `LGFX_CAP_TOUCH` |
| `calibrateTouch` | `T0/bad_target` | `F0` | `5` | `requires_init` | `LGFX_CAP_TOUCH` |
<!-- END:generated_ops_matrix -->

If an operation is not listed here, it is not implemented and must return `{error, bad_op}`.

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

Generated capability vocabulary:

<!-- BEGIN:generated_caps_table -->
<!-- generated by scripts/sync_lgfx_protocol_doc.exs -->

| C macro | Protocol bit | Shift | Value | Source |
| --- | --- | --- | --- | --- |
| `LGFX_CAP_SPRITE` | `CAP_SPRITE` | `0` | `0x0001` | `ops.def` feature_cap_bit |
| `LGFX_CAP_PUSHIMAGE` | `CAP_PUSHIMAGE` | `1` | `0x0002` | `ops.def` feature_cap_bit |
| `LGFX_CAP_LAST_ERROR` | `CAP_LAST_ERROR` | `2` | `0x0004` | `ops.def` feature_cap_bit |
| `LGFX_CAP_TOUCH` | `CAP_TOUCH` | `3` | `0x0008` | `ops.def` feature_cap_bit |
| `LGFX_CAP_PALETTE` | `CAP_PALETTE` | `4` | `0x0010` | `ops.def` feature_cap_bit |
<!-- END:generated_caps_table -->

Meaning:

- `CAP_SPRITE`
  - sprite operations are available

- `CAP_PUSHIMAGE`
  - `pushImage` is available

- `CAP_LAST_ERROR`
  - `getLastError` is available

- `CAP_TOUCH`
  - touch operations are available

- `CAP_PALETTE`
  - palette lifecycle operations are available
  - specifically `createPalette` and `setPaletteColor`

Touch note:

- `CAP_TOUCH` is advertised only when touch support is enabled in the build and touch is attached
- compiling touch support with `LGFX_PORT_TOUCH_CS_GPIO = -1` keeps touch unattached and unadvertised

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
  - primitive op color argument is interpreted as a palette index instead of default RGB input

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

- foreground and background scalar colors independently support default RGB mode or indexed palette mode
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
- does not change the wire format used by default RGB scalar colors for primitive or text operations
- does not by itself enable indexed scalar-color semantics
- does not by itself create palette backing for a sprite
- `setColorDepth(24)` does not preserve full RGB888 input fidelity for primitive or text operations in default RGB mode
- `pushImage` remains RGB565-only regardless of target color depth

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

### `pushImage`

Request args:

- `pushImage(Xi16, Yi16, Wu16, Hu16, StridePixelsU16, DataRgb565Binary)`

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
- optional transparent scalar uses RGB565 by default
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
- optional transparent scalar uses RGB565 by default
- `LGFX_F_TRANSPARENT_INDEX` interprets the transparent scalar as a palette index
- indexed transparent mode requires palette backing on the source sprite
- edge clipping is allowed

## Recommended host smoke checks

Useful checks:

- target policy
  - for example, `getCaps` with `Target != 0` should fail with `bad_target`

- capability advertisement
  - `pushImage` support should match `CAP_PUSHIMAGE`
  - `getLastError` support should match `CAP_LAST_ERROR`
  - palette support should match `CAP_PALETTE`
  - touch support should disappear from `FeatureBits` when touch is compiled but unattached

- sprite path
  - deterministic `createSprite` at a chosen handle succeeds
  - creating the same handle twice fails
  - `setPivot` rejects LCD target
  - valid `pushSprite` to LCD succeeds
  - valid `pushRotateZoom` with LovyanGFX-like angle and zoom values succeeds
  - missing destination sprite fails for sprite destination

- palette path
  - `createSprite(..., 4)` followed by `createPalette` succeeds
  - `setPaletteColor(0, 16#112233)` succeeds on a palette-backed sprite
  - out-of-range `setPaletteColor` index fails for the selected depth
  - `createPalette` on a true-color sprite fails
  - indexed primitive or text color mode on LCD fails
  - indexed primitive or text color mode on a sprite without palette backing fails
  - indexed transparent mode on a non-paletted source sprite fails

- text path
  - `setTextWrap(true)` should map to `wrap_x=true, wrap_y=false`
  - `setTextWrap(true, true)` should set both axes true
  - `setTextSize(1)` should mean `1.0x`
  - zero scale should fail

- color contract path
  - primitive and text colors should accept `0x00RRGGBB` in default RGB mode
  - default RGB scalar-color inputs should quantize before device-side primitive or text execution
  - `setColorDepth(24)` should not imply full RGB888 fidelity for primitive or text ops
  - `pushImage` should remain RGB565-only

- jpg path
  - valid short-form and extended-form calls should succeed
  - zero or negative scale should fail
  - non-binary payload should fail
  - over-cap binary should fail
  - corrupt JPEG data should fail without crashing the driver

## Maintenance checklist

When adding or changing an operation:

- update `lgfx_port/include_internal/lgfx_port/ops.def`

- implement or update the handler

- update capability, error, or protocol constants if needed

- resync generated protocol tables
  - `elixir scripts/sync_lgfx_protocol_doc.exs`

- verify `getCaps` matches the new `feature_cap_bit`

- update this document only for externally visible semantics not obvious from the generated tables

## Compatibility rules

### Pre-release note

This repository is pre-release.

Until the first tagged release:

- breaking protocol changes may still happen during active development
- when `LGFX_PORT_PROTO_VER` changes, the change should be recorded in this document
- host and driver should be updated together

After the first release:

- breaking protocol changes must bump `LGFX_PORT_PROTO_VER`

### Compatible changes

- add new operations
- add new capability bits
- add new error reasons without reinterpreting existing ones
- tighten validation only when it rejects requests already invalid by contract
- add optional error detail tuples while preserving `{error, Reason}`

### Breaking changes

- change the request tuple shape for existing ops
- change argument order or meaning
- reinterpret existing flags
- change RGB565 byte order for `pushImage`
