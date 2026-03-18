<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# lgfx_port

`lgfx_port/` is the AtomVM-facing native layer.

It owns:

- request tuple handling
- metadata-driven validation
- handler dispatch
- reply mapping
- the worker bridge used to execute device work safely

See [the protocol spec](../docs/protocol.md) for wire-level rules and
[the architecture overview](../docs/architecture.md) for the top-level repository map.

## File map

- `lgfx_port.c`
  - port entrypoint
  - mailbox drain
  - per-port lifecycle
  - request dispatch

- `proto_term.c`
  - request and reply term helpers

- `handlers/*.c`
  - op-specific wire decode
  - handler-to-worker bridge

- `worker_core.c`
  - worker task lifecycle
  - queue management
  - stop and shutdown flow

- `worker_device.c`
  - synchronous worker wrappers
  - plain-C job construction

- `include_internal/lgfx_port/ops.def`
  - protocol-visible operation metadata

- `include_internal/lgfx_port/worker_jobs.def`
  - worker job metadata

## Responsibility split

### Port thread

The port thread owns all AtomVM-facing work:

- mailbox handling
- tuple decoding
- metadata validation
- handler dispatch
- reply encoding
- protocol-visible error state such as `last_error`

### Worker task

The worker task owns device execution only:

- receive plain C jobs
- run `lgfx_device_*`
- free copied payloads
- notify the waiting caller

The worker must stay term-free.

### Device layer

Detailed device semantics belong in `../lgfx_device/`, not here.

Examples:

- sprite existence and allocation rules
- target resolution
- palette-backed behavior
- `pushImage` payload semantics
- rotate/zoom semantic validity

## Request flow

```text
AtomVM message
  -> port thread decodes and validates
  -> handler decodes wire args
  -> handler calls worker wrapper
  -> wrapper builds lgfx_job_t
  -> wrapper enqueues and waits
  -> worker executes lgfx_device_*
  -> worker writes err and frees payload
  -> worker notifies caller
  -> handler maps result to protocol reply
```

## Core rules

- AtomVM terms never cross into the worker task.
- Worker wrappers are synchronous.
- Queue-by-pointer is safe only because wrappers wait for completion.
- Variable-length payloads must be deep-copied before enqueue.
- Handlers decode wire arguments, but should not duplicate device semantics.

## Job model

`worker_jobs.def` is the canonical worker job list.

It drives:

- job kind enum entries
- union members in `lgfx_job_t`
- worker dispatch bodies

Important job fields:

- `kind`
  - which worker action to run

- `notify_task`
  - caller task handle

- `err`
  - result written by the worker

- `owned_payload`
  - optional copied byte buffer owned by the job

## Payload ownership

Wrappers must not enqueue raw pointers into caller-owned term memory.

Current rule:

- allocate and copy payload bytes before enqueue
- point job fields at the owned copy
- free the owned copy in the worker after `lgfx_device_*` returns

This applies especially to text, JPEG, and RGB565 image payloads.

## Why completion stays synchronous

Wrappers commonly enqueue pointers to stack-allocated `lgfx_job_t`.

That means the wrapper must not return before the worker finishes.

```text
stack job + queue-by-pointer + async return = use-after-free
```

Until ownership changes, synchronous completion is required.

## Shutdown model

Worker shutdown is also synchronous:

- mark worker as stopping
- send `NULL` stop sentinel
- wait for acknowledgement
- delete queue
- clear worker pointer
- free worker state

## When changing this layer

When adding a new worker-visible operation:

- add one row to `worker_jobs.def`
- add the wrapper in `worker_device.c`
- keep AtomVM decoding in handlers
- keep AtomVM and FreeRTOS types out of `worker_jobs.def`
- deep-copy variable-length payloads before enqueue
- preserve synchronous completion unless the ownership model is redesigned
