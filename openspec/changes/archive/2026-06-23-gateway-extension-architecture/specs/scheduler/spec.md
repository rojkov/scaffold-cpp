## ADDED Requirements

### Requirement: Scheduler abstract interface
The system SHALL define an abstract `Scheduler` interface with a single method `selectNode(task, eligibleNodes) → nodeId`. The scheduler SHALL receive a `Task` and a pre-filtered list of eligible `NodeInfo*` (matching task type, alive, with capacity). It SHALL return the chosen node's ID.

#### Scenario: Scheduler selects from eligible nodes
- **WHEN** `selectNode` is called with a task and three eligible nodes
- **THEN** it SHALL return the ID of one of the three nodes

#### Scenario: No eligible nodes
- **WHEN** `selectNode` is called with an empty eligible list
- **THEN** it SHALL return an empty string or throw, indicating no node is available

### Requirement: RoundRobin scheduler
A `RoundRobin` scheduler extension SHALL be provided by default. It SHALL maintain a per-task-type cursor and rotate through eligible nodes.

#### Scenario: RoundRobin rotates
- **WHEN** two echo tasks arrive consecutively with the same two eligible nodes A and B
- **THEN** the first task SHALL be assigned to A
- **AND** the second task SHALL be assigned to B
