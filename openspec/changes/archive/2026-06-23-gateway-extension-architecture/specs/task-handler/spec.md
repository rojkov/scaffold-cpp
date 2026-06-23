## ADDED Requirements

### Requirement: TaskHandler abstract interface
The system SHALL define an abstract `TaskHandler` interface with `handleTask(const Task&, ResultReceiver&)`. The handler SHALL read the task body, execute the task, and call `receiver.sendChunk(chunk, isFinal)` zero or more times.

#### Scenario: Handler produces single chunk
- **WHEN** an echo handler processes an echo task
- **THEN** it SHALL call `receiver.sendChunk(task.body, true)` exactly once

#### Scenario: Handler produces multiple chunks
- **WHEN** a streaming task handler processes a task
- **THEN** it MAY call `receiver.sendChunk(chunk, false)` multiple times
- **AND** call `receiver.sendChunk(chunk, true)` as the final call

### Requirement: Echo handler
An `Echo` task handler SHALL be provided as a default extension. It SHALL read the entire `task.body` and return it as a single chunk.

#### Scenario: Echo task completes
- **WHEN** the echo handler is invoked with a task containing "hello"
- **THEN** it SHALL send one chunk with data "hello" and is_final=true

### Requirement: Handler registration
Task handlers SHALL register via the `CARROT_REGISTER_EXTENSION(task_handler, name, HandlerFactory)` macro, just like gateway extensions.

#### Scenario: Handler auto-registers
- **WHEN** an echo task handler library is linked into the node agent
- **THEN** it SHALL be registered in the `task_handler` category before main()
