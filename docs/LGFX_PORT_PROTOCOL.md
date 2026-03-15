# LovyanGFX AtomVM Port Protocol (v1)

## Summary

This document defines the tuple protocol between an AtomVM host application and the `lgfx_port` driver.

Key points:

- Host and driver communicate using Erlang terms.
- Large payloads such as text and pixel data use binaries.
- Validation, dispatch, and capability advertisement are metadata-driven.
- The implemented operation surface is the one declared in `ops.def`.
- Open-time config passed through `open_port/2` is outside the request tuple protocol documented here.
- The current sprite surface includes deterministic handle-based `createSprite`, destination-aware whole-sprite blit via `pushSprite`, and destination-aware rotate/zoom blit via `pushRotateZoom`.
- Touch is advertised only when touch support is both enabled and attached.
- Primitive and text colors are accepted on the wire as `0x00RRGGBB`, then quantized to RGB565 before entering the worker and device path.
- `setColorDepth(24)` changes target depth, but does not change scalar color wire encoding and does not imply full 24-bit input color fidelity for primitive or text operations.
- `setTextSize` uses plain positive integer size arguments on the wire (`1..255`), not fixed-point.

## Source of truth

The protocol contract is defined by these sources, each with a different role:

- `lgfx_port/include_internal/lgfx_port/ops.def`
  - normative operation surface
  - op atom
  - handler
  - arity range
  - allowed flags mask
  - target policy
  - state policy
  - `feature_cap_bit`

- `lgfx_port/include_internal/lgfx_port/protocol.h`
  - protocol constants
  - capability bit names and values
  - wire-level limits

- generated `lgfx_port/lgfx_port_config.h`
  - build knobs and derived gates used by the component

- this document
  - human-readable wire contract
  - generated tables synchronized from source metadata

Important invariants:

- If an op is not declared in `ops.def`, it is not part of the protocol.
- `getCaps` derives `FeatureBits` from metadata plus the active dispatch surface.
- `FeatureBits` contains protocol bits only.
- A capability bit is meaningful only when backed by at least one real enabled operation.
- Touch capability is advertised only when touch ops are both compiled in and attached.
- Generated tables and implementation must agree.

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

Examples of device-layer semantic checks:

- source or destination sprite existence
- `pushImage` stride normalization and required byte count
- rotate/zoom finite and positive constraints
- deterministic sprite allocation rules

## Binary payload lifetime

Binaries are used for throughput, but raw pointers from term binaries are request-scoped.

Rule:

- The driver must not retain pointers into caller binaries past the request boundary unless it explicitly manages lifetime.

Current model:

- deep-copy variable-length payloads before enqueueing worker jobs
- free the copied payload after the device call completes

That matters especially for `pushImage` and string-bearing operations.

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

### Text scale

`setTextSize` uses positive x256 fixed-point integers on the wire:

- `256` => `1.0x`
- `384` => `1.5x`
- `512` => `2.0x`
- `768` => `3.0x`

Rules:

- wire form is integer-only
- `0` is invalid
- current accepted range is `1..65535`

### Strings

- text arguments are UTF-8 binaries
- no trailing NUL is required
- embedded NUL may be rejected for ops that call C-string APIs

### Colors

Primitive and text colors:

- wire format is `0x00RRGGBB` as packed RGB888 in `u32`
- handler decode quantizes that value to RGB565 before entering worker and device layers
- worker and device layers do not preserve the original RGB888 value for primitive or text ops
- this scalar-color contract is the same regardless of target color depth

This applies to scalar color arguments used by primitive and text operations such as:

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
- does not change primitive or text wire encoding
- does not change the primitive or text worker/device ABI
- therefore does not imply full 24-bit input color fidelity for primitive or text operations

Examples:

- `0x112233` is accepted on the wire as RGB888
- that value is quantized to RGB565 `0x1106` before primitive or text execution
- on a 16-bit target, primitive and text ops use that RGB565 value directly
- on a 24-bit target, primitive and text ops still start from the same quantized RGB565 value, not the original `0x112233`

`pushImage` pixel blobs:

- RGB565 only
- big-endian per pixel (`hi lo`)
- unaffected by `setColorDepth`

