## ADDED Requirements

### Requirement: Extension registry singleton
The system SHALL provide a global `ExtensionRegistry` singleton accessible via `ExtensionRegistry::getInstance()`. The registry SHALL maintain a type-safe map per category, keyed by extension name.

#### Scenario: Registry is accessible
- **WHEN** any code calls `ExtensionRegistry::getInstance()`
- **THEN** the same singleton instance SHALL be returned every time

#### Scenario: Categories are isolated by type
- **WHEN** two extensions with the same name are registered in different categories
- **THEN** they SHALL NOT conflict with each other

### Requirement: Static registration macro
The system SHALL provide a `CARROT_REGISTER_EXTENSION(category, name, FactoryClass)` macro. The macro SHALL expand to a file-scope static object whose constructor registers the factory in the registry. Extension libraries SHALL use `alwayslink = True` to prevent linker stripping.

#### Scenario: Extension auto-registers
- **WHEN** an extension target is linked into the binary
- **THEN** its factory SHALL be registered in the appropriate category map before `main()` executes

#### Scenario: Factory creates instances
- **WHEN** the registry is queried with the extension's category and name
- **THEN** it SHALL return a `std::unique_ptr` to the extension interface, constructed via the registered factory

### Requirement: Factory signature
Every extension factory SHALL accept `FactoryContext&` and `const Config&` (the YAML config node for that extension). The factory SHALL return `std::unique_ptr<ExtensionType>`.

#### Scenario: Factory is invoked with context
- **WHEN** the registry instantiates an extension
- **THEN** the factory SHALL receive the thread-local FactoryContext and the YAML config sub-tree for that extension

### Requirement: FactoryContext
`FactoryContext` SHALL provide access to the thread's `Dispatcher` instance. Future fields (stats, RNG, API) SHALL be added without breaking existing factories.

#### Scenario: Extension receives dispatcher
- **WHEN** an extension is constructed via its factory
- **THEN** it SHALL be able to access the thread's Dispatcher through the FactoryContext

### Requirement: Per-executable extension list
Each executable SHALL have a ..bzl file (e.g. `extensions_gateway.bzl`) containing a flat list of Bazel labels for its extensions. Commenting out a line SHALL remove that extension from the binary.

#### Scenario: Extension excluded by commenting
- **WHEN** a line is commented out in `extensions_gateway.bzl`
- **THEN** the corresponding extension SHALL NOT be linked into the gateway binary
- **AND** its registration SHALL NOT fire
