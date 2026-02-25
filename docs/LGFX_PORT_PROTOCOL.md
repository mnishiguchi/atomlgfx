# LovyanGFX AtomVM Port Protocol (v1)

This document defines the **term (tuple) protocol** between a host application (Elixir on AtomVM) and an AtomVM **port driver** that wraps **LovyanGFX**.

- Host and driver communicate using **Erlang terms** (tuples, atoms, integers, binaries, lists).
- Pixel/image payloads are carried as **binaries** for throughput.

---

## Goals

- Elixir-friendly API via tuples and atoms (pattern matching on the caller side).
- Minimal parsing complexity in C (arity/type/range checks, no custom frame parser).
- LovyanGFX function names are used as **operation atoms**.
- Keep protocol validation/dispatch/capability advertisement aligned from a single metadata source (`ops.def`).
- No general filesystem opcode suite.
  - SD and files use AtomVM FS APIs (`esp:mount/4`, `esp:umount/1`, `atomvm:posix_*`).
  - Optional file-backed decode ops may exist later (JPEG/PNG) to keep hot paths in C.
- Keep host-side protocol smoke tests aligned with the same metadata-driven contract.

---

## Source of truth and metadata model

The protocol contract is defined across C metadata headers and this document, with distinct roles:

- **Normative op metadata:** `ports/include/lgfx_port/ops.def`
  - Defines the protocol-visible operation surface:
    - operation atom
    - handler
    - arity range
    - allowed flags mask
    - target policy
    - state policy
    - `feature_cap_bit` (protocol-facing capability bit, if any)
  - Also drives protocol-facing capability advertisement (`getCaps`) via `feature_cap_bit`

- **Normative capability bit vocabulary:** `ports/include/lgfx_port/caps.h`
  - Defines `LGFX_CAP_*` protocol bit names and bit positions

- **Normative error vocabulary:** `ports/include/lgfx_port/errors.h`
  - Defines canonical protocol error atoms (`LGFX_ERR_*`)
  - Defines optional detail tuple reason tags (for example `batch_failed`)

- **Human-readable contract:** this document (`docs/LGFX_PORT_PROTOCOL.md`)
  - Mirrors the current protocol surface and explains semantics
  - Includes generated tables synced from the metadata files above

### Important invariants

- `getCaps` **must derive `FeatureBits` from `ops.def` metadata** (not ad hoc implementation-only flags).
- `FeatureBits` **must contain only protocol bits** (`CAP_*`), never internal/private bit ranges.
- Capability advertisement is based on:
  - `feature_cap_bit` metadata in `ops.def`
  - whether the op is actually present in the dispatch table
  - optional compile-time gating (for example `getLastError` support)
- Error atoms and detail tags documented in this file **must match** `errors.h`.
- Capability bit names/positions documented in this file **must match** `caps.h`.

This prevents protocol/implementation drift.

---

## Protocol version

```c
#define LGFX_PORT_PROTO_VER 1
```

- The version is carried in every request term and must match.
- Compatibility rules are defined in **Protocol compatibility rules**.

---

## Transport model

- The caller sends a **request term** to the port driver.
- The driver returns a **response term** for `$call` requests.
- AtomVM v0.7.0-dev does not currently provide `$cast` semantics; treat all requests as `$call`.

This protocol definition specifies **request/response terms** only.

---

## Binary payload lifetime (implementation requirement)

This protocol uses Erlang binaries for throughput (text strings and pixel blobs).

**Rule:** any pointer obtained from a term binary (for example via `term_binary_data(...)`) is valid only for the duration of handling that request, unless the driver explicitly extends its lifetime.

This matters for operations like `pushImage` (and any future op that passes binary-backed buffers to the device layer).

Driver requirements:

- The driver must ensure the device layer has **finished reading** the input buffer before the request handler returns.
- If the device call is **synchronous** with respect to the input buffer, passing the term-binary pointer directly is acceptable.
- If the device call can start **async DMA** (or otherwise outlive the call), the driver must do one of:
  - **Copy** the binary into driver-owned (DMA-safe) memory before handing it to the device/worker, and only free/reuse after completion.
  - **Block** until the transfer completes (explicit completion barrier) before allowing the request to return.

Host guidance:

- Binaries are treated as **immutable inputs**. The driver must not modify caller-provided binary contents.

---

## Request term format

All requests use a single uniform tuple shape:

