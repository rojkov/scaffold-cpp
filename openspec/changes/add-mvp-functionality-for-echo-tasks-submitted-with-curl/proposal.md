## Why

The gateway extension architecture is implemented but the data pipeline has gaps that prevent a basic end-to-end test: curl → gateway → node agent → echo handler → response. Closing these gaps validates the architecture with a real network roundtrip and unblocks further development.

## What Changes

- Wire `HandleCompletion` in `HttpTaskSource` to dispatch read completions separately from accept completions, allocate per-connection read buffers, and call `onRequest` when HTTP data arrives
- Implement TCP `connect()` in `ConnectionPool::addNode` to establish real connections to node agents
- Fix `Static` node discovery to parse the `nodes` list from YAML config instead of iterating over the top-level map
- Allocate read buffers and handle reads properly in `NodeAgentConnection`
- Add proper HTTP response headers in `HttpResultReceiver` for curl compatibility (`Content-Type`, `Connection: close`)
- Wire `SignalMonitor` to `DispatcherImpl` for graceful shutdown on SIGINT/SIGTERM
- Update the gateway sample config to match the actual config structure

## Capabilities

### New Capabilities
- `http-task-source-read`: HTTP request reading and parsing pipeline in HttpTaskSource
- `connection-pool-connect`: TCP connection establishment to node agents
- `node-agent-read-loop`: Read buffer management and message dispatch in NodeAgentConnection
- `quick-start-docs`: Documentation for end-to-end MVP usage

### Modified Capabilities
- *(none — all changes are new capability implementations within existing interfaces)*

## Impact

- `src/extensions/gateway/task_sources/http/http_task_source.cc` — read completion dispatch, buffer management, HTTP response formatting
- `src/core/common/connection_pool.cc` — `connect()` call in `addNode`
- `src/extensions/gateway/discovery/static/static_discovery.cc` — YAML nodes list parsing
- `src/exe/nodeagent/nodeagent.cc` — read buffer allocation, read dispatch
- `src/exe/gateway/gateway.cc` — wire up SignalMonitor for shutdown
- `examples/configs/gateway.yaml` — fix config structure to match parser expectations
- `README.md` — add Quick Start section with CLI commands for e2e test
