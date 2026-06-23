## Context

Carrot is a C++23 event-loop gateway server using io_uring. Currently it has a single-threaded DispatcherImpl, a hardcoded HTTP echo connection handler, and no extension mechanism. The build system uses Bazel with custom macros in `bazel/build_system.bzl`. This design covers the extension architecture, the gateway worker model, the wire protocol, and the node agent — the foundation for a distributed serverless runtime.

## Goals / Non-Goals

**Goals:**
- Define a static-init extension registry with type-safe category maps
- Support per-executable extension selection via a flat `.bzl` file
- Redesign gateway as main thread + N share-nothing worker threads
- Define mutable, pre-indexed NodeDirectory per worker, updated via commands forwarded from the main thread
- Define a TLV + msgpack wire protocol for gateway↔node communication
- Design the node agent as a standalone executable with its own extension registry
- Keep core (event loop, networking, logging, wire protocol) shared between executables

**Non-Goals:**
- Passive node discovery (node connects to gateway) — deferred
- Asynchronous result delivery (POST to endpoint, file:///dev/null drop) — deferred
- Hot-reload of config — deferred
- Task isolation models beyond best-effort (process/thread/WASM) — node agent implementation details
- Performance benchmarking or optimization — this is the structural foundation

## Decisions

### D1: Extension Registry — Static Init with Macros

Use file-scope static registration via `CARROT_REGISTER_EXTENSION(category, name, Factory)`.

Alternatives considered:
- **Build-time codegen** (genrule reads .bzl, generates registration .cc): more explicit, avoids static init, but requires a custom Bazel rule and adds a build step for every extension change
- **Explicit registration in main()**: requires editing a central file when adding/removing extensions — scales poorly

Chosen because extension factories are stateless and don't depend on other extensions, so init order is irrelevant. Each extension library uses `alwayslink = True` to prevent linker from stripping the static registration object.

### D2: Per-Executable Extension Lists in .bzl

Each executable has a flat list of Bazel labels in its own `.bzl` file:

```python
# extensions_gateway.bzl
GATEWAY_EXTENSIONS = [
    "//ext/gateway/task_sources:http",
    "//ext/gateway/schedulers:round_robin",
    "//ext/gateway/discovery:static",
]
```

Commenting out a line removes the extension from the binary. The category and name are declared by the extension itself in the macro — the build file only says "link this."

### D3: Main Thread + Worker Threads

- **Main thread**: owns a small io_uring ring (~64 SQEs), runs NodeDiscovery extensions (IOObjects), forwards `ADD_NODE`/`REMOVE_NODE`/`UPDATE_NODE` commands to each worker's eventfd, manages worker lifecycle
- **Workers**: each owns a 4096-entry io_uring ring, listens on SO_REUSEPORT :8081, runs TaskSource + Scheduler extensions, maintains per-worker connection pools to nodes
- Main thread creates workers with an initial (possibly empty) NodeDirectory before entering its own event loop

Alternatives considered:
- **Single-threaded event loop**: simpler but cannot scale beyond one core
- **Reactor-per-worker with shared accept**: adds complexity for connection migration

### D4: NodeDirectory — Mutable, Command-Updated

- Each worker owns a mutable `NodeDirectory` updated only during its own command queue drain — no synchronization needed
- The main thread forwards `ADD_NODE(info)`, `REMOVE_NODE(id)`, `UPDATE_NODE(info)` commands to each worker via eventfd
- Workers process commands sequentially during their queue drain: `addNode` → also triggers `ConnectionPool::connect`, `removeNode` → also triggers `ConnectionPool::disconnect`
- Pre-indexed: `byType(type) → span<NodeInfo*>`, `byId(id) → NodeInfo*`
- Available (dynamic) resource capacity tracked per-worker in ConnectionPool
- The Scheduler receives pre-filtered candidates from NodeDirectory + liveness/capacity from ConnectionPool

### D5: ResultReceiver Owned by TaskSource

- `TaskSource` creates a `ResultReceiver` bound to the user's connection when a request arrives
- Core calls `receiver.sendChunk(chunk, isFinal)` as chunks arrive from the node
- `TaskSource` owns the user connection lifecycle (fd, io_uring read/write SQEs)
- The core doesn't know about HTTP, gRPC, or any protocol — it only calls `sendChunk`

### D6: Wire Protocol — TLV + msgpack

- Binary protocol over TCP between gateway and node agent
- 1-byte type, 4-byte length (network byte order), variable-length msgpack-encoded value
- Message types: SUBMIT_TASK (0x01), CHUNK (0x02), COMPLETE (0x03), ERROR (0x04), REGISTER (0x05), HEARTBEAT (0x06)
- Serialization is in core, not an extension

Alternatives considered:
- **Protobuf**: codegen dependency, heavier build integration
- **FlatBuffers**: zero-copy reads but more complex schema management
- **Custom binary format**: flexible but reinvents serialization

msgpack is chosen as a well-known, lightweight, header-only serialization library with C++ support.

### D7: NodeDiscovery — Push-Based, Main-Thread IOObject

- Extensions implement IOObject and register with the main thread's dispatcher
- On node lifecycle events, they call a handler with `onNodeAdded(info)`, `onNodeRemoved(id)`, `onNodeUpdated(info)`
- The main thread forwards each event as a command to every worker's eventfd
- Workers drain the command during their next io_uring wait cycle and apply it to their local NodeDirectory + ConnectionPool

### D8: Worker Failure — Crash on Failure

During the development phase, if a worker thread dies unexpectedly the main thread SHALL crash immediately to surface errors early. A configurable recovery policy (restart, drain, ignore) is deferred.

### D9: Node Disconnect — Fail In-Flight Tasks

When a node connection drops, all in-flight tasks assigned to that node SHALL be failed with an error. A configurable retry policy is deferred — for now the user receives the error and may resubmit.

## Risks / Trade-offs

- **[Static init order]**: If an extension factory depends on another extension's presence, static init order is undefined. **Mitigation**: extension factories MUST be stateless and MUST NOT depend on other extensions. Enforced by convention and code review.
- **[alwayslink bloat]**: Every extension uses `alwayslink = True`, so the linker cannot strip unused code from extension libraries. **Mitigation**: compile-time selection via `.bzl` means unused extensions simply aren't linked at all.
- **[Per-worker connection redundancy]**: N workers × M nodes = N×M TCP connections. **Mitigation**: deferred — a dedicated upstream I/O thread could consolidate connections later.
- **[msgpack performance]**: msgpack is not zero-copy; every message is serialized/deserialized through intermediate buffers. **Mitigation**: acceptable for an initial implementation; can be optimized or replaced later without changing the TLV framing.
- **[SO_REUSEPORT uneven load]**: Linux distributes accepts unevenly under high load. **Mitigation**: acceptable for initial implementation; can add `SO_ATTACH_REUSEPORT_CBPF` for balanced distribution later.

## Open Questions

- Should ConnectionPool expose available capacity to Scheduler as a simple numeric score, or as raw resource data for custom scheduling logic? TBD when implementing Scheduler interface.