Sprite transparent keys:

- optional transparent color uses RGB565 in `u16`

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
| `fillScreen` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `clear` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `drawPixel` | `LGFX_OP_TARGET_ANY` | `F0` | `8` | `requires_init` | - |
| `drawFastVLine` | `LGFX_OP_TARGET_ANY` | `F0` | `9` | `requires_init` | - |
| `drawFastHLine` | `LGFX_OP_TARGET_ANY` | `F0` | `9` | `requires_init` | - |
| `drawLine` | `LGFX_OP_TARGET_ANY` | `F0` | `10` | `requires_init` | - |
| `drawRect` | `LGFX_OP_TARGET_ANY` | `F0` | `10` | `requires_init` | - |
| `fillRect` | `LGFX_OP_TARGET_ANY` | `F0` | `10` | `requires_init` | - |
| `drawCircle` | `LGFX_OP_TARGET_ANY` | `F0` | `9` | `requires_init` | - |
| `fillCircle` | `LGFX_OP_TARGET_ANY` | `F0` | `9` | `requires_init` | - |
| `drawTriangle` | `LGFX_OP_TARGET_ANY` | `F0` | `12` | `requires_init` | - |
| `fillTriangle` | `LGFX_OP_TARGET_ANY` | `F0` | `12` | `requires_init` | - |
| `setTextSize` | `LGFX_OP_TARGET_ANY` | `F0` | `6/7` | `requires_init` | - |
| `setTextDatum` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextWrap` | `LGFX_OP_TARGET_ANY` | `F0` | `6/7` | `requires_init` | - |
| `setTextFont` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextFontPreset` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextColor` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_TEXT_HAS_BG)` | `6/7` | `requires_init` | - |
| `drawString` | `LGFX_OP_TARGET_ANY` | `F0` | `8` | `requires_init` | - |
| `pushImage` | `LGFX_OP_TARGET_ANY` | `F0` | `11` | `requires_init` | `LGFX_CAP_PUSHIMAGE` |
| `setClipRect` | `LGFX_OP_TARGET_ANY` | `F0` | `9` | `requires_init` | - |
| `clearClipRect` | `LGFX_OP_TARGET_ANY` | `F0` | `5` | `requires_init` | - |
| `createSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7/8` | `requires_init` | `LGFX_CAP_SPRITE` |
| `deleteSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `5` | `requires_init` | `LGFX_CAP_SPRITE` |
| `setPivot` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `8/9` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushRotateZoom` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `11/12` | `requires_init` | `LGFX_CAP_SPRITE` |
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
- apply real build/runtime gates
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
<!-- END:generated_caps_table -->

Current meaning:

- `CAP_SPRITE`
  - sprite operations are available

- `CAP_PUSHIMAGE`
  - `pushImage` is available

- `CAP_LAST_ERROR`
  - `getLastError` is available

- `CAP_TOUCH`
  - touch operations are available

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

Two font-selection paths are exposed:

- `setTextFont(FontIdU8)`
  - raw numeric passthrough to the pinned LovyanGFX API
  - accepts `0..255`
  - for stable protocol-owned font selection, prefer `setTextFontPreset`

- `setTextFontPreset(PresetIdU8)`
  - driver-defined stable preset selection

### `setTextSize`

Args:

- `setTextSize(ScaleXX256U16)`
- `setTextSize(ScaleXX256U16, ScaleYX256U16)`

Wire encoding:

- positive x256 fixed-point integer
- `256` means `1.0x`
- `384` means `1.5x`
- `512` means `2.0x`

Rules:

- one-argument form applies the same scale to both axes
- two-argument form sets both axes explicitly
- `0` is invalid
- the worker and device path keep protocol-owned x256 scale values
- conversion to the pinned LovyanGFX float call shape happens at the final device-call boundary

Errors:

- zero or out-of-range value => `{error, bad_args}`

### `setTextDatum`

Args:

- `setTextDatum(DatumU8)`

Rules:

- `DatumU8` must be an integer in `0..255`
- forwarded as a raw numeric passthrough to the pinned LovyanGFX text-datum API
- this protocol does not define a smaller stable subset of datum values

