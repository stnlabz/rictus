# Installation

## Status

`Rictus` is currently in active development.

The current development and operational platform is Windows. Installation remains development-oriented while the core, module system, Intelligence module, and command-and-control interfaces continue to evolve.

## Platform

Rictus currently targets:

- Windows
- x64
- Visual Studio 2026
- ISO C
- Windows Schannel for TLS

## Requirements

Current development requirements include:

- Windows x64
- Visual Studio 2026 with C development support
- Git
- Network access to the configured IRC service
- Access to the configured Rictus runtime directories

Additional STN-LABZ applications are required only when their associated Rictus command interfaces are implemented and used.

These include:

- `chain`
- `rag_builder`
- Digit

## Repository

Clone the Rictus repository and open the solution in Visual Studio.

The repository contains the Rictus core and module source.

The general source layout is:

```text
rictus/
├── src/
├── include/
├── modules/
│   └── intelligence/
│       ├── src/
│       ├── include/
│       └── intelligence.vcxproj
├── docs/
├── rictus.vcxproj
└── rictus.slnx
```

The exact project files present may change during active development.

## Build

Rictus uses a DLL hot-load module architecture.

A complete development build therefore includes both:

1. The Rictus core executable.
2. The required module DLLs.

Building only the Rictus core does not produce a complete module-enabled runtime.

### Rictus Core

Open the Rictus solution in Visual Studio and select the required configuration.

Current development commonly uses:

```text
Configuration: Debug
Platform: x64
```

Build the Rictus core project.

A successful build produces the Rictus executable and its associated development artifacts.

### Intelligence Module

Build the Intelligence module project separately or as part of the configured solution build.

The Intelligence module must produce its DLL successfully before it can be discovered and loaded by Rictus.

The compiled DLL is a build artifact and is not committed to the source repository.

## Module Deployment

Rictus discovers modules from its configured module directory.

After building the Intelligence module, deploy the resulting DLL to the module directory configured for the Rictus runtime.

The repository source location and runtime module location are separate concerns.

For example:

```text
Repository:
rictus\modules\intelligence\

Runtime:
<configured Rictus module directory>\intelligence.dll
```

The configured runtime location is authoritative for module discovery.

Presence of a DLL in the module directory does not automatically authorize activation.

Discovered modules remain subject to Rictus core-controlled:

- Discovery
- Identity validation
- API compatibility validation
- Qualification state
- Lifecycle management
- Activation control

## Configuration

Rictus requires a valid runtime configuration before normal communications can begin.

Configuration includes the deployment-specific values required for:

- IRC server
- IRC port
- IRC identity
- IRC channel
- SASL
- Module discovery
- Logging

See:

```text
docs/CONFIGURATION.md
```

for current configuration architecture and runtime-path information.

## IRC Runtime

The current communications path is:

```text
Rictus
   |
   v
TCP
   |
   v
Schannel TLS
   |
   v
IRC
   |
   +-- IRCv3 capability negotiation
   +-- SASL authentication
   +-- channel join
   +-- command interface
```

After configuration and module deployment, start the Rictus executable.

Successful startup should establish the configured IRC connection and process the module discovery and lifecycle sequence.

## Intelligence Runtime

When the Intelligence module is active, it maintains operational intelligence state outside the source repository.

Security Research Target candidate reports are written under:

```text
C:\stn-labz\reports\SRT
```

These reports and associated runtime state are operational artifacts and are not source files.

## RAG Runtime

The approved RAG input location is:

```text
C:\stn-labz\rag\input
```

Future `!rag SRT-*` processing will place the applicable approved and successfully chained controlled document into this location before invoking `rag_builder`.

The RAG workflow is not an automatic startup operation.

It remains behind an explicit human-controlled command boundary.

## External Application Integration

Rictus is being developed as the command-and-control coordinator for additional STN-LABZ applications.

Planned integration includes:

```text
Rictus
   |
   +-- chain
   |
   +-- rag_builder
   |
   +-- Digit
```

These applications remain separate components.

Rictus will invoke them through controlled interfaces rather than incorporating their implementation into the Rictus core or Intelligence module.

## Human-Controlled Pipeline

Installation of the required applications does not authorize Rictus to automatically advance information through the intelligence pipeline.

Pipeline transitions remain operator controlled:

```text
!show INT-*
!srt INT-*
!approve INT-* / !reject INT-*
!chain SRT-*
!rag SRT-*

Future:
!update_corpus
```

Each successful stage stops and reports its result before another human-authorized transition can occur.

## Build Products

Generated build products should remain outside normal source control.

Examples include:

```text
*.dll
*.exe
*.obj
*.pdb
x64/
Debug/
Release/
```

Runtime intelligence state and generated reports should likewise remain outside the source repository unless explicitly retained as controlled evidence.

## Development Status

Rictus does not yet have a stable end-user installer.

Until a formal installation package is established, deployment consists of:

1. Building the Rictus core.
2. Building the required module DLLs.
3. Deploying module DLLs to the configured runtime module directory.
4. Providing valid runtime configuration.
5. Starting Rictus.
6. Verifying communications and module state before operational use.

Build success alone does not establish qualification.

Core and module qualification remain separate validation requirements.