```erlang
{lgfx, ProtoVer, Op, Target, Flags, Arg1, Arg2, ...}
```

Where:

- `lgfx` is a tag atom.

- `ProtoVer` is an integer (must equal `LGFX_PORT_PROTO_VER`).

- `Op` is an atom identifying the operation (typically a LovyanGFX method name).

- `Target` is an integer:
  - `0` => LCD (`lgfx::LGFX_Device`)
  - `1..254` => sprite handle (used by sprite operations when `CAP_SPRITE` is advertised)
  - `255` reserved (invalid)

- `Flags` is an integer bitset (operation-specific; `0` if unused).

- `ArgN` are operation-specific arguments.

### Validation rules

- Wrong tuple tag/version => `bad_proto`
- Unknown op => `bad_op`
- Wrong arity/types => `bad_args`
- Values outside allowed ranges => `bad_args` (or a more specific reason if defined)
- Invalid `Target` => `bad_target`
- Non-zero `Flags` for an operation that does not define flags => `bad_flags`

---

## Response term format

Responses are always one of:

```erlang
{ok, Result}
{error, Reason}
```

Conventions:

- Void operations return `{ok, ok}`.
- Getters return `{ok, Value}`.
- Structured returns use tuples.

`Reason` is an atom or a tuple (see **Error reasons**).

---

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
| `LGFX_ERR_BATCH_FAILED` | `batch_failed` | `detail tag` |
<!-- END:generated_error_reasons_table -->

Optional detail forms (examples):

- `{error, {bad_args, Detail}}`
- `{error, {internal, EspErr}}`
- `{error, {batch_failed, FailedIndex, FailedOp, Reason}}` (reserved for `batchVoid`)

Client guidance:

- Always match `{error, reason}` and treat `reason` as opaque (it may be an atom or a tuple).

---

## Common data types (semantic)

Driver-enforced ranges:

- `i16`: `-32768..32767`
- `i32`: `-2147483648..2147483647`
- `u16`: `0..65535`
- `u32`: `0..4294967295`

Coordinates and sizes:

- `x`, `y`: `i16`
- `w`, `h`: `u16`
- angle/zoom encodings are operation-specific (for example `pushRotateZoom`
  uses centi-degree `i32` and x1024 zoom i32 fixed-point fields; see that
  section)

---

## String encoding

- Text/path arguments are UTF-8 binaries.
- No trailing null terminator is required.

Rules:

- Text/path must be a binary (reject charlists for predictability).
- Embedded NUL bytes (`0x00`) may be rejected for string-bearing operations that call C-string APIs (for example `drawString`).

---

## Color encoding rules

### Primitive and text colors

Colors are **RGB888 packed into a u32 integer**:

- `color888 = 0x00RRGGBB`
- The top byte must be `0x00` (alpha ignored)

The driver converts `color888` to RGB565 internally before calling LovyanGFX.

### Pixel blob formats (`pushImage`)

RGB565 only:

- RGB565 binary is big-endian per pixel (`hi lo`), 2 bytes per pixel.

### Sprite transparent-key colors (`pushSprite*`, `pushRotateZoom`)

The optional transparent color argument for sprite compositing ops uses **RGB565 as `u16`** (native sprite path), not `0x00RRGGBB`:

- `transparent565 = 0x0000..0xFFFF` (RGB565)

This applies to:

- `pushSprite`
- `pushSpriteRegion`
- `pushRotateZoom`

---

## Fonts

The protocol exposes two ways to select fonts:

- `setTextFont(FontIdU8)`
  - Direct LovyanGFX numeric font selection.
  - Intended for ASCII-oriented UI and lightweight labels.
  - Does not imply a text size change.

- `setFontPreset(PresetIdU8)`
  - A driver-defined stable preset enum intended for higher-level font choices.
  - Presets may also normalize or change text size on the device.

### `setFontPreset`

#### Request

- `setFontPreset(PresetIdU8)`

#### Preset IDs (v1)

`PresetIdU8` is an integer:

- `0` = `ascii`
  - Device behavior: select ASCII fallback (typically `setTextFont(1)`) and normalize text size to `1`.

- `1` = `jp_small`
  - Japanese-capable font preset, scaled to size `1`.

- `2` = `jp_medium`
  - Japanese-capable font preset, scaled to size `2`.

- `3` = `jp_large`
  - Japanese-capable font preset, scaled to size `3`.

#### Errors