Errors:

- out-of-range value => `{error, bad_args}`

### `setTextWrap`

Args:

- `setTextWrap(WrapXBool)`
- `setTextWrap(WrapXBool, WrapYBool)`

Rules:

- booleans are accepted as atom `true` / `false`

- numeric `0` / `1` are also accepted by the handler decode path

- one-argument form follows LovyanGFX semantics
  - `setTextWrap(WrapXBool)` means `wrap_x = WrapXBool`, `wrap_y = false`

- two-argument form sets both axes explicitly
  - `setTextWrap(WrapXBool, WrapYBool)` means `wrap_x = WrapXBool`, `wrap_y = WrapYBool`

Examples:

- `setTextWrap(true)` => wrap horizontally only
- `setTextWrap(true, true)` => wrap horizontally and vertically
- `setTextWrap(false)` => disable horizontal and vertical wrapping

### `setTextFont`

Args:

- `setTextFont(FontIdU8)`

Rules:

- `FontIdU8` must be an integer in `0..255`
- forwarded as a raw numeric passthrough to the pinned LovyanGFX API
- this protocol does not define a smaller stable subset of font IDs
- for stable protocol-owned font selection, prefer `setTextFontPreset`

Errors:

- out-of-range value => `{error, bad_args}`

### `setTextFontPreset`

Preset IDs:

- `0` = `ascii`
  - select ASCII fallback
  - normalize text scale to `256` (`1.0x`)

- `1` = `jp_small`
  - Japanese-capable preset
  - text scale `256` (`1.0x`)

- `2` = `jp_medium`
  - Japanese-capable preset
  - text scale `512` (`2.0x`)

- `3` = `jp_large`
  - Japanese-capable preset
  - text scale `768` (`3.0x`)

Errors:

- unknown preset => `{error, bad_args}`
- preset compiled out => `{error, unsupported}`

## Flags

`Flags` is op-specific unless documented otherwise.

### `setTextColor`

- `F_TEXT_HAS_BG = 1 bsl 0`

Semantics:

- when present, the background color argument is enabled
- the driver enforces the bitmask

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
- does not change the wire format for primitive or text color arguments
- does not change the primitive or text worker/device ABI, which remains RGB565-based
- therefore `setColorDepth(24)` does not preserve full RGB888 input fidelity for primitive or text operations
- `pushImage` remains RGB565-only regardless of target color depth

Examples:

- `setColorDepth(16), fillScreen(0x112233)` uses quantized RGB565 `0x1106`
- `setColorDepth(24), fillScreen(0x112233)` still quantizes to RGB565 `0x1106` before drawing
- `setColorDepth(24)` may affect how the target stores or renders the widened result, but not the original input precision of primitive or text colors

### `setTextWrap`

Args:

- `setTextWrap(WrapXBool)`
- `setTextWrap(WrapXBool, WrapYBool)`

Semantics:

- one-argument form follows LovyanGFX and sets `wrap_y = false`
- two-argument form sets both axes explicitly

### `pushImage`

Request args:

- `pushImage(Xi16, Yi16, Wu16, Hu16, StridePixelsU16, DataRgb565Binary)`

Handler-side responsibilities:

- decode tuple fields
- require `W > 0`
- require `H > 0`
- require the final argument to be a binary
- enforce the binary-size cap

Device-layer responsibilities:

- if `StridePixelsU16 == 0`, normalize effective stride to `W`
- require effective stride `>= W`
- require `byte_size(Data)` to be even
- check overflow and required byte count
- require `byte_size(Data)` to be large enough for the requested image
- ignore trailing bytes beyond the required minimum

Ownership rule:

- the driver must not pass a term-binary pointer into any path that can outlive the request unless it first copies or synchronously waits for completion

### `createSprite`

This is deterministic sprite allocation.

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
- sprite color depth affects the target storage format, but does not change primitive or text input color decoding rules

### `pushSprite`

This is a destination-aware whole-sprite blit.

Request-header `Target` is the source sprite handle:

- `1..254` => valid source sprite domain
- `0` => invalid

Args:

