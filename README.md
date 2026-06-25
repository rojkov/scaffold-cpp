# Scaffold for C++ projects using Bazel as the main build system.

Given you've got a fresh VM or a new Linux PC with Docker installed
for development just type in:

```bash
$ make env
```

After a moment or two you'll get into a brand new build environment
inside a Docker container.

Then check the initial fake targets build successfully:

```bash
$ make build
```

and the tests pass

```bash
$ make test
```

Now feel free to replace the fake bazel build targets with your own
real ones and build `compile_commands.json` to help your IDE do
auto-complete:

```bash
$ make compiledb
```

## Quick Start

Run a full end-to-end MVP test: curl → gateway → node agent → echo handler → response.

**1. Build the binaries**

```bash
bazel build //src/exe/gateway:gateway //src/exe/nodeagent:nodeagent
```

**2. Start the node agent** (terminal 1)

```bash
./bazel-bin/src/exe/nodeagent/nodeagent
```

Config is loaded from `./examples/configs/nodeagent.yaml` (listens on port 9090).

**3. Start the gateway** (terminal 2)

```bash
./bazel-bin/src/exe/gateway/gateway
```

Config is loaded from `./examples/configs/gateway.yaml` (HTTP on port 8081, static nodes).

**4. Submit an echo task with curl** (terminal 3)

```bash
curl -X POST http://127.0.0.1:8081 -d "hello carrot"
```

Expected response:

```
hello carrot
```

## Toolchain selection

By default the host compiler is used which is *gcc* usually. If you want to build
with the custom local *clang* toolchain add the following option to your build command:
`--extra_toolchains=//toolchain:cc_toolchain_for_linux_x86_64` or edit `.bazelrc`.

But first you need to get the toolchain. Two options:

  1. unpack a binary distribution of clang and llvm tools to `./toolchain/clang`;
  2. clone the sources of the LLVM project somewhere and enter:

```bash
$ make llvm-toolchain LLVM_PROJECT_PATH=/path/to/repo/with/llvm-project
```

By default `LLVM_PROJECT_PATH` is set to `/path/to/this/repo/../llvm-project`.

With the clang toolchain available you can run tests with address and thread
sanitizers enabled:

```bash
$ make test_asan
$ make test_tsan
```

As well as `clang-tidy` checks:

```bash
$ make clang-tidy
```

## Code coverage

Run tests instrumentalized for collecting coverage statistics:

```bash
$ make coverage
```

The code coverage report should be in `./coverage` now.
