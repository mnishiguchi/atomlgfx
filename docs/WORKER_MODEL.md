# Worker model

## Summary

`lgfx_port` uses two execution contexts with a strict ownership split:

- The port thread owns AtomVM-facing work:
  - mailbox drain
  - request decode
  - metadata validation
  - handler wire decode
  - reply encoding
  - `last_error`

- The worker task owns device execution only:
  - receive plain C jobs
  - run `lgfx_device_*`
  - free copied payloads
  - notify the waiting caller

Core rules:

- AtomVM terms and mailbox objects stay on the port thread.
- The worker task stays term-free.
- Worker wrappers are synchronous.
- Variable-length payloads are deep-copied before enqueue.
- Queue-by-pointer is safe only because wrappers block until completion.
- Handlers decode protocol arguments, but device semantics stay in the device layer.

That is the whole model.

## Big picture

```text
AtomVM world                           Device world
----------------------------          ----------------------------
mailbox / terms / replies             FreeRTOS task / plain C args

message
  |
  v
+---------------------------+
| port thread               |
| - drain mailbox           |
| - decode request          |
| - metadata validate       |
| - decode handler args     |
| - build C job             |
| - enqueue                 |
| - wait                    |
+-------------+-------------+
              |
              | lgfx_job_t *
              v
+---------------------------+
| worker queue              |
+-------------+-------------+
              |
              v
+---------------------------+
| worker task               |
| - run lgfx_device_*       |
| - write err               |
| - free owned_payload      |
| - notify caller           |
+-------------+-------------+
              |
              v
+---------------------------+
| port thread resumes       |
| - encode reply            |
| - send reply              |
+---------------------------+
```

## Ownership

### Port thread

The port thread owns all protocol-facing work.

It:

- drains the AtomVM mailbox
- decodes request tuples into `lgfx_request_t`
- applies metadata-driven validation
- decodes handler arguments
- calls `lgfx_worker_device_*` wrappers
- waits for completion
- builds and sends replies

It also owns protocol state:

- request shape and version checks
- op lookup
- arity, flags, target, and init-state checks
- `last_error`

Handlers stay intentionally narrow:

- decode wire arguments
- reject clearly malformed wire input
- avoid duplicating device-facing semantic checks

Examples of checks that do **not** belong in handlers:

- sprite existence
- destination sprite existence
- `pushImage` stride and byte-count semantics
- rotate/zoom semantic validity beyond basic wire decode

### Worker task

The worker task owns device execution only.

It:

- receives `lgfx_job_t *`
- dispatches on `job->kind`
- runs the matching `lgfx_device_*`
- writes `job->err`
- frees `job->owned_payload` when present
- notifies the waiting caller

It must not:

- decode AtomVM terms
- access mailbox objects
- build protocol replies
- own protocol validation rules

The worker ABI is plain-C transport between the port thread and the device layer. For example, `pushRotateZoom` carries fixed-point protocol values through the worker path and converts near the device call boundary.

## Mailbox ownership

Mailbox ownership stays entirely on the port thread.

Mailbox draining lives in `lgfx_port.c`.

It:

- pulls messages from the AtomVM mailbox
- forwards normal payloads to the request handler
- disposes mailbox messages on the port thread

The worker task never touches mailbox objects.

## Request flow

```text
1. AtomVM sends a message to the port mailbox
2. port thread drains the mailbox
3. port thread decodes the request tuple
4. port thread validates the protocol envelope and op metadata
5. handler decodes op-specific wire arguments
6. handler calls lgfx_worker_device_* wrapper
7. wrapper builds lgfx_job_t
8. wrapper enqueues the job and blocks
9. worker task runs lgfx_device_*
10. worker stores err, frees payload, notifies caller
11. wrapper resumes and returns esp_err_t
12. handler maps the result to a protocol reply
13. port thread sends the reply
```

## Job model

`worker_jobs.def` is the canonical worker job list.

It is the single source of truth for:

- job kind enum entries
- union members
- worker dispatch bodies

`worker_jobs.h` materializes that into:

- `lgfx_job_kind_t`
- `lgfx_job_t`

Conceptually:

```text
lgfx_job_t
├─ kind
├─ notify_task
├─ err
├─ owned_payload
└─ union a
   ├─ init
   ├─ get_dims
   ├─ draw_pixel
   ├─ draw_string
   ├─ push_image
   ├─ push_rotate_zoom
   └─ ...
```

Important fields:

- `kind`
  - which worker action to run

- `notify_task`
  - caller task handle, set before enqueue

- `err`
  - result written by the worker

- `owned_payload`
  - optional copied byte buffer owned by the job

## Why synchronous completion matters

Wrappers are synchronous by design.

Today they usually build `lgfx_job_t` on the stack and enqueue a pointer to that stack object. Because of that, the wrapper must not return until the worker is done.

Current contract:

- wrapper builds stack job
- wrapper enqueues `lgfx_job_t *`
- wrapper blocks
- worker finishes
- worker notifies caller
- wrapper resumes and returns

Compact rule:

```text
stack job + queue-by-pointer + async return = use-after-free
```

So the model stays synchronous.

## Payload ownership

Some jobs carry variable-length bytes, such as:

- `draw_string`
- `push_image_rgb565_strided`

Those bytes may come from caller-owned memory, including Erlang binaries. Wrappers must not pass those raw pointers through the queue.

Payload-bearing wrappers do this:

- allocate heap memory
- copy the bytes
- store the copy in `job->owned_payload`
- point the job fields at the copy
- enqueue the job
- block until completion

Then the worker:

- calls `lgfx_device_*` with the copied bytes
- frees `job->owned_payload`
- notifies the caller

```text
caller bytes
    |
    | copy
    v
job->owned_payload
    |
    v
lgfx_device_* call
    |
    +--> worker frees payload --> notify caller
```

## Important assumption

The current cleanup model assumes the device path fully consumes the payload before `lgfx_device_*` returns.

That matches the current implementation.

If a future backend introduces DMA or any other async transfer that outlives the device call, this model must change. Payload release would then need to move to the real completion boundary, not immediate return from `lgfx_device_*`.

## Stop and shutdown

Worker shutdown is also synchronous.

Sequence:

- mark worker as stopping
- record `stop_notify_task`
- send `NULL` as the stop sentinel
- wait for worker acknowledgement
- delete queue
- clear `port->worker`
- free worker state

The worker exits when it receives the `NULL` sentinel, notifies the stopper, and deletes itself.

## Error flow

Worker wrappers return `esp_err_t`. They do not build protocol replies.

Mapping stays on the port side:

```text
lgfx_device_* -> esp_err_t -> worker wrapper -> handler -> protocol reply
```

This keeps:

- worker code protocol-agnostic
- device code focused on device semantics
- reply mapping centralized on the port side

## Boundaries by layer

### Port thread / handlers

Belongs here:

- AtomVM term decoding
- tuple shape interpretation
- protocol validation
- op metadata checks
- reply construction
- `last_error`
- wire decode
- binary-size caps

### Worker

Belongs here:

- enqueue and dequeue
- synchronous execution of `lgfx_device_*`
- copied payload lifetime
- plain C argument transport
- worker task lifecycle

### Device layer

Belongs here:

- LovyanGFX-backed operations
- target resolution
- resource allocation
- device-specific validation
- driver state management

Examples:

- deterministic `createSprite` at the caller-selected handle
- destination-aware `pushSprite` / `pushRotateZoom`
- `pushImage` stride and payload checks
- rotate/zoom semantic validation

## Rules for adding a new worker job

When adding a new worker-visible operation:

- add one row to `worker_jobs.def`
- keep the row direct and boring
- add the wrapper in `lgfx_worker_device.c`
- keep AtomVM decoding in handlers, not wrappers
- keep AtomVM and FreeRTOS types out of `worker_jobs.def`
- deep-copy variable-length payloads before enqueue
- preserve synchronous completion unless the ownership model is redesigned

## When async requires redesign

Async completion requires changing the ownership model first.

That means at least one of:

- heap-owned or pool-owned jobs
- queue-by-value instead of queue-by-pointer

and explicit rules for:

- job lifetime
- payload lifetime
- completion notification
- timeout or cancellation

Until then, synchronous queue-by-pointer is the intended model.
