## ADDED Requirements

### Requirement: Per-worker connection pool
Each worker SHALL maintain a `ConnectionPool` that holds active TCP connections to all known nodes. Connections SHALL be initiated when the worker processes an `ADD_NODE` command during its command queue drain.

#### Scenario: Worker connects on ADD_NODE command
- **WHEN** a worker drains an `ADD_NODE` command from its queue
- **THEN** the worker SHALL establish a TCP connection to that node's address
- **AND** add it to the connection pool

#### Scenario: Worker disconnects on REMOVE_NODE command
- **WHEN** a worker drains a `REMOVE_NODE` command from its queue
- **THEN** the worker SHALL close the TCP connection to that node
- **AND** fail any in-flight tasks assigned to that node

### Requirement: Connection lifecycle tracking
The `ConnectionPool` SHALL track for each node:
- Connection state (alive / dead / connecting)
- Available resource capacity (current load, decremented on task dispatch, incremented on completion)
- Whether the node can accept more tasks

It SHALL expose `isAlive(nodeId) → bool` and `hasCapacity(nodeId) → bool` for the Scheduler.

#### Scenario: Pool reports node state
- **WHEN** a node connection is healthy
- **THEN** `isAlive(nodeId)` SHALL return true

#### Scenario: Pool tracks capacity
- **WHEN** a task is sent to a node
- **THEN** the pool SHALL decrement the node's available capacity
- **AND** when COMPLETE or ERROR is received, SHALL increment it back

### Requirement: Connection reconnection
If a node connection drops, the `ConnectionPool` SHALL mark the node as dead and attempt to reconnect. In-flight tasks on a dropped connection SHALL be failed with an error.

#### Scenario: Node goes down
- **WHEN** a TCP connection to a node is lost
- **THEN** the pool SHALL mark the node as dead
- **AND** `isAlive(nodeId)` SHALL return false