- `pushSprite(DstTargetU8, DstXi16, DstYi16)`
- `pushSprite(DstTargetU8, DstXi16, DstYi16, TransparentRgb565U16)`

Rules:

- `DstTargetU8 == 0` => LCD destination
- `DstTargetU8 in 1..254` => destination sprite
- source sprite existence is resolved in the device layer
- destination sprite existence is resolved in the device layer when `DstTarget != 0`
- optional transparent color is RGB565
- edge clipping is allowed

There is no region-based sprite blit op in the current protocol.

### `pushRotateZoom`

This draws a source sprite to a destination target with rotation and scaling.

Request-header `Target` is the source sprite handle.

Args:

- `pushRotateZoom(DstTargetU8, DstXi16, DstYi16, AngleCentiDegI32, ZoomXX1024I32, ZoomYX1024I32)`
- `pushRotateZoom(DstTargetU8, DstXi16, DstYi16, AngleCentiDegI32, ZoomXX1024I32, ZoomYX1024I32, TransparentRgb565U16)`

Wire encoding:

- `AngleCentiDegI32` uses centi-degrees
- `ZoomXX1024I32` uses x1024 fixed-point scale
- `ZoomYX1024I32` uses x1024 fixed-point scale

Rules:

- destination target rules are the same as `pushSprite`
- rotation uses the source sprite pivot set by `setPivot`
- source and destination sprite existence rules are resolved in the device layer
- both zoom values must represent positive scale
- optional transparent color is RGB565
- edge clipping is allowed

Conversion model:

- the protocol carries fixed-point integers
- the worker ABI carries those fixed-point values deeper than the handler path
- conversion to the float LovyanGFX call shape happens close to the device call boundary

Equivalent formulas:

- `angle_deg = AngleCentiDegI32 / 100.0`
- `zoom_x = ZoomXX1024I32 / 1024.0`
- `zoom_y = ZoomYX1024I32 / 1024.0`

## Recommended host smoke checks

Useful checks:

- target policy
  - for example, `getCaps` with `Target != 0` should fail with `bad_target`

- capability advertisement
  - `pushImage` support should match `CAP_PUSHIMAGE`
  - `getLastError` support should match `CAP_LAST_ERROR`
  - touch support should disappear from `FeatureBits` when touch is compiled but unattached

- sprite path
  - deterministic `createSprite` at a chosen handle succeeds
  - creating the same handle twice fails
  - valid `pushSprite` to LCD succeeds
  - missing destination sprite fails for sprite destination

- text-wrap path
  - `setTextWrap(true)` should map to `wrap_x=true, wrap_y=false`
  - `setTextWrap(true, true)` should set both axes true
  - one-argument and two-argument forms should remain distinct

- text-scale path
  - `setTextSize(256)` should mean `1.0x`
  - `setTextSize(384)` should mean `1.5x`
  - `setTextSize(256, 512)` should mean `1.0x, 2.0x`
  - zero scale should fail

- color contract path
  - primitive and text colors should accept `0x00RRGGBB` on the wire
  - primitive and text color inputs should quantize before worker/device execution
  - `setColorDepth(16)` and `setColorDepth(24)` should not change primitive or text input quantization behavior
  - `setColorDepth(24)` should not imply full RGB888 input fidelity for primitive or text ops
  - `pushImage` should remain RGB565-only

- rotate/zoom path
  - valid fixed-point call succeeds
  - zero or invalid zoom fails

- metadata sanity
  - `getCaps` returns `{caps, ProtoVer, MaxBinaryBytes, MaxSprites, FeatureBits}`
  - host wrapper protocol version matches `ProtoVer`

## Maintenance checklist

When adding or changing an operation:

- update `lgfx_port/include_internal/lgfx_port/ops.def`

- implement or update the handler

- update capability, error, or protocol constants if needed

- resync generated protocol tables
  - `elixir scripts/sync_lgfx_protocol_doc.exs`

- verify `getCaps` matches the new `feature_cap_bit`

- update this document only for semantics not obvious from the generated tables

## Compatibility rules

### Pre-release note

This repository is pre-release.

Until the first tagged release:

- breaking protocol changes may happen without bumping `LGFX_PORT_PROTO_VER`
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
