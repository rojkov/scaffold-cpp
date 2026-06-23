## ADDED Requirements

### Requirement: ResultReceiver abstract interface
The system SHALL define a `ResultReceiver` interface with `sendChunk(Chunk chunk, bool isFinal)`. `Chunk` SHALL be an alias for `std::vector<std::byte>` (or similar span of bytes).

#### Scenario: sendChunk delivers data
- **WHEN** `sendChunk` is called with a chunk
- **THEN** the implementation SHALL deliver the chunk bytes to the user (e.g., write to HTTP response)

### Requirement: ResultReceiver owned by TaskSource
A `ResultReceiver` implementation SHALL be created by the `TaskSource` that accepted the user's request. The core SHALL receive the receiver when the task is submitted and call it as chunks arrive from the node.

#### Scenario: TaskSource creates receiver
- **WHEN** an HTTP TaskSource parses a complete HTTP request
- **THEN** it SHALL create a ResultReceiver that writes to that HTTP connection's response
- **AND** pass the receiver alongside the Task to the core

### Requirement: Connection lifecycle
The `TaskSource` implementation SHALL own the user connection fd and SHALL close it when `sendChunk` with `isFinal = true` is called, or on error.

#### Scenario: Connection closed on final chunk
- **WHEN** the core calls `sendChunk(data, true)` on the receiver
- **THEN** the HTTP ResultReceiver SHALL write the HTTP response (with appropriate Content-Length or chunked encoding)
- **AND** close the user connection

### Requirement: User disconnect handling
If the `TaskSource` detects that the user has disconnected before the task completes, it SHALL notify the core to cancel the in-flight task.

#### Scenario: User disconnects early
- **WHEN** the user TCP connection is closed before the final chunk arrives
- **THEN** the HTTP TaskSource SHALL detect the closed fd
- **AND** signal the core to abandon/cancel the task