- Unknown preset ID => `{error, bad_args}`
- Preset compiled out (build option) => `{error, unsupported}`

#### Notes

- JP preset availability is build-dependent (see `LGFX_PORT_ENABLE_JP_FONTS`).
- Host wrappers may cache an implied size for presets as an optimization, but the device remains the source of truth for actual text state.

---

## Flags

`Flags` is operation-specific unless explicitly marked common.

### `setTextColor`

- `F_TEXT_HAS_BG = 1 bsl 0` (bg color included)

Flag names are conceptual host constants. The driver enforces the bitmask, not the host-side constant name.

---

## Operation policy notation

This notation is used in the operation matrix and mirrors the validation metadata in `ops.def`.

### Target rule

- `T0/bad_target`
  - Require `Target == 0`, else `{error, bad_target}`
  - Used when the operation is effectively targetless (non-zero target is nonsensical)

- `T0/unsupported`
  - Require `Target == 0`, else `{error, unsupported}`
  - Used when the operation is target-aware in principle, but only LCD target `0` is supported today (sprite target support may be added later)

- `LGFX_OP_TARGET_ANY`
  - Accept `Target == 0` (LCD) or `Target in 1..254` (sprite handle)
  - `255` remains invalid
  - Used by target-aware drawing/text/image ops that can operate on either LCD or sprite

- `LGFX_OP_TARGET_SPRITE_ONLY`
  - Require `Target in 1..254` (sprite handle), else `{error, bad_target}`
  - Used by sprite lifecycle / sprite compositing operations where `Target` names the source sprite object
  - `Target == 0` (LCD) is invalid for these ops

### Flags rule

- `F0`
  - Require `Flags == 0`, else `{error, bad_flags}`

- `Fmask(X)`
  - Require `(Flags & ~X) == 0`, else `{error, bad_flags}`

### Arity rule

- Arity mismatch => `{error, bad_args}`

### State rule

- `any`
  - Operation may be called before `init`

- `requires_init`
  - Operation requires a successfully initialized display state, else `{error, not_initialized}`

---

## Implemented operation matrix (current protocol surface)

This table is the current documented contract for the protocol-visible operation metadata.

**Implementation source of truth:** `ports/include/lgfx_port/ops.def`
If this table and `ops.def` disagree, `ops.def` (and the built driver) wins.

<!-- BEGIN:generated_ops_matrix -->
<!-- generated by scripts/sync_lgfx_protocol_doc.exs -->

