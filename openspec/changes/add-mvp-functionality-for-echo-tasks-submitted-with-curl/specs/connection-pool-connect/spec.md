## ADDED Requirements

### Requirement: ConnectionPool connects to node agents asynchronously

`ConnectionPool::addNode` SHALL create a TCP socket and issue an async connect via the Dispatcher's `PrepareConnect`. The connection SHALL use the node's address from `NodeInfo::address` (format `host:port`).

#### Scenario: Node added with valid address connects successfully

- **WHEN** `addNode` is called with a `NodeInfo` containing `address: "127.0.0.1:9090"`
- **THEN** a non-blocking socket is created and an async connect is submitted to the dispatcher

### Requirement: Dispatcher supports PrepareConnect

The `Dispatcher` interface SHALL have a `PrepareConnect` method. `DispatcherImpl` SHALL use `io_uring_prep_connect` to submit the connect SQE.

#### Scenario: Connect SQE is submitted

- **WHEN** `PrepareConnect` is called with an IOObject, fd, and sockaddr
- **THEN** an `IORING_OP_CONNECT` SQE is prepared and linked to the given IOObject

### Requirement: Connected flag prevents writes before connect completes

`NodeConnection` SHALL have a `connected` bool (default `false`). `Worker::onTaskReady` SHALL check `conn->connected` before writing to the fd. If not connected, the task SHALL be queued or dropped.

#### Scenario: Write deferred until connect completes

- **WHEN** `onTaskReady` finds the connection not yet established
- **THEN** the result receiver receives an error (empty chunk with is_final=true)
