## ADDED Requirements

### Requirement: Node agent executable
A `nodeagent` executable SHALL exist. It SHALL:
- Own a `DispatcherImpl` (4096 entries) for its main thread
- Own a logging thread (shared core logging system)
- Listen for TCP connections from the gateway
- Use its own extension registry to load `TaskHandler` extensions
- Use the same wire protocol as the gateway for communication

#### Scenario: Node agent starts
- **WHEN** the node agent starts
- **THEN** it SHALL listen on a configured port for gateway connections

### Requirement: Gateway-initiated connections
The node agent SHALL accept TCP connections initiated by the gateway (active mode only). Passive node discovery (node connects to gateway) SHALL NOT be required initially.

#### Scenario: Gateway connects to node
- **WHEN** a gateway worker connects to the node agent's port
- **THEN** the node agent SHALL accept the connection and be ready to receive SUBMIT_TASK messages

### Requirement: Task dispatch on node
When the node agent receives a `SUBMIT_TASK` message, it SHALL deserialize the `Task`, look up the handler for `task.type` in its extension registry, and call the handler's `handleTask(task, resultReceiver)` method. The `ResultReceiver` SHALL send `CHUNK` and `COMPLETE`/`ERROR` messages back to the gateway.

#### Scenario: Echo handler responds
- **WHEN** the node agent receives a SUBMIT_TASK with type "echo"
- **THEN** it SHALL find the Echo handler
- **AND** call `handleTask(task, receiver)`
- **AND** the handler SHALL send a CHUNK with the task body and is_final=true
