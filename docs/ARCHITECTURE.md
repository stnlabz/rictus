# Architecture

## Overview

`Rictus` is the STN-LABZ communications, command-and-control, and external intelligence coordination agent.

Rictus is implemented as an ISO C core with hot-load DLL modules. The core owns communications, command routing, module lifecycle, and shared host services. Modules provide bounded mission capabilities through explicit interfaces.

Rictus coordinates specialized STN-LABZ applications without absorbing their responsibilities.

```text
Approved External Sources
          |
          v
+-------------------------+
| Rictus Intelligence DLL |
+------------+------------+
             |
             v
       INT-XXXXXXXX
             |
             v
+-------------------------+
|       Rictus Core       |
|                         |
| IRC / TLS / SASL        |
| Command Routing         |
| Module Lifecycle        |
| Host Services           |
| Application Interface   |
+------------+------------+
             |
             v
          Operator
```

The operating principle for Rictus Intelligence is:

> What does STN-LABZ need to know before it becomes important?

Collection is intentionally broader than notification. Rictus may inspect many approved sources while presenting only a small number of items that warrant operator attention.

## Core Responsibility

The Rictus core coordinates communications and module execution.

Core responsibilities include:

- Configuration loading
- IRC connection and session management
- TLS transport
- SASL authentication
- Command routing
- Module discovery
- Module registry and inventory
- Module lifecycle control
- Core-controlled module qualification
- Logging and operational state
- Shared host services
- Controlled application execution interfaces

The core does not embed mission-specific intelligence logic that belongs in the Intelligence module.

## Module Architecture

Rictus capabilities are extended through hot-load DLL modules.

Modules operate through the Rictus module interface and remain subordinate to core lifecycle and qualification controls.

A module does not inherit trust merely because it can be discovered or loaded. Required qualification is controlled by the core, and activation is permitted only after the applicable qualification requirements have been satisfied.

Current module development includes:

```text
Rictus Core
    |
    +-- Intelligence DLL
            |
            +-- Approved source registry
            +-- Collection
            +-- Parsing
            +-- Relevance processing
            +-- Intelligence records
            +-- Operator commands
            +-- SRT candidate handoff
```

The Intelligence module owns intelligence-specific behavior. The core owns the communications and execution mechanisms through which that behavior reaches operators and other STN-LABZ systems.

## Intelligence Model

Rictus Intelligence follows the principle:

> Find what STN-LABZ needs, not what the Internet is shouting.

An approved source is not automatically an intelligence finding.

The module collects from approved sources, evaluates candidate material, retains evidence under an `INT-*` identifier, and notifies the operator only when an item reaches the applicable relevance threshold.

A notification is intentionally concise:

```text
[Rictus Intel] Hey, I found one thing I think you should inspect. INT-XXXXXXXX
```

The evidence remains available through the command interface:

```text
!show INT-XXXXXXXX
```

This separation keeps IRC focused on signal rather than becoming a general news feed.

## Human-Controlled Intelligence Pipeline

Rictus carries evidence between systems. The operator carries authority between states.

No successful stage automatically authorizes the next stage.

```text
Rictus Intelligence
        |
        v
   INT-XXXXXXXX
        |
        |  !show INT-XXXXXXXX
        v
  Human Inspection
        |
        +--------------------------+
        |                          |
        | no further action        | !srt INT-XXXXXXXX
        v                          v
   INT retained              SRT Candidate
   pipeline stops            Status: Pending
                                   |
                                   | research / review
                                   |
                         +---------+---------+
                         |                   |
                         | !reject INT-*     | !approve INT-*
                         v                   v
                      Rejected          SRT-YYYYMMDD-###
                      retained          Status: Approved
                                             |
                                             | !chain SRT-*
                                             v
                                           chain
                                             |
                                      +------+------+
                                      |             |
                                     FAIL          PASS
                                      |             |
                                     STOP      sha256 reported
                                                    |
                                                   STOP
                                                    |
                                             !rag SRT-*
                                                    |
                                                    v
                                           approved chained
                                               document
                                                    |
                                                    v
                                    C:\stn-labz\rag\input
                                                    |
                                                    v
                                             rag_builder
                                                    |
                                      +-------------+-------------+
                                      |                           |
                                     FAIL                        PASS
                                      |                           |
                                     STOP                  CORPUS REBUILT
                                                                  |
                                                                 STOP
                                                                  |
                                                        future operator
                                                           command
                                                                  |
                                                                  v
                                                          Digit corpus
                                                             update
```

