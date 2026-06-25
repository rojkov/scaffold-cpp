# Deferred Design Decisions

Items here are design decisions deferred from active changes. Each entry links to the change that deferred it.

## Async connect via io_uring `IORING_OP_CONNECT`

**Deferred from:** `add-mvp-functionality-for-echo-tasks-submitted-with-curl`

**What:** Replace the synchronous `connect()` in `ConnectionPool::addNode` with an async connect submitted via `io_uring_prep_connect`.

**Why deferred:** `addNode` is called from the main thread via discovery callbacks, but the Worker's io_uring ring lives on a worker thread. Issuing SQEs cross-thread is a data race. Synchronous connect is acceptable for MVP (localhost node agents).

**When to revisit:** When non-local nodes are needed, or when connection retry/timeout logic is required.

**Approach:**
1. Add `PrepareConnect` to the `Dispatcher` interface
2. Implement it in `DispatcherImpl` using `io_uring_prep_connect`
3. Add a thread-safe command mechanism so the main thread can enqueue connect requests for the worker thread to process (e.g., mutex-guard the `command_queue_` in `DispatcherImpl`)
4. Replace the blocking `connect()` in `addNode` with a queued command

## Buffer lifetime for io_uring SQE data pointers

**Deferred from:** `add-mvp-functionality-for-echo-tasks-submitted-with-curl`

**What:** Multiple places create local `std::vector<std::byte>` buffers and pass their data pointers to `PrepareWrite`/`PrepareRead`. The vectors go out of scope before the SQE is submitted (on the next `io_uring_submit_and_wait` call), creating a use-after-free.

**Affected paths:**
- `Worker::onTaskReady` — `header` and `task_body` local vectors
- `NodeAgentConnection::handleTask` — `NodeResultReceiver::sendChunk` local `data` and `header` vectors
- `ClientConnection::onMessageCompleteImpl` — `HttpResultReceiver::sendChunk` uses `owner_->response_buf_` (this one is fixed — stored in a member)

**Why deferred:** The issue exists in the original code pre-MVP and doesn't manifest in practice because:
- SQEs and buffers are typically processed in the same event loop tick
- Memory is rarely overwritten before the kernel copies it during submission
- Echo tasks produce small, fast responses

**When to revisit:** Before any production deployment, or when introducing larger payloads, multiple workers, or slower responses.

**Approach:**
1. Store SQE buffers in per-connection member variables (pattern: `response_buf_` in `ClientConnection`)
2. For `NodeResultReceiver`, store buffers in the receiver itself or in the owner `NodeAgentConnection`
3. For `Worker::onTaskReady`, store buffers in a map keyed by pending task or in the `NodeReadContext`

## Gateway/node task_id mapping mismatch

**Deferred from:** `add-mvp-functionality-for-echo-tasks-submitted-with-curl`

**What:** The gateway assigns task IDs via its own `task_index_` counter. The node agent assigns task IDs via its own `task_id_` counter. Response messages (kChunk, kComplete, kError) carry the node's task_id string. The gateway has no way to map the node's task_id back to its pending receiver.

**Workaround:** For MVP with a single in-flight task, the gateway dispatches responses to the sole pending receiver regardless of the returned task_id.

**Why deferred:** The echo handler processes synchronously, so only one task is ever in flight. Adding proper task_id mapping requires extending the wire protocol.

**When to revisit:** When multiple concurrent tasks per node connection are needed.

**Approach:**
1. Extend `serializeTask` to include the gateway's task_id in the msgpack payload
2. Have the node agent echo the gateway's task_id back in kChunk/kComplete/kError messages
3. Use the echoed task_id to look up `pending_receivers_` on the gateway side
