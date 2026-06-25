## ADDED Requirements

### Requirement: HttpTaskSource reads HTTP requests from accepted connections

The HttpTaskSource SHALL allocate a 4096-byte read buffer per accepted client connection and submit an async read via PrepareRead. When the read completes, the source SHALL call `onRequest` with the received data.

#### Scenario: Accepted connection is read and dispatched to onRequest

- **WHEN** `HandleCompletion` receives a read completion (byte count, not a new fd)
- **THEN** the read buffer contents are passed to `onRequest` as a string

#### Scenario: Accept completion creates a new read

- **WHEN** `HandleCompletion` receives an accept completion (a new fd)
- **THEN** a 4096-byte buffer is allocated and `PrepareRead` is called with that buffer on the new fd

### Requirement: HttpTaskSource correctly dispatches accept vs read completions

The HttpTaskSource SHALL distinguish between accept CQEs (yielding a new client fd) and read CQEs (yielding bytes read) in its `HandleCompletion`.

#### Scenario: Accept completion does not trigger onRequest

- **WHEN** a new client connection is accepted
- **THEN** `onRequest` is NOT called; only the read buffer is set up

#### Scenario: Read completion does not trigger onAccept

- **WHEN** data is read from a client connection
- **THEN** `onAccept` is NOT called; `onRequest` is called with the data

### Requirement: HTTP requests parsed with llhttp

HttpTaskSource SHALL use the `llhttp` C library to parse incoming HTTP/1.1 request data. Raw bytes read from accepted connections SHALL be fed into an llhttp parser instance. When a complete HTTP message body is received, the source SHALL invoke `onRequest` with the body bytes. The parser SHALL support body data split across multiple reads.

#### Scenario: Single read contains complete HTTP request

- **WHEN** a single read returns the full HTTP request including headers and body
- **THEN** llhttp parses the complete message and `onRequest` is called with the body bytes

#### Scenario: HTTP body arrives across multiple reads

- **WHEN** the HTTP body is larger than the read buffer or arrives in separate TCP segments
- **THEN** data from each read is appended and fed to llhttp incrementally until the message is complete, then `onRequest` is called with the full body

### Requirement: llhttp build dependency added

The build system SHALL fetch and compile `llhttp` as an external dependency. `llhttp` header SHALL be available to the http_task_source build target via Bazel.

#### Scenario: llhttp fetched as http_archive in MODULE.bazel

- **WHEN** the project is built
- **THEN** `llhttp` source is fetched from GitHub and compiled as a static library

#### Scenario: http_task_source BUILD depends on llhttp

- **WHEN** `http` extension target is compiled
- **THEN** the `llhttp` library is linked and headers are available for `#include "llhttp.h"`
