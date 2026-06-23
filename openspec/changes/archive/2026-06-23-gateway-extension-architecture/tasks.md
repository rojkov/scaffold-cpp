## 1. Extension Registry Core

- [x] 1.1 Add `ExtensionRegistry` class with type-safe per-category maps and singleton access
- [x] 1.2 Add `FactoryContext` class with `Dispatcher&` accessor
- [x] 1.3 Add `CARROT_REGISTER_EXTENSION` macro and self-registration machinery
- [x] 1.4 Add `carrot_extension` Bazel macro with `alwayslink = True` in `build_system.bzl`
- [x] 1.5 Create `extensions_gateway.bzl` and `extensions_nodeagent.bzl` flat extension lists
- [x] 1.6 Add msgpack and yaml-cpp dependencies to `MODULE.bazel`

## 2. Core Data Types

- [x] 2.1 Define `Task` struct (type, body, function?, result_endpoint?, requirements)
- [x] 2.2 Define `Chunk` type alias (bytes)
- [x] 2.3 Define `NodeInfo` struct (id, address, task_types, max_capacity)
- [x] 2.4 Define `Requirement` struct (type string, amount uint64)
- [x] 2.5 Define `ResultReceiver` abstract interface with `sendChunk(chunk, isFinal)`

## 3. NodeDirectory

- [x] 3.1 Implement `NodeDirectory` mutable class with byType and byId pre-indexed lookups
- [x] 3.2 Implement mutation methods (addNode, removeNode, updateNode)
- [x] 3.3 Define command types: ADD_NODE, REMOVE_NODE, UPDATE_NODE

## 4. Wire Protocol

- [x] 4.1 Implement TLV framer (1-byte type, 4-byte big-endian length, msgpack payload)
- [x] 4.2 Define message type enum (SUBMIT_TASK, CHUNK, COMPLETE, ERROR, REGISTER, HEARTBEAT)
- [x] 4.3 Implement Task↔msgpack serialization
- [x] 4.4 Implement Chunk↔msgpack serialization
- [x] 4.5 Implement message reader (reads TLV header, deserializes payload)

## 5. Interchange Core Types

- [x] 5.1 Define TaskSource interface (TaskSource, setHandler, IOObject inheritance)
- [x] 5.2 Define Scheduler interface (selectNode(task, eligibleNodes) → nodeId)
- [x] 5.3 Define NodeDiscovery::Handler interface (onNodeAdded, onNodeRemoved, onNodeUpdated)
- [x] 5.4 Define TaskHandler interface (handleTask(task, ResultReceiver&))

## 6. Extension Implementations (Gateway)

- [x] 6.1 Implement HTTP TaskSource (listener on configured port, HTTP request parsing via llhttp, ResultReceiver that writes HTTP response)
- [x] 6.2 Implement RoundRobin Scheduler
- [x] 6.3 Implement Static NodeDiscovery (reads nodes from YAML config)
- [x] 6.4 Register each extension with `CARROT_REGISTER_EXTENSION` macro

## 7. Extension Implementations (Node Agent)

- [x] 7.1 Implement Echo TaskHandler (reads body, returns as single chunk)
- [ ] 7.2 Implement ResultReceiver on node side that sends CHUNK/COMPLETE messages over gateway connection
- [x] 7.3 Register each handler with `CARROT_REGISTER_EXTENSION` macro

## 8. ConnectionPool

- [x] 8.1 Implement per-worker ConnectionPool that establishes TCP connections on ADD_NODE command
- [x] 8.2 Implement liveness tracking (isAlive, dead detection)
- [x] 8.3 Implement capacity tracking (available resources, decrement on send, increment on completion)
- [x] 8.4 Implement reconnection logic on connection drop

## 9. Gateway Worker Model

- [x] 9.1 Parameterize `DispatcherImpl` constructor with ring size (small for main, large for workers)
- [x] 9.2 Implement main thread lifecycle (parse config, create workers, enter event loop)
- [x] 9.3 Implement worker thread creation with per-worker extensions, dispatcher, and connection pool
- [x] 9.4 Implement main→worker command forwarding (eventfd + ADD_NODE/REMOVE_NODE/UPDATE_NODE commands)
- [x] 9.5 Set up SO_REUSEPORT listener distribution across workers
- [x] 9.6 Wire the core task routing pipeline: TaskSource → parse → Scheduler → ConnectionPool → wire → ResultReceiver

## 10. Node Agent Executable

- [x] 10.1 Create `src/exe/nodeagent/` with `nodeagent.cc` entrypoint
- [x] 10.2 Implement gateway connection handler (accept, wire protocol reader)
- [x] 10.3 Implement task dispatch: deserialize SUBMIT_TASK → lookup TaskHandler → invoke → stream CHUNK back
- [x] 10.4 Add `extensions_nodeagent.bzl` with Echo handler included by default
- [x] 10.5 Create `BUILD.bazel` for nodeagent linking selected extensions

## 11. Config and Wiring

- [x] 11.1 Implement YAML config parsing for gateway (workers count, extensions config, node discovery)
- [x] 11.2 Implement YAML config parsing for node agent (listen port, task handlers)
- [x] 11.3 Wire YAML config → extension registry lookup → extension instantiation with FactoryContext

## 12. Cleanup and Testing

- [ ] 12.1 Remove old hardcoded TcpListener/Connection/LlhttpParser from gateway entrypoint
- [x] 12.2 Replace `printf` debug calls in DispatcherImpl with LOG_* macros
- [ ] 12.3 Add unit tests for ExtensionRegistry (registration, lookup, factory invocation)
- [ ] 12.4 Add unit tests for TLV framer and msgpack serialization
- [ ] 12.5 Add unit tests for NodeDirectory (byType, byId, addNode, removeNode)
- [ ] 12.6 Add unit tests for RoundRobin Scheduler
- [ ] 12.7 Add unit tests for ConnectionPool (liveness, capacity tracking)
- [x] 12.8 Add integration test: gateway + node agent = echo task roundtrip
