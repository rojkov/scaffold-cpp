## ADDED Requirements

### Requirement: Mutable per-worker NodeDirectory
Each worker SHALL own a mutable `NodeDirectory` that is updated only during the worker's own command queue drain. No synchronization SHALL be needed between the main thread and workers — changes arrive as commands via the worker's eventfd and are applied sequentially.

#### Scenario: Worker applies ADD_NODE command
- **WHEN** a worker drains its command queue and finds an `ADD_NODE` command
- **THEN** it SHALL insert the node into the directory
- **AND** trigger a TCP connection to the node's address

### Requirement: Pre-indexed lookups
`NodeDirectory` SHALL provide:
- `byType(string_view type) → span<NodeInfo*>`: O(1) lookup of all nodes supporting a task type
- `byId(string_view id) → NodeInfo*`: O(1) lookup of a single node by ID

#### Scenario: Lookup by task type
- **WHEN** `byType("echo")` is called
- **THEN** it SHALL return references to all nodes whose `task_types` include "echo"

### Requirement: Mutation commands
`NodeDirectory` SHALL provide mutation methods that are called only from the worker thread:
- `addNode(NodeInfo info)`
- `removeNode(string_view id)`
- `updateNode(NodeInfo info)`

#### Scenario: Node removed
- **WHEN** `removeNode("node-1")` is called
- **THEN** `byId("node-1")` SHALL return nullptr
- **AND** `byType` lookups SHALL no longer include the removed node

### Requirement: Dynamic resource tracking
Available (dynamic) resource capacity SHALL NOT be stored in `NodeDirectory`. It SHALL be tracked per-worker in `ConnectionPool`. The `Scheduler` SHALL combine directory lookups with connection pool state to make final node selections.

#### Scenario: Capacity filtered by connection pool
- **WHEN** Scheduler calls `byType("echo")` returning three nodes
- **AND** only two have available capacity according to ConnectionPool
- **THEN** the Scheduler SHALL only consider those two for selection
