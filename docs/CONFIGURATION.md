# Configuration

## Purpose

`Rictus` is configuration-driven.

Configuration defines the communications environment, runtime paths, module discovery location, and other deployment-specific values required by the Rictus core.

Mission policy and human authority are not configuration substitutes. Configuration enables operation; it does not grant pipeline authority.

## Runtime Configuration

Rictus configuration supports the values required to establish and maintain its IRC session.

Current configuration areas include:

- IRC server
- IRC port
- IRC nickname
- IRC username
- IRC real name
- IRC channel
- SASL account
- SASL authentication value
- Module discovery path
- Logging behavior

The exact supported keys are defined by the current Rictus core implementation and must pass configuration validation before normal operation begins.

## Communications

Rictus currently operates over IRC using TLS.

The communications path includes:

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
   +-- channel session
   +-- private-message command interface
```

Configuration supplies the deployment-specific connection values used by this path.

## Module Discovery

Rictus uses a hot-load DLL module architecture.

The configured module directory is scanned by the core. Discovered modules remain subject to core-controlled identity validation, API compatibility checks, qualification state, lifecycle management, and activation controls.

Presence in the module directory does not itself authorize activation.

The Intelligence module is deployed as a DLL through this module-loading architecture.

## Intelligence Runtime State

The Intelligence module maintains operational evidence and state separately from source code.

Current Intelligence-related runtime artifacts include retained intelligence records, seen-item state, SRT request state, and generated Security Research Target candidate documents.

Generated SRT candidate documents are written under:

```text
C:\stn-labz\reports\SRT
```

SRT request state is maintained under the same operational area.

These files are runtime evidence/state and are not module source artifacts.

## Security Research Target Output

The `!srt INT-*` command creates a candidate Security Research Target document.

Candidate documents are generated under:

```text
C:\stn-labz\reports\SRT
```

The candidate remains Pending until a later explicit human decision.

Future approval processing will assign an `SRT-*` identity to an approved candidate while retaining its originating `INT-*` identifier as provenance.

## Trust Chain Coordination

Rictus will coordinate with the STN-LABZ `chain` application through a controlled application interface.

The planned command boundary is:

```text
!chain SRT-*
```

Only an approved SRT may proceed to this stage.

Rictus will wait for deterministic completion and report the result to the operator. A successful Trust Chain operation will include reporting the authoritative `sha256` produced through the controlled-document process.

A successful `!chain` operation stops after reporting. It does not automatically authorize corpus ingestion.

## RAG Coordination

The approved RAG input location is:

```text
C:\stn-labz\rag\input
```

The planned command boundary is:

```text
!rag SRT-*
```

Before invoking `rag_builder`, Rictus will require the applicable approved and successfully chained state for the requested SRT.

The controlled document will then be copied into the RAG input location and `rag_builder` invoked through the controlled application interface.

Successful completion will be reported to the operator as a corpus rebuild result.

A successful corpus rebuild does not automatically authorize Digit to update its active corpus.

## Human-Controlled Commands

Pipeline advancement is controlled by explicit operator commands rather than configuration.

The developing command lifecycle is:

```text
!show INT-*       Inspect intelligence evidence
!srt INT-*        Create SRT candidate
!approve INT-*    Approve candidate and assign SRT identity
!reject INT-*     Reject candidate
!chain SRT-*      Invoke Trust Chain processing
!rag SRT-*        Invoke approved corpus ingestion

Future:
!update_corpus    Authorize Digit corpus update
```

Each successful stage stops and reports its result.

Configuration must not be used to enable automatic progression between these human-controlled states.

## Validation Rules

Configuration is validated before dependent operations begin.

Invalid, missing, unsupported, or unusable required configuration causes the affected operation to fail explicitly rather than silently falling back to unknown behavior.

Runtime paths must resolve to the state required by the operation that uses them.

Module discovery and application coordination remain subject to their own validation and lifecycle controls even when their configured paths are syntactically valid.

## Repository Boundary

Repository configuration examples may document expected keys and path formats.

Machine-specific runtime state and generated operational artifacts are kept outside the source tree.

Build products and runtime artifacts such as DLL binaries, object files, debug symbols, intelligence records, seen-item state, SRT request state, and generated reports are not source documentation and should remain excluded from normal source control unless explicitly required as controlled evidence.

## Future Configuration

As Rictus application coordination is implemented, additional configuration may define the controlled locations of:

- `chain`
- `rag_builder`
- RAG input
- RAG output or corpus artifacts
- Digit command interface
- Additional approved Rictus modules

Adding a path to configuration will not itself grant authority to invoke the associated application. Human-control gates and component validation remain authoritative.
