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
  - `1..254` => sprite handle (reserved for future sprite ops)
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
- `u16`: `0..65535`
- `u32`: `0..4294967295`

Coordinates and sizes:

- `x`, `y`: `i16`
- `w`, `h`: `u16`
- angles: `i16` degrees

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
| `fillScreen` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
| `clear` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
| `drawPixel` | `T0/unsupported` | `F0` | `8` | `requires_init` | - |
| `drawFastVLine` | `T0/unsupported` | `F0` | `9` | `requires_init` | - |
| `drawFastHLine` | `T0/unsupported` | `F0` | `9` | `requires_init` | - |
| `drawLine` | `T0/unsupported` | `F0` | `10` | `requires_init` | - |
| `drawRect` | `T0/unsupported` | `F0` | `10` | `requires_init` | - |
| `fillRect` | `T0/unsupported` | `F0` | `10` | `requires_init` | - |
| `drawCircle` | `T0/unsupported` | `F0` | `9` | `requires_init` | - |
| `fillCircle` | `T0/unsupported` | `F0` | `9` | `requires_init` | - |
| `drawTriangle` | `T0/unsupported` | `F0` | `12` | `requires_init` | - |
| `fillTriangle` | `T0/unsupported` | `F0` | `12` | `requires_init` | - |
| `setTextSize` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
| `setTextDatum` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
| `setTextWrap` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
| `setTextFont` | `T0/unsupported` | `F0` | `6` | `requires_init` | - |
| `setTextColor` | `T0/unsupported` | `Fmask(LGFX_F_TEXT_HAS_BG)` | `6/7` | `requires_init` | - |
| `drawString` | `T0/unsupported` | `F0` | `8` | `requires_init` | - |
| `pushImage` | `T0/unsupported` | `F0` | `11` | `requires_init` | `LGFX_CAP_PUSHIMAGE` |
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
| `LGFX_CAP_SPRITE` | `CAP_SPRITE` | `0` | `0x0001` | reserved / not currently op-linked |
| `LGFX_CAP_PUSHIMAGE` | `CAP_PUSHIMAGE` | `1` | `0x0002` | `ops.def` feature_cap_bit |
| `LGFX_CAP_JPG_FILE` | `CAP_JPG_FILE` | `2` | `0x0004` | reserved / not currently op-linked |
| `LGFX_CAP_PNG_FILE` | `CAP_PNG_FILE` | `3` | `0x0008` | reserved / not currently op-linked |
| `LGFX_CAP_LAST_ERROR` | `CAP_LAST_ERROR` | `4` | `0x0010` | `ops.def` feature_cap_bit |
| `LGFX_CAP_BATCH_VOID` | `CAP_BATCH_VOID` | `5` | `0x0020` | reserved / not currently op-linked |
| `LGFX_CAP_SAFE_YIELD_FORGIVING` | `CAP_SAFE_YIELD_FORGIVING` | `8` | `0x0100` | build option (`LGFX_PORT_SAFE_YIELD_CAP`) |
| `LGFX_CAP_SAFE_YIELD_STRICT` | `CAP_SAFE_YIELD_STRICT` | `9` | `0x0200` | build option (`LGFX_PORT_SAFE_YIELD_CAP`) |
<!-- END:generated_caps_table -->

Semantics and usage notes:

- `CAP_SPRITE        = 1 bsl 0`
  - Reserved for sprite support (not currently advertised)

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
- `CAP_SPRITE` is **unset** (no sprite ops yet)
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

These are host-side optimizations only.

They must not change the wire contract defined in this document.

In particular:

- `pushImage` still remains the protocol operation on the wire
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
