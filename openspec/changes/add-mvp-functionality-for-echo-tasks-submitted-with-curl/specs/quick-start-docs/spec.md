## ADDED Requirements

### Requirement: README includes a "Quick Start" section

The project's README.md SHALL contain a "Quick Start" section at the top level. The section SHALL document the CLI commands needed to run an end-to-end MVP test using curl, the gateway config, and the node agent config from `./examples/configs/`.

#### Scenario: Quick Start describes how to start the node agent

- **WHEN** the Quick Start section is read
- **THEN** it SHALL include the command `./bazel-bin/src/exe/nodeagent/nodeagent` to start the node agent, with a note that the config is loaded from `./examples/configs/nodeagent.yaml`

#### Scenario: Quick Start describes how to start the gateway

- **WHEN** the Quick Start section is read
- **THEN** it SHALL include the command `./bazel-bin/src/exe/gateway/gateway` to start the gateway, with a note that the config is loaded from `./examples/configs/gateway.yaml`

#### Scenario: Quick Start describes how to submit an echo task with curl

- **WHEN** the Quick Start section is read
- **THEN** it SHALL include a curl command that submits a POST request to the gateway (default `http://127.0.0.1:8081`) with a body, and SHALL show the expected echo response

#### Scenario: Quick Start describes how to build the binaries

- **WHEN** the Quick Start section is read
- **THEN** it SHALL include the command `bazel build //src/exe/gateway:gateway //src/exe/nodeagent:nodeagent` to build both binaries

#### Scenario: Quick Start lists commands in ordered steps

- **WHEN** the Quick Start section is read
- **THEN** it SHALL present the commands as a numbered list of ordered steps: build, start node agent(s), start gateway, submit task with curl
