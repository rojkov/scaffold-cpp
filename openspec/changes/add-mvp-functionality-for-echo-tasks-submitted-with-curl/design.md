## Context

The gateway architecture (workers with HttpTaskSource, ConnectionPool, Scheduler) and node agent (listener, TaskHandler) are implemented but the data pipeline has three breaks:

1. **HttpTaskSource** allocates zero-byte read buffers (`PrepareRead(this, fd, {}, 0)`) — reads return 0 bytes and `onRequest` is never called
2. **ConnectionPool::addNode** creates a socket but never issues `connect()` — writes to the fd from `Worker::onTaskReady` fail silently
3. **NodeAgentConnection** also allocates zero-byte read buffers — agent never receives tasks
4. **Static discovery** iterates the top-level config node instead of the `nodes` sub-key — node entries are never parsed

## Goals / Non-Goals

**Goals:**
- Curl can submit an HTTP POST to the gateway and receive an echo response via the full pipeline: gateway → node agent → echo handler → response
- Graceful shutdown on SIGINT/SIGTERM
- Gateway + node agent configs match the actual parser expectations

**Non-Goals:**
- Not a production-quality implementation (minimal error handling, single-frame reads, no connection retry)
- No TLS, no keep-alive, no HTTP/2
- No integration tests in this change

## Decisions

### 1. Per-connection read buffers tracked by fd in HttpTaskSource
Instead of creating a separate IOObject per client connection, keep buffers in a `std::unordered_map<int, std::vector<std::byte>>`. This is minimal and avoids restructuring the HandleCompletion dispatch. The `HandleCompletion` distinguishes accept completions (`res` is a valid fd) from read completions (`res` is a byte count) by checking whether the CQE's user_data belongs to the accept path or a read buffer.

### 2. ConnectionPool uses synchronous connect for MVP
`ConnectionPool::addNode` creates the socket and issues a blocking `connect()` inline. Since node agents run on localhost during development, the connect completes in microseconds. The socket is created blocking, connected, then set to `O_NONBLOCK`. This avoids the cross-thread SQE submission problem (addNode runs on the main thread while each Worker's io_uring ring lives on a worker thread). The async connect via `PrepareConnect` is deferred — tracked in `TODO.md`.

### 3. NodeAgentConnection: fixed-size 4096-byte read buffers
Same approach as HttpTaskSource. The wire protocol uses a 5-byte header + payload, and echo tasks are small. A single 4KB read is sufficient for MVP.

### 4. Static discovery drills into `nodes` key
The YAML config for node_discovery is `{type: static, nodes: [...]}`. The constructor receives the full `node_discovery` node, so it must access `cfg["nodes"]` to get the list.

### 5. SignalMonitor already wired — no changes needed
`main_dispatcher` creates a SignalMonitor and calls Run(). The monitor's HandleCompletion calls Shutdown(), which writes to the eventfd and unblocks the loop. Verified: this already works in the current code.

### 6. Worker reads node responses with per-connection buffers
After `Worker::onTaskReady` writes a task to a node connection fd, it SHALL submit a `PrepareRead` on the same fd. The Worker tracks these pending reads in a `std::unordered_map<int, PendingNodeRead>` (keyed by node fd). Each `PendingNodeRead` holds a 4096-byte buffer and wire protocol parsing state. When a read completion arrives at `Worker::HandleCompletion`, the Worker parses the 5-byte TLV header (1 byte type + 4 bytes length network order) and the msgpack payload, then calls `ConnectionPool::Handler::onTaskResult` or `onTaskError`. A write completion is distinguished from a read completion by the fd not being in the pending-reads map.

#### task_id mapping
The gateway assigns a local `task_index_` and stores it in `pending_receivers_`. The node agent assigns its own local `task_id_`. Response messages (kChunk, kComplete, kError) carry the node's task_id. For MVP with a single task, the gateway ignores the returned task_id and dispatches to the sole pending receiver. A production implementation would embed the gateway's task_id in the serialized task for the node to echo back.

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Single 4096-byte read buffer may not capture full HTTP request body | Sufficient for echo tasks; production read would use multiple reads or dynamic buffer |
| Blocking connect blocks the main thread during startup | Acceptable for MVP with localhost node agents (sub-ms per connect) |
| Gateway and node agent use independent task_id counters; response messages carry node's task_id, which doesn't match gateway's | MVP fix: single in-flight task → dispatch to sole pending_receivers_ entry. Production: embed gateway task_id in serialized task for echo-back |
