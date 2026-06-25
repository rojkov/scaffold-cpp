## 1. Build system — add llhttp dependency

- [x] 1.1 Add `http_archive` for `llhttp` in `MODULE.bazel` (source: `https://github.com/nodejs/llhttp`)
- [x] 1.2 Add `configure_make` (or `cmake`) build rule for `llhttp` in root `BUILD.bazel`
- [x] 1.3 Add `//:llhttp` dep to `src/extensions/gateway/task_sources/http/BUILD.bazel`

## 2. HttpTaskSource — read buffer allocation and llhttp parsing

- [x] 2.1 Add `std::unordered_map<int, std::vector<std::byte>>` read buffers member to `HttpTaskSource`
- [x] 2.2 In `onAccept`, allocate a 4096-byte buffer and call `PrepareRead` with it (instead of empty span)
- [x] 2.3 In `HandleCompletion`, distinguish accept (new fd) vs read (byte count) completions; route accept to `onAccept` and read to llhttp parser
- [x] 2.4 Feed completed read data into an `llhttp` parser instance per connection; invoke `onRequest` with the body bytes when `on_message_complete` fires
- [x] 2.5 In `ProcessCommand(CLOSE_CONNECTION)`, clean up the associated read buffer

## 3. ConnectionPool — synchronous connect for MVP

- [x] 3.1 In `addNode`, after creating socket, parse address from `info.address`, call blocking `connect()`, then set `O_NONBLOCK`
- [x] 3.2 Set `conn.alive = true` only after connect succeeds
- [x] 3.3 In `Worker::onTaskReady`, check `conn->alive` before writing; if not alive, drop task with error

## 4. Worker — read node responses and dispatch to receivers

- [x] 4.1 Add `PendingNodeRead` struct with `std::vector<std::byte> buffer` and wire parse state
- [x] 4.2 Add `std::unordered_map<int, PendingNodeRead>` member to `Worker` (keyed by node fd)
- [x] 4.3 In `onTaskReady`, after writing task to `conn->fd`, submit `PrepareRead` on the same fd with a 4096-byte buffer
- [x] 4.4 In `Worker::HandleCompletion`, distinguish write completions (no pending read for this fd) from read completions (fd found in the map)
- [x] 4.5 Parse the 5-byte TLV header (MessageType + 32-bit length) from the read buffer
- [x] 4.6 Parse the msgpack payload and call `handler_.onTaskResult` (for kChunk/kComplete) or `handler_.onTaskError` (for kError)
- [x] 4.7 After processing a read, re-arm `PrepareRead` on the node fd for the next response

## 5. NodeAgentConnection — read buffer allocation

- [x] 5.1 In `startRead`, allocate a 4096-byte buffer and call `PrepareRead` with it
- [x] 5.2 In `HandleCompletion`, after processing message, clear buffer and re-arm `PrepareRead` with a new buffer

## 6. Static discovery — fix YAML node iteration

- [x] 6.1 In `Static::Static`, access `(*nodes)["nodes"]` to get the list instead of iterating the top-level node

## 7. Gateway config — fix example config

- [x] 7.1 Verify `examples/configs/gateway.yaml` structure matches what the parser expects (task_source, scheduler, node_discovery with nested nodes)

## 8. Quick Start docs — add e2e usage instructions to README

- [x] 8.1 Add a "Quick Start" section to `README.md` with numbered steps: build, start node agent, start gateway, curl POST, expected echo response
