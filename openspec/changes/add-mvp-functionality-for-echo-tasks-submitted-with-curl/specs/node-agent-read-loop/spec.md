## ADDED Requirements

### Requirement: NodeAgentConnection allocates read buffers

`NodeAgentConnection` SHALL allocate a 4096-byte read buffer and pass it to `PrepareRead`. After each read completion, the buffer contents are parsed and a new read is submitted.

#### Scenario: startRead allocates buffer and submits read

- **WHEN** `startRead` is called
- **THEN** a 4096-byte buffer is allocated and `PrepareRead` is called on the dispatcher with that buffer

#### Scenario: Read completion parses message and re-arms

- **WHEN** a read completes with data
- **THEN** the wire protocol header is decoded, the payload is despatched to the appropriate handler, the buffer is cleared, and a new `PrepareRead` is submitted
