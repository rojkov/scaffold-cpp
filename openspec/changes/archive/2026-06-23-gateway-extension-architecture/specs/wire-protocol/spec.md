## ADDED Requirements

### Requirement: TLV framing
Every message between gateway and node agent SHALL be framed as:
- 1 byte: message type
- 4 bytes: payload length (network byte order, big-endian)
- N bytes: msgpack-encoded payload

#### Scenario: Message is sent
- **WHEN** the gateway sends a SUBMIT_TASK message
- **THEN** the first byte SHALL be 0x01
- **AND** the next 4 bytes SHALL be the big-endian length of the msgpack payload
- **AND** the remaining bytes SHALL be the msgpack-encoded Task

### Requirement: Message types
The following message types SHALL be defined:
- `SUBMIT_TASK` (0x01): gateway → node, serialized Task
- `CHUNK` (0x02): node → gateway, task_id + bytes + is_final
- `COMPLETE` (0x03): node → gateway, task_id — success, no more chunks
- `ERROR` (0x04): node → gateway, task_id + error message
- `REGISTER` (0x05): node → gateway, node info + capabilities (deferred)
- `HEARTBEAT` (0x06): bidirectional keepalive (deferred)

#### Scenario: Chunk delivery
- **WHEN** a task handler produces a chunk on the node agent
- **THEN** the node SHALL send a CHUNK (0x02) message with the task_id, chunk bytes, and is_final flag

### Requirement: Serialization in core
The wire protocol (TLV framing + msgpack schema) SHALL be implemented in core, shared by both gateway and node agent. It SHALL NOT be an extension.

#### Scenario: Shared serialization code
- **WHEN** the gateway serializes a Task for SUBMIT_TASK
- **AND** the node agent deserializes it
- **THEN** both SHALL use the same serialization functions from core
