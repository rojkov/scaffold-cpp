## ADDED Requirements

### Requirement: Main thread lifecycle
The main thread SHALL:
1. Parse the YAML config file
2. Create an initial `NodeDirectory` (possibly empty) and deliver it to each worker
3. Instantiate `NodeDiscovery` extension and register it with the main dispatcher
4. Create N worker threads
5. Enter the main event loop (small io_uring ring)

#### Scenario: Gateway starts with workers
- **WHEN** the gateway executable starts
- **THEN** the main thread SHALL create at least one worker thread
- **AND** each worker SHALL receive a means to receive commands via eventfd

### Requirement: Worker thread structure
Each worker SHALL:
- Own a `DispatcherImpl` with 4096 SQE entries
- Listen on the configured port via SO_REUSEPORT
- Instantiate `TaskSource` and `Scheduler` extensions via the registry
- Maintain a per-worker `NodeDirectory` and `ConnectionPool` for all known nodes
- Process `ADD_NODE`, `REMOVE_NODE`, `UPDATE_NODE` commands during its command queue drain
- Enter its event loop and never exit until shutdown

#### Scenario: Workers listen on same port
- **WHEN** two workers exist
- **THEN** both SHALL be able to accept connections on port 8081 simultaneously via SO_REUSEPORT

### Requirement: Main→worker command forwarding
The main thread SHALL forward node lifecycle events to workers by submitting `ADD_NODE`, `REMOVE_NODE`, or `UPDATE_NODE` commands via each worker's eventfd. Workers SHALL process these commands during their command queue drain before submitting io_uring SQEs.

#### Scenario: Node added triggers command
- **WHEN** a new node is discovered by a NodeDiscovery extension
- **THEN** the main thread SHALL submit an `ADD_NODE` command to every worker's eventfd
- **AND** each worker SHALL drain the command and update its local NodeDirectory and ConnectionPool
