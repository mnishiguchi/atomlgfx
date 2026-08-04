<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0006: Flatten native v2 implementation

## Status

Accepted

This ADR remains active for the native implementation shape.

Batching language should be read with [ADR 0010: Treat BinaryBatch as the standard render transaction API](0010-binary-batch-as-render-transaction-api.md): current v2 has an explicit binary frame-script path through `submitBinaryBatch`, not a public tuple/list batch runtime.

## Context

`atomlgfx` v2 already has the desired external protocol direction:

- one request tuple shape
- numeric opcodes before crossing into native code
- `ops.def` as the protocol-visible operation metadata source
- ordinary operations on a direct synchronous path
- explicit batching as an opt-in grouped control-plane path
- binary operations reserved for repeated hot-path data-plane workloads

The current native implementation is correct in shape but larger than ideal for
a thin LovyanGFX wrapper. The native tree is split across separate files for
AtomVM atoms, request validation, protocol term helpers, operation registry,
batch decoding, batch builders, many category-specific ordinary handlers,
runtime state, runtime command dispatch, open config, and device adapters.

That split made the v2 rewrite easier to stage, but it now creates navigation
and maintenance overhead. Many ordinary handlers are mostly wire decoding plus
one call into `lgfx_device_*`. The batch runtime was also intentionally narrow:
ordered execution, no general scheduler, and no requirement that ordinary
operations become queue-backed.

The design question is whether the v2 protocol should keep this native layering
or collapse it into a flatter implementation that better reflects the actual
scope of the driver.

## Decision

Flatten the handwritten native v2 implementation around the existing protocol
model.

Keep these protocol and ownership rules:

- Elixir API names are mapped to generated numeric opcodes before native calls.
- Native dispatch uses numeric opcodes and never receives API-name strings or
  dynamically created atoms on the hot path.
- `ops.def` remains the source of truth for opcode order, arity, flags, target
  policy, state policy, capability linkage, and batch eligibility.
- Ordinary operations remain direct and synchronous.
- Packed scalar batch remains explicit, opt-in, and narrower than the ordinary surface.
- Specialized binary operations remain the preferred path for repeated homogeneous animation
  data where packed scalar batch decode cost is measurable.
- Borrowed binary payloads remain request-scoped unless explicitly copied into
  native-owned storage.

Prefer this target shape for the native implementation:

```text
include/
  lgfx_port/
    protocol.h
    ops.h

lgfx_port/
  lgfx_port.c        # port lifecycle, mailbox drain, request/reply flow
  protocol.c         # term decode/encode and common validation
  ops.c              # ops.def metadata and dispatch
  handlers.c         # ordinary op handlers
  binary_batch_dispatch.cpp # submit_binary_batch stream preflight and execution
  open_config.c      # open_port options only

lgfx_device/
  lgfx_device.h
  device.cpp         # state, init, close, target resolution
  drawing.cpp        # primitives and clip
  text.cpp
  images.cpp
  sprites.cpp
  touch.cpp          # optional; fold into device.cpp if it stays tiny
```

This is an implementation target, not a new public protocol. The exact file
names may vary if measurement or C/C++ boundaries justify it, but the native
shape should stay flatter than the current staged rewrite layout.

## Rationale

The v2 value is the protocol model, not a large native framework around it.

The hot path should stay simple:

```text
decode request envelope
lookup op metadata
validate arity, flags, target, and init state
decode op-specific args
call lgfx_device_*
reply
```

Splitting every operation family into a separate ordinary handler file is not
buying much isolation when the handlers mostly decode scalars and forward to the
device layer. A single `handlers.c` is easier to search and review while the
surface remains modest.

Similarly, request validation should remain metadata-driven but does not need a
separate mini-framework. A small protocol helper layer is enough:

```c
bool lgfx_decode_request(Context *ctx, term input, lgfx_request_t *request);
term lgfx_validate_request(Context *ctx, lgfx_port_t *port, const lgfx_request_t *request);
term lgfx_reply_ok(Context *ctx, term value);
term lgfx_reply_error(Context *ctx, term reason);
```

The dispatch mechanism should be chosen by measured code size and clarity. A
generated metadata table is still useful. A generated `switch` may be better
than a handler function-pointer table if it reduces flash size or lets the
compiler inline tiny handlers. If the function-pointer table is smaller, keep
it.

For batch execution, the main simplification goal is to avoid pretending there
is a broader runtime than the driver actually needs. The implementation may
execute packed scalar batches synchronously on the port thread if that preserves
the documented `submit_binary_batch` behavior. The current public contract is
completion-reporting: success means the packed stream was decoded and executed.

## Consequences

### Positive

- Reduces native file count and navigation overhead
- Keeps the protocol metadata while deleting staged rewrite scaffolding
- Makes ordinary handler flow easier to audit
- Keeps the C hot path focused on compact numeric dispatch
- Removes the runtime-shaped batch layer instead of growing it into a general
  job system
- Makes future code-size measurement easier because fewer layers are involved

### Negative

- Larger combined files may need clear sectioning to stay readable
- Some merge history and blame granularity will be less category-specific
- Collapsing runtime pieces requires care around batch failure reporting
- Changing `submit_binary_batch` completion semantics would be externally visible and
  cannot be treated as a private refactor
- A generated switch may not improve code size on every compiler/configuration

## Rejected alternatives

### Alternative 1: keep the current staged native layout indefinitely

Rejected.

The current layout is clean but too structured for the amount of real behavior
in a thin LovyanGFX wrapper. It preserves staging boundaries that are no longer
all useful after the v2 model is understood.

### Alternative 2: pass operation names into C and dispatch by strings or atoms

Rejected.

The v2 protocol intentionally maps Elixir operation names to numeric opcodes
before crossing the port boundary. Native dispatch should not compare API-name
strings or atoms on the hot path.

### Alternative 3: widen packed batch into a general graphics VM

Rejected.

Packed scalar batch is useful for grouped control-plane work, but repeated
homogeneous animation data should use fixed-layout binary operations where
measurement justifies it. Broadening packed batch would increase decode
complexity in the wrong representation.

### Alternative 4: remove `lgfx_runtime` without preserving batch semantics

Rejected.

Runtime removal is acceptable only if the externally documented batch contract
is preserved or deliberately changed through a separate protocol decision.
Implementation flattening must not accidentally change caller-visible
`submit_binary_batch` behavior.

## Follow-up implications

- Measure function-pointer dispatch against generated `switch` dispatch before
  choosing one permanently.
- Keep `submit_binary_batch` completion-reporting unless a later protocol ADR changes it.
- Keep packed scalar batch limited to scalar/no-payload control-plane commands.
- Add focused protocol tests for bad opcode, bad flags, bad target, bad args,
  uninitialized drawing ops, and malformed batch commands.
- Update `docs/architecture.md` and `lgfx_port/README.md` as the implementation
  is flattened.
