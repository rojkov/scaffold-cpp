## ADDED Requirements

### Requirement: TaskSource abstract interface
The system SHALL define an abstract `TaskSource` interface that accepts incoming user connections, parses requests into `Task` structs, and creates a `ResultReceiver` bound to the user connection.

#### Scenario: TaskSource produces Task
- **WHEN** a complete request arrives on a user connection managed by a TaskSource
- **THEN** the TaskSource SHALL parse it into a `Task` struct
- **AND** create a `ResultReceiver` that can write the result back to the same user connection

### Requirement: Task structure
`Task` SHALL contain the following fields:
- `type` (string): identifies the task handler type that can execute this task
- `body` (bytes): opaque blob interpreted by the task handler
- `function` (optional string): handler-specific metadata (e.g. WASM module ID)
- `result_endpoint` (optional string): if non-empty, task is asynchronous and the result SHALL be delivered to this endpoint
- `requirements` (list of (string, uint64) tuples): resource requirements for scheduling

#### Scenario: Echo task
- **WHEN** a user submits an HTTP request with `type: echo`
- **THEN** the HTTP TaskSource SHALL produce a Task with `type = "echo"` and `body` containing the request body

### Requirement: TaskSource registers with dispatcher
The TaskSource SHALL be an `IOObject` registered with the worker's dispatcher. It SHALL manage its own listener socket (via SO_REUSEPORT) and accept user connections.

#### Scenario: HTTP TaskSource listens on configured port
- **WHEN** an HTTP TaskSource extension is instantiated with port 8081 in config
- **THEN** it SHALL create a TCP listener on port 8081 with SO_REUSEPORT
- **AND** register a multishot accept SQE with the worker's dispatcher
