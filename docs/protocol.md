<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# LovyanGFX AtomVM Port Protocol

This document defines the tuple protocol between an AtomVM host application and the native `lgfx_port` driver.

See [the architecture overview](architecture.md) for the repository map, [the `lgfx_port` README](../lgfx_port/README.md) for port-layer and ownership details, and [the protocol reference](protocol-reference.md) for generated operation, capability, and error tables.

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

- `docs/protocol-reference.md`
  - generated reference tables synchronized from source metadata

Important invariants:

- if an op is not declared in `ops.def`, it is not part of the protocol
- `getCaps` derives `FeatureBits` from metadata plus the active dispatch surface
- `FeatureBits` contains protocol bits only
- touch is advertised only when touch ops are both compiled in and effectively attached
- generated reference tables and implementation must agree

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

- display colors used by primitive and text operations
- palette lifecycle colors
- palette indices
- `pushImage` pixel blobs

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

#### `pushImage` pixel blobs

- RGB565 only
- little-endian per pixel (`lo hi`) as ordinary 16-bit RGB565 words
- unaffected by `setColorDepth`
- target-side byte swapping remains controlled separately by `setSwapBytes`

## Error reasons

Canonical protocol error atoms and detail tags are listed in [the generated error reference](protocol-reference.md#generated-error-reasons).

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

The generated implemented operation matrix lives in [the protocol reference](protocol-reference.md#implemented-operation-matrix).

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

Generated capability vocabulary is listed in [the protocol reference](protocol-reference.md#generated-capability-vocabulary).

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
  - primitive and text colors should accept RGB565 in non-index mode
  - `pushSprite` and `pushRotateZoom` transparent values should use the same non-index display-color contract
  - indexed primitive or text color mode should still require explicit index flags
  - `setColorDepth(24)` should not change the non-index display-color wire format
  - `setPaletteColor` should remain RGB888-only
  - `pushImage` should remain RGB565-only

- jpg path
  - valid short-form and extended-form calls should succeed
  - zero or negative scale should fail
  - non-binary payload should fail
  - corrupt JPEG data should fail without crashing the driver

- binary limit path
  - over-cap binary should fail

## Maintenance checklist

When changing the protocol surface or behavior, verify all of the following:

- `ops.def` metadata still matches the implementation
- generated protocol-reference tables still match source metadata
- `getCaps()` advertisement still reflects the real dispatch surface
- allowed flags and arity still match the handler decode paths
- request and response shapes in this document still match the implementation
- smoke checks still cover the main happy paths and contract edges
- sample host code still uses the current scalar color contract
- palette lifecycle examples still use packed RGB888
- `pushImage` examples still use RGB565 payloads

## Compatibility rules

Treat these changes as protocol-affecting and bump `LGFX_PORT_PROTO_VER` when they occur:

- changing request tuple shape
- changing response shape
- changing operation meaning
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
