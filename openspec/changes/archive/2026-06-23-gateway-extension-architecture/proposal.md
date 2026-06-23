## Why

Carrot's current gateway is a single-threaded echo server hardcoded to HTTP on port 8081. To become a distributed runtime for serverless tasks, it needs a pluggable extension architecture, a multi-worker event-loop model, node-side task execution, and a wire protocol for gateway↔node communication.

## What Changes

- Introduce a **static-init extension registry** with macros — extensions register their category, name, and factory at compile time; the flat `.bzl` file controls what is linked into each binary
- Define core extension categories: `TaskSource`, `Scheduler`, `NodeDiscovery` (gateway) and `TaskHandler` (node agent)
- Rework gateway into a **main thread + N worker threads** architecture:
  - Main thread: owns a small io_uring ring, runs `NodeDiscovery` extensions, forwards `ADD_NODE`/`REMOVE_NODE`/`UPDATE_NODE` commands to each worker's eventfd
  - Workers: share-nothing, SO_REUSEPORT listener, own io_uring ring (4096 entries), run `TaskSource` + `Scheduler`, maintain per-worker connection pools to nodes
- Define a **TLV + msgpack wire protocol** between gateway and node agents
- Create a **node agent** executable that accepts tasks from the gateway and dispatches them to pluggable `TaskHandler` extensions
- Make `ResultReceiver` an interface owned by `TaskSource` extensions — the task source that accepted the user connection also writes the result back to it
- Introduce `NodeDirectory` as a mutable, pre-indexed data structure (by task type and by id) per worker, updated via commands forwarded from the main thread
- Add config structs: `Task { type, body, function?, result_endpoint?, requirements }`, `Chunk { data }`, `NodeInfo { id, address, task_types, max_capacity }`

## Capabilities

### New Capabilities

- `extension-registry`: Static-init extension registry with `CARROT_REGISTER_EXTENSION` macro, type-safe category maps, and per-executable `.bzl` build config
- `task-source`: Abstract interface for pluggable ingress protocols (HTTP, gRPC, etc.). Produces `Task` + `ResultReceiver` per user request
- `scheduler`: Abstract interface for node selection given a `Task` and eligible node list
- `node-discovery`: Abstract interface for push-based node catalog maintenance on the main thread
- `node-directory`: Mutable, pre-indexed node catalog per worker, updated via commands forwarded from the main thread — core data structure, not an extension
- `gateway-worker-model`: Main thread lifecycle, worker creation, config distribution, event-loop integration
- `wire-protocol`: TLV + msgpack framing and message types for gateway↔node communication
- `node-agent`: Standalone executable with its own extension registry for `TaskHandler` plugins
- `task-handler`: Abstract interface for task execution on node agents (echo, WASM, subprocess, etc.)
- `result-receiver`: Interface for streaming `Chunk`s back through the originating task source connection
- `connection-pool`: Per-worker pool of active TCP connections to nodes, with liveness tracking

### Modified Capabilities

*(none — this is the first set of specs)*

## Impact

- `src/exe/gateway/gateway.cc` — complete rewrite to main/worker model with extension loading from YAML config
- `src/core/event/dispatcher_impl.hh` — add constructor parameter for ring size
- `include/carrot/common/` — new `extension_registry.hh` header
- New interfaces in `include/carrot/gateway/` and `include/carrot/nodeagent/`
- New directory `ext/gateway/` and `ext/nodeagent/` for extension implementations
- `bazel/build_system.bzl` — new `carrot_extension` macro with `alwayslink = True`
- New Bazel build file `extensions_gateway.bzl` for selecting gateway extensions
- New `yaml-cpp` dependency for config parsing
- New `msgpack` dependency for wire protocol serialization
- New executable `//src/exe/nodeagent:nodeagent`
