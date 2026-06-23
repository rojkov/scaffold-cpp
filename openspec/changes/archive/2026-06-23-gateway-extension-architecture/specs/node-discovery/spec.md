## ADDED Requirements

### Requirement: NodeDiscovery abstract interface
The system SHALL define an abstract `NodeDiscovery` interface. Extensions SHALL implement `IOObject` and register with the main thread's dispatcher. A `Handler` callback SHALL be provided to the extension for reporting node lifecycle events.

#### Scenario: StaticDiscovery reads YAML
- **WHEN** a `StaticDiscovery` extension starts
- **THEN** it SHALL read the node list from the YAML config file
- **AND** call `onNodeAdded(info)` for each node

### Requirement: Handler callbacks
The `Handler` SHALL provide:
- `onNodeAdded(NodeInfo info)`
- `onNodeRemoved(string nodeId)`
- `onNodeUpdated(NodeInfo info)`

The main thread SHALL forward each event as a command (`ADD_NODE`, `REMOVE_NODE`, `UPDATE_NODE`) to every worker's eventfd. The main thread SHALL NOT process node events itself — it is a dispatcher, not a consumer.

#### Scenario: Node added
- **WHEN** `onNodeAdded` is called with a new echo-capable node
- **THEN** the main thread SHALL submit an `ADD_NODE` command to each worker's eventfd
- **AND** each worker SHALL, on next queue drain, update its NodeDirectory and establish a TCP connection

### Requirement: NodeInfo structure
`NodeInfo` SHALL contain:
- `id` (string): unique node identifier
- `address` (string): ip:port for TCP connection
- `task_types` (vector of string): task handler types this node supports
- `max_capacity` (vector of (string, uint64)): maximum resource capacity

#### Scenario: NodeInfo constructed
- **WHEN** a node is discovered
- **THEN** its `NodeInfo` SHALL contain id, address, task types, and max capacities