| Op | Target rule | Flags rule | Arity | State rule | Capability bit |
| --- | --- | --- | --- | --- | --- |
| `ping` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `getCaps` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `getLastError` | `T0/bad_target` | `F0` | `5` | `any` | `LGFX_CAP_LAST_ERROR` |
| `width` | `T0/unsupported` | `F0` | `5` | `requires_init` | - |
| `height` | `T0/unsupported` | `F0` | `5` | `requires_init` | - |
| `init` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `close` | `T0/bad_target` | `F0` | `5` | `any` | - |
| `setRotation` | `T0/bad_target` | `F0` | `6` | `requires_init` | - |
| `setBrightness` | `T0/bad_target` | `F0` | `6` | `requires_init` | - |
| `setColorDepth` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
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
| `setTextSize` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextDatum` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextWrap` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextFont` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setFontPreset` | `LGFX_OP_TARGET_ANY` | `F0` | `6` | `requires_init` | - |
| `setTextColor` | `LGFX_OP_TARGET_ANY` | `Fmask(LGFX_F_TEXT_HAS_BG)` | `6/7` | `requires_init` | - |
| `drawString` | `LGFX_OP_TARGET_ANY` | `F0` | `8` | `requires_init` | - |
| `pushImage` | `LGFX_OP_TARGET_ANY` | `F0` | `11` | `requires_init` | `LGFX_CAP_PUSHIMAGE` |
| `createSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7/8` | `requires_init` | `LGFX_CAP_SPRITE` |
| `deleteSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `5` | `requires_init` | `LGFX_CAP_SPRITE` |
| `setPivot` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushSprite` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `7/8` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushSpriteRegion` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `11/12` | `requires_init` | `LGFX_CAP_SPRITE` |
| `pushRotateZoom` | `LGFX_OP_TARGET_SPRITE_ONLY` | `F0` | `10/11` | `requires_init` | `LGFX_CAP_SPRITE` |
| `getTouch` | `T0/bad_target` | `F0` | `5` | `requires_init` | `LGFX_CAP_TOUCH` |
| `getTouchRaw` | `T0/bad_target` | `F0` | `5` | `requires_init` | `LGFX_CAP_TOUCH` |
| `setTouchCalibrate` | `T0/bad_target` | `F0` | `13` | `requires_init` | `LGFX_CAP_TOUCH` |
| `calibrateTouch` | `T0/bad_target` | `F0` | `5` | `requires_init` | `LGFX_CAP_TOUCH` |
<!-- END:generated_ops_matrix -->

If an operation is not listed here, it is not implemented and must return `{error, bad_op}`.

---

## Capabilities (`getCaps`)

### Request

- `getCaps()` (LCD only; `Target` must be `0`)

### Response

```erlang
{ok, {caps, ProtoVer, MaxBinaryBytes, MaxSprites, FeatureBits}}
```

Fields:

- `ProtoVer`
  - `LGFX_PORT_PROTO_VER`

- `MaxBinaryBytes`
  - Maximum accepted size (bytes) for any binary argument

- `MaxSprites`
  - Maximum concurrently allocated sprites
  - If `CAP_SPRITE` is not set, `MaxSprites` must be `0`

- `FeatureBits`
  - Protocol feature bitset (`CAP_*` only)

### Capability derivation rules (normative behavior)

`FeatureBits` is derived from the operation metadata and active dispatch table:

- Start with `0`

- For each implemented operation in `ops.def`:
  - Read its `feature_cap_bit`
  - If non-zero and the op is present in dispatch, OR it into `FeatureBits`

- Apply compile-time gates (for example `getLastError` support)

- Optionally add a safe-yield capability bit only when transaction-style ops are advertised

- Mask to known protocol bits before returning

### Feature bits

<!-- BEGIN:generated_caps_table -->
<!-- generated by scripts/sync_lgfx_protocol_doc.exs -->

| C macro | Protocol bit | Shift | Value | Source |
| --- | --- | --- | --- | --- |
| `LGFX_CAP_SPRITE` | `CAP_SPRITE` | `0` | `0x0001` | `ops.def` feature_cap_bit |
| `LGFX_CAP_PUSHIMAGE` | `CAP_PUSHIMAGE` | `1` | `0x0002` | `ops.def` feature_cap_bit |
| `LGFX_CAP_JPG_FILE` | `CAP_JPG_FILE` | `2` | `0x0004` | reserved / not currently op-linked |
| `LGFX_CAP_PNG_FILE` | `CAP_PNG_FILE` | `3` | `0x0008` | reserved / not currently op-linked |
| `LGFX_CAP_LAST_ERROR` | `CAP_LAST_ERROR` | `4` | `0x0010` | `ops.def` feature_cap_bit |
| `LGFX_CAP_BATCH_VOID` | `CAP_BATCH_VOID` | `5` | `0x0020` | reserved / not currently op-linked |
| `LGFX_CAP_TOUCH` | `CAP_TOUCH` | `6` | `0x0040` | `ops.def` feature_cap_bit |
| `LGFX_CAP_SAFE_YIELD_FORGIVING` | `CAP_SAFE_YIELD_FORGIVING` | `8` | `0x0100` | build option (`LGFX_PORT_SAFE_YIELD_CAP`) |
| `LGFX_CAP_SAFE_YIELD_STRICT` | `CAP_SAFE_YIELD_STRICT` | `9` | `0x0200` | build option (`LGFX_PORT_SAFE_YIELD_CAP`) |
<!-- END:generated_caps_table -->

Semantics and usage notes:

- `CAP_SPRITE        = 1 bsl 0`
  - Advertised when sprite operations are available (for example `createSprite`, `pushSprite`, `pushRotateZoom`)

- `CAP_PUSHIMAGE     = 1 bsl 1`
  - Advertised when `pushImage` is available

- `CAP_JPG_FILE      = 1 bsl 2`
  - Reserved

- `CAP_PNG_FILE      = 1 bsl 3`
  - Reserved

- `CAP_LAST_ERROR    = 1 bsl 4`
  - Advertised when `getLastError` is enabled

- `CAP_BATCH_VOID    = 1 bsl 5`
  - Reserved for `batchVoid`

- `CAP_SAFE_YIELD_FORGIVING = 1 bsl 8`
  - Reserved; advertised only when transaction ops exist and the build selects forgiving mode

- `CAP_SAFE_YIELD_STRICT    = 1 bsl 9`
  - Reserved; advertised only when transaction ops exist and the build selects strict mode

Implementation note:

- The build may expose **at most one** safe-yield capability bit (`FORGIVING` or `STRICT`), and only when transaction-style operations are supported.

### Current expected capability surface (with the current op matrix)

Given the current `ops.def` surface:

- `CAP_PUSHIMAGE` is **set** (because `pushImage` exists)
- `CAP_LAST_ERROR` is **set** when `getLastError` support is enabled
- `CAP_SPRITE` is **set** (because sprite ops exist in the current surface)
- `CAP_BATCH_VOID` is **unset** (no batch ops yet)
- `CAP_SAFE_YIELD_FORGIVING` is **unset** (no transaction ops)
- `CAP_SAFE_YIELD_STRICT` is **unset** (no transaction ops)
- `CAP_JPG_FILE` is **unset** (no JPEG file ops)
- `CAP_PNG_FILE` is **unset** (no PNG file ops)

---

## Diagnostics

### `getLastError()`

### Request

- `getLastError()` (LCD only; `Target` must be `0`)

### Response

```erlang
{ok, {last_error, LastOp, Reason, LastFlags, LastTarget, EspErr}}
```

Fields:

- `LastOp`
  - Last failing operation atom, or `none`

- `Reason`
  - Last error reason, or `none`

- `LastFlags`
  - Flags from the failing request (integer)

- `LastTarget`
  - Target from the failing request (integer)

- `EspErr`
  - `esp_err_t` integer, or `0`

Behavior:

- The driver snapshots the last-error state and returns it.
- On success, the last-error state is cleared after the response payload is successfully encoded.
- If last-error support is not compiled in, `getLastError` returns `{error, unsupported}` and `CAP_LAST_ERROR` must not be advertised.

---

## `pushImage` (RGB565)

### Request args

- `pushImage(Xi16, Yi16, Wu16, Hu16, StridePixelsU16, DataRgb565Binary)`

### Rules

- `StrideEff`:
  - if `StridePixelsU16 == 0`, `StrideEff = W`
  - else `StrideEff = StridePixelsU16`

- `StrideEff >= W`

- `byte_size(Data)` is even (2 bytes per pixel)

- Required minimum:
  - `byte_size(Data) >= StrideEff * H * 2`

- Trailing bytes may be present and are ignored

### Binary lifetime requirement

- If the underlying device path uses async DMA (or otherwise reads `Data` after the call returns), the driver must not pass a raw term-binary pointer directly unless it also guarantees completion before returning.
- Acceptable implementations are:
  - copy `Data` into a driver-owned DMA-safe buffer, or
  - block until the transfer completes

---

## `pushSprite` (sprite -> LCD blit)

This operation draws an existing sprite (named by `Target`) onto the LCD at a destination position.

- `Target` is the **source sprite handle** (`1..254`)
- `Target == 0` is invalid (`LGFX_OP_TARGET_SPRITE_ONLY`)
- Destination is the LCD (not another sprite)

### Request args

- `pushSprite(DstXi16, DstYi16)`
- `pushSprite(DstXi16, DstYi16, TransparentRgb565U16)` (optional transparent-key blit)

### Rules

- `DstXi16`, `DstYi16` are destination coordinates on the LCD
- Optional `TransparentRgb565U16` is an RGB565 color key (`u16`)
- When `TransparentRgb565U16` is provided, pixels matching that color are skipped
- Requires the sprite handle in `Target` to be currently allocated, else `{error, bad_target}` (or `{error, bad_args}` if your handler chooses that error shape consistently)

### Response

- Success: `{ok, ok}`
- Failure: `{error, Reason}`

---

## `pushSpriteRegion` (sprite region -> LCD blit)

This operation draws a rectangular region from an existing sprite (named by `Target`) onto the LCD at a destination position.

- `Target` is the **source sprite handle** (`1..254`)
- `Target == 0` is invalid (`LGFX_OP_TARGET_SPRITE_ONLY`)
- Destination is the LCD (not another sprite)

### Request args

- `pushSpriteRegion(DstXi16, DstYi16, SrcXi16, SrcYi16, Wu16, Hu16)`
- `pushSpriteRegion(DstXi16, DstYi16, SrcXi16, SrcYi16, Wu16, Hu16, TransparentRgb565U16)` (optional transparent-key blit)

### Rules

- `DstXi16`, `DstYi16` are destination coordinates on the LCD

- `SrcXi16`, `SrcYi16` are source coordinates inside the sprite

- `Wu16`, `Hu16` are source region size

- `Wu16` and `Hu16` must be non-zero

- Source rectangle validation is **strict**:
  - `SrcXi16 >= 0`
  - `SrcYi16 >= 0`
  - `SrcXi16 + Wu16 <= sprite_width(Target)`
  - `SrcYi16 + Hu16 <= sprite_height(Target)`
  - otherwise `{error, bad_args}`

- LCD edge clipping is allowed (off-screen destination pixels may be clipped by the device/LovyanGFX path)

- Optional `TransparentRgb565U16` is an RGB565 color key (`u16`)

- When `TransparentRgb565U16` is provided, pixels matching that color are skipped during compositing

- Requires the sprite handle in `Target` to be currently allocated, else `{error, bad_target}` (or `{error, bad_args}` if your handler chooses that error shape consistently)

### Response

- Success: `{ok, ok}`
- Failure: `{error, Reason}`

---

## `pushRotateZoom` (sprite -> LCD rotated/scaled blit)

This operation draws an existing sprite (named by `Target`) onto the LCD with rotation and scaling.

- `Target` is the **source sprite handle** (`1..254`)
- `Target == 0` is invalid (`LGFX_OP_TARGET_SPRITE_ONLY`)
- Destination is the LCD (not another sprite)
- Rotation uses the sprite pivot set by `setPivot`

### Request args

- `pushRotateZoom(DstXi16, DstYi16, AngleCentiDegI32, ZoomXX1024I32, ZoomYX1024I32)`
- `pushRotateZoom(DstXi16, DstYi16, AngleCentiDegI32, ZoomXX1024I32, ZoomYX1024I32, TransparentRgb565U16)` (optional transparent-key blit)

### Rules

- `DstXi16`, `DstYi16` are destination coordinates on the LCD

- `AngleCentiDegI32` is a fixed-point angle in **centi-degrees** (`i32`)
  - `100` = `1.00°`
  - `9000` = `90.00°`
  - `-4500` = `-45.00°`

- `ZoomXX1024I32` and `ZoomYX1024I32` are fixed-point zoom values in **x1024 units** (`i32`)
  - `1024` = `1.0x`
  - `512` = `0.5x`
  - `2048` = `2.0x`

- `ZoomXX1024I32 > 0` and `ZoomYX1024I32 > 0`, else `{error, bad_args}`

- The driver converts fixed-point values to the LovyanGFX/native representation using:
  - `angle_deg = AngleCentiDegI32 / 100.0`
  - `zoom_x = ZoomXX1024I32 / 1024.0`
  - `zoom_y = ZoomYX1024I32 / 1024.0`

- Optional `TransparentRgb565U16` is an RGB565 color key (`u16`)

- When `TransparentRgb565U16` is provided, pixels matching that color are skipped during compositing

- Requires the sprite handle in `Target` to be currently allocated, else `{error, bad_target}` (or `{error, bad_args}` if your handler chooses that error shape consistently)

- LCD edge clipping is allowed (off-screen destination pixels may be clipped by the device/LovyanGFX path)

### Response

- Success: `{ok, ok}`
- Failure: `{error, Reason}`

### Host guidance

- Set sprite pivot via `setPivot` before calling `pushRotateZoom` if you need stable rotation behavior (for example center rotation used by demo-style motion)
- Host wrappers should expose helpers for centi-degree / x1024 fixed-point conversion

---

## Planned / reserved features (not implemented yet)

The following sections describe intended protocol shapes. They are not implemented unless the corresponding capability bit is set in `FeatureBits`.

### `batchVoid()` (`CAP_BATCH_VOID`)

Intended to reduce message overhead by batching void-returning ops.

### Request

```erlang
{lgfx, ProtoVer, batchVoid, 0, 0, [SubCmd1, SubCmd2, ...]}
```

Sub-command shape:

```erlang
{Op, Target, Flags, Arg1, Arg2, ...}
```

Intended rules:

- Only ops that normally return `{ok, ok}` are allowed.
- No nesting.
- Execute strictly in order.
- Stop at first failing sub-command.
- No rollback.

### Response

- Success: `{ok, ok}`
- Failure: `{error, {batch_failed, FailedIndex, FailedOp, Reason}}`

---

## Host-side protocol smoke tests (recommended)

In addition to unit tests on the driver side, a tiny host-side smoke test is strongly recommended (for example from the Elixir demo app).

This catches protocol/implementation drift at the integration boundary (host ↔ driver), which is exactly where mismatches hurt most.

### Core checks (small but high value)

- **Target policy check**
  - Example: an op documented as `T0/unsupported` (such as `setColorDepth`) must return `{error, unsupported}` when `Target != 0`

- **Capability advertisement check**
  - If `pushImage` is implemented, `CAP_PUSHIMAGE` must be advertised in `getCaps`

- **Capability/availability consistency check**
  - `CAP_LAST_ERROR` bit must match actual `getLastError` behavior:
    - bit set + op available => valid
    - bit clear + `{error, unsupported}` => valid
    - all other combinations => drift

- **Sprite region path check**
  - `pushSpriteRegion` valid call succeeds; out-of-bounds source rect returns `{error, bad_args}`

- **Rotate/zoom path check**
  - `pushRotateZoom` valid fixed-point args (centi-deg + x1024 zoom) succeed; zero zoom returns `{error, bad_args}`

### Tiny protocol metadata self-test (future-proof)

A small metadata sanity check is recommended after `getCaps`:

- Verify `getCaps` returns the expected tuple shape:
  - `{caps, ProtoVer, MaxBinaryBytes, MaxSprites, FeatureBits}`

- Verify field types and ranges:
  - `ProtoVer` is an integer
  - `MaxBinaryBytes` is a positive integer
  - `MaxSprites` is a non-negative integer
  - `FeatureBits` is a non-negative integer

- Verify protocol version agreement:
  - Host wrapper protocol version constant should equal `ProtoVer`

- Verify required bits, not exact bit equality:
  - Check that bits required by the currently used features are set (for example `CAP_PUSHIMAGE`)
  - Avoid asserting an exact `FeatureBits` value in generic smoke tests, because future protocol-compatible builds may add new capability bits

This keeps the smoke test strict about correctness while remaining forward-compatible.

### Why this matters

These checks catch common regression classes quickly:

- `ops.def` metadata updated but handler behavior not updated
- handler added/removed but `getCaps` advertisement not updated
- compile-time gate toggled but capability bit not synchronized
- host wrapper protocol version drifting from the built driver

---

## Example host wrapper behavior (informative)

Typical host wrappers may add convenience behavior on top of the wire protocol, for example:

- caching `getCaps().max_binary_bytes`
- caching text state (`setTextColor`, `setTextSize`) to reduce repeated port calls
- chunking `pushImage` automatically when a pixel blob exceeds `MaxBinaryBytes`
- caching sprite capability / max sprite count and refusing sprite calls early when `CAP_SPRITE` is absent

These are host-side optimizations only.

They must not change the wire contract defined in this document.

In particular:

- `pushImage` still remains the protocol operation on the wire
- `pushSprite` / `pushRotateZoom` still remain the protocol operations on the wire
- chunking is a host implementation detail (multiple valid `pushImage` calls)

---

## Maintenance checklist (when adding or changing an op)

When you add or change an operation:

- Update `ports/include/lgfx_port/ops.def`
  - atom
  - handler
  - arity
  - flags mask
  - target policy
  - state policy
  - `feature_cap_bit`

- Implement or update the handler

- If adding/changing protocol capability bits, update `ports/include/lgfx_port/caps.h`

- If adding/changing protocol error atoms or detail tags, update `ports/include/lgfx_port/errors.h`

- Regenerate/sync protocol doc tables:
  - `elixir scripts/sync_lgfx_protocol_doc.exs`

- Verify `getCaps` advertisement matches the new `feature_cap_bit`

- If the change introduces a new public capability or behavior, update:
  - **Capabilities**
  - **Planned / reserved features** (if relevant)
  - **Compatibility rules** (if needed)

---

## Protocol compatibility rules

### Allowed (compatible) changes

- Add new operations (`Op` atoms)
- Add new capability bits
- Add new error reasons (without reinterpreting existing ones)
- Tighten validation if it only rejects requests already invalid per this document
- Add optional error detail tuples while preserving `{error, Reason}` outer shape

### Not allowed (breaking) changes

- Change the request tuple shape `{lgfx, ProtoVer, Op, Target, Flags, ...}` for existing operations
- Change argument order/meaning for existing operations
- Reinterpret existing flags
- Change RGB565 pixel byte order for `pushImage`