Each transition across an authority boundary requires an explicit operator command.

## Intelligence and SRT Identity

An intelligence finding begins with an `INT-*` identity.

Example:

```text
INT-03ABC206
```

If the operator invokes:

```text
!srt INT-03ABC206
```

Rictus creates a candidate Security Research Target document. The candidate remains associated with the originating `INT-*` identity while its status is Pending.

Approval is the identity transition.

When an authorized operator approves the candidate, the approved Security Research Target receives a permanent `SRT-*` identity while retaining the originating `INT-*` identifier as provenance.

```text
INT-03ABC206
      |
      | !approve INT-03ABC206
      v
SRT-20260820-001
Source Intelligence ID: INT-03ABC206
```

Rejected candidates remain retained as historical records and do not proceed to controlled-document processing.

## Operator Accountability

Human-authorized transitions record sufficient information to identify who exercised authority.

The operational record includes, as applicable:

- Command
- Intelligence or SRT identifier
- Operator identity
- Accountable office or authority
- Timestamp
- Result

For SRT review, the document maintains a human-review record so a later reviewer can determine who approved or rejected the transition and under which organizational authority.

## Command Interface

The intelligence pipeline uses explicit commands as human-control gates.

Current and planned command roles are:

```text
!show INT-*       Inspect retained intelligence evidence
!srt INT-*        Create an SRT candidate
!approve INT-*    Approve candidate and assign SRT identity
!reject INT-*     Reject candidate and retain disposition
!chain SRT-*      Invoke controlled-document Trust Chain processing
!rag SRT-*        Authorize corpus ingestion and invoke rag_builder

Future:
!update_corpus    Authorize Digit to update its active corpus
```

A command completing successfully does not imply permission to execute the next command.

## External Application Coordination

Rictus is the command-and-control coordinator for the pipeline.

Specialized applications retain their own responsibilities:

```text
Rictus
  |
  +-- chain
  |     |
  |     +-- controlled-document identity
  |     +-- revision lineage
  |     +-- reference validation
  |     +-- authoritative sha256
  |
  +-- rag_builder
  |     |
  |     +-- approved input processing
  |     +-- validation
  |     +-- corpus construction
  |
  +-- Digit
        |
        +-- authorized corpus consumption
```

Rictus invokes these applications through controlled interfaces, waits for deterministic completion, and reports the resulting evidence to the operator.

Rictus does not calculate the authoritative controlled-document `sha256` on behalf of `chain`, and it does not duplicate `rag_builder` corpus-processing responsibilities.

## Failure Boundaries

Pipeline operations fail closed.

Examples include:

```text
chain FAIL
    -> report failure
    -> STOP
    -> do not authorize corpus ingestion

copy to rag input FAIL
    -> report failure
    -> STOP
    -> do not invoke rag_builder

rag_builder FAIL
    -> report failure
    -> preserve applicable evidence
    -> STOP
```

A failure in one component does not grant another component authority to bypass that failure.

## Source and Knowledge Authority

External information collected by Rictus is evidence, not automatically authorized STN-LABZ knowledge.

An `INT-*` record may remain intelligence-only indefinitely.

Only material that passes the required human review, controlled-document processing, and corpus authorization boundaries may proceed toward Digit.

The approved controlled document remains authoritative over derived corpus artifacts.

## Extensibility

Future Rictus modules may provide additional bounded mission capabilities through the established module interface.

New modules remain subject to the STN-LABZ module lifecycle, qualification requirements, and core-controlled activation model.

New capabilities will be added through explicit modules or controlled application interfaces rather than by expanding the core into mission-specific logic.
