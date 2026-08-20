# Project Policy

## Purpose

This document records repository-level policy for `Rictus`.

Rictus is the STN-LABZ communications, command-and-control, and external intelligence coordination agent.

This project policy defines the boundaries under which Rictus communicates, coordinates modules, handles intelligence evidence, and interfaces with other STN-LABZ systems.

## Authority

STN-LABZ organizational policy governs this project where applicable.

Project-specific behavior remains subordinate to applicable organizational policy.

Rictus does not create its own authority.

Human operators remain responsible for decisions that cross defined authority boundaries.

## Mission

Rictus provides a controlled bridge between approved external information sources, IRC operational communications, and the STN-LABZ intelligence workflow.

The guiding intelligence question is:

> What does STN-LABZ need to know before it becomes important?

Rictus Intelligence will seek information relevant to the STN-LABZ mission rather than reproduce general Internet or news activity.

The operational collection principle is:

> Find what STN-LABZ needs, not what the Internet is shouting.

Broad collection does not imply broad notification.

Rictus may inspect many approved sources while presenting only findings that satisfy the applicable relevance criteria.

## Human Authority

Rictus carries evidence between systems.

The operator carries authority between states.

Rictus will not automatically advance intelligence through human-controlled pipeline boundaries merely because a preceding operation completed successfully.

Each controlled transition requires an explicit operator action.

The developing command lifecycle is:

```text
!show INT-*       Inspect retained intelligence evidence
!srt INT-*        Create a Security Research Target candidate
!approve INT-*    Approve a candidate and assign SRT identity
!reject INT-*     Reject a candidate
!chain SRT-*      Invoke controlled-document Trust Chain processing
!rag SRT-*        Authorize corpus ingestion

Future:
!update_corpus    Authorize Digit corpus update
```

Successful completion of one command does not authorize execution of the next command.

## Intelligence Authority

An item collected by Rictus is not automatically a Security Research Target and is not automatically authorized STN-LABZ knowledge.

Rictus may notify an operator that an intelligence record warrants inspection.

Example:

```text
[Rictus Intel] Hey, I found one thing I think you should inspect. INT-XXXXXXXX
```

The operator may inspect the retained evidence through:

```text
!show INT-XXXXXXXX
```

After inspection, the operator may take no further action.

No further action is a valid disposition.

The `INT-*` record remains retained as intelligence evidence and does not enter the Security Research or corpus pipeline.

## Security Research Target Transition

An operator may explicitly move an intelligence record into Security Research consideration through:

```text
!srt INT-XXXXXXXX
```

This action creates a candidate Security Research Target.

Creation of a candidate does not constitute approval.

The candidate remains Pending and requires human review.

The originating `INT-*` identity remains associated with the candidate during this stage.

## SRT Approval

An SRT candidate may proceed only after an explicit human approval decision.

Approval is performed through the applicable controlled command:

```text
!approve INT-XXXXXXXX
```

Approval creates the transition from intelligence-stage identity to permanent Security Research Target identity.

Example:

```text
INT-03ABC206
      |
      | !approve INT-03ABC206
      v
SRT-20260820-001
```

The approved SRT retains the originating `INT-*` identifier as provenance.

The `INT-*` identity is not discarded or rewritten.

Rejected candidates remain retained as historical records and do not proceed through the controlled-document or corpus pipeline.

## Human Accountability

Human-controlled transitions will preserve sufficient evidence to identify the authority responsible for the action.

The applicable operational record will include:

- Command
- Intelligence or SRT identifier
- Operator identity
- Accountable office or authority
- Timestamp
- Result

Security Research review records will identify the human reviewer and the organizational office under which the decision was made.

This information exists so later review can determine who authorized a transition, when it occurred, and under what authority.

## Intelligence Provenance

Rictus will preserve traceability from retained intelligence back to the evidence from which it was derived.

Applicable provenance may include:

- Intelligence identifier
- Source
- Source URL
- Source title
- Publication information when available
- Intelligence fingerprint
- Collection or request timestamp
- Originating module
- Human transition records

Missing evidence will not be silently invented.

## Source Authority

External information collected by Rictus is evidence.

Collection does not establish organizational truth.

An approved external source does not make every item published by that source relevant, correct, actionable, or authorized for downstream STN-LABZ use.

Rictus Intelligence evaluates material according to its assigned mission and retains provenance for findings presented to operators.

## Source Integrity

Rictus will preserve traceability between intelligence records and their originating evidence.

Rictus will not silently replace or reinterpret source evidence as approved STN-LABZ knowledge.

Downstream authorization remains subject to the applicable Security Research, controlled-document, Trust Chain, corpus, and human-review requirements.

## Trust

Trust will not be inferred solely from:

- File presence
- Filename
- Directory location
- Source reputation
- Module presence
- Successful collection
- Successful parsing
- Retrieval similarity
- Prior qualification of a different revision

Applicable approval, provenance, integrity, qualification, and Trust Chain requirements must be satisfied at the boundary where they apply.

## Controlled Documents

Approved Security Research Targets intended for downstream corpus processing become controlled documents.

Rictus does not independently establish the authoritative controlled-document `sha256`.

The STN-LABZ `chain` application owns its assigned controlled-document responsibilities, including applicable identity, revision lineage, reference validation, and authoritative `sha256` processing.

Rictus coordinates this operation through the human-controlled command boundary:

```text
!chain SRT-*
```

Only an approved SRT may proceed to this stage.

A failed Trust Chain operation stops the pipeline.

A successful Trust Chain operation is reported to the operator, including the authoritative `sha256` when available.

Successful Trust Chain processing does not automatically authorize RAG ingestion.

## Corpus Processing

Corpus ingestion requires a separate human-controlled command:

```text
!rag SRT-*
```

Only an SRT that satisfies the required approved and successfully chained state may proceed.

The applicable controlled document is copied to:

```text
C:\stn-labz\rag\input
```

Rictus then invokes `rag_builder` through the defined application interface.

`rag_builder` retains responsibility for its own validation and corpus-construction mission.

Rictus does not duplicate that functionality.

Successful processing is reported to the operator as a corpus rebuild result.

A successful corpus rebuild does not automatically authorize Digit to update its active corpus.

## Digit Boundary

Digit remains a separate STN-LABZ system.

Future corpus-update integration will remain behind an explicit human-controlled command boundary.

Conceptually:

```text
!update_corpus
```

Rictus may coordinate the request and report Digit's result.

Rictus will not treat successful corpus construction as implicit authorization for Digit to change its active knowledge state.

## Component Separation

Rictus coordinates specialized systems without absorbing their assigned responsibilities.

The intended boundary is:

```text
Rictus
  |
  +-- Intelligence Module
  |
  +-- chain
  |
  +-- rag_builder
  |
  +-- Digit
```

Rictus provides command-and-control coordination.

The Intelligence module provides bounded intelligence capabilities.

`chain` provides its controlled-document and Trust Chain functions.

`rag_builder` provides its approved source-to-corpus processing functions.

Digit consumes authorized corpus material through its defined interface.

## Modularity

Specialized Rictus capabilities will be implemented through modules with defined interfaces.

Rictus uses a core-controlled hot-load DLL module architecture.

New modules are subject to the applicable STN-LABZ Module Creation Request process.

Modules remain subject to core-controlled discovery, validation, qualification, lifecycle, and activation requirements.

A module cannot authorize itself, suppress required qualification, or bypass core lifecycle controls.

## Qualification

Build success does not establish qualification.

Qualification applies to the specific revision tested.

A modified core or module must satisfy the applicable qualification requirements before qualification is restored.

Required negative validation remains part of module qualification.

Qualification does not itself grant authority to cross a human-controlled operational boundary.

## Configuration

Configuration provides operational values required by Rictus.

Configuration does not grant organizational authority.

A configured application path, module path, source, or destination does not authorize Rictus to use that resource outside the applicable operational and human-control requirements.

Configuration will not be used to silently enable automatic progression through the Intelligence-to-Corpus pipeline.

## Failure Behavior

Rictus will fail closed at controlled boundaries.

A failed operation will not be silently converted into success.

A failed stage will not authorize the next stage.

Examples include:

```text
chain FAIL
    -> report failure
    -> STOP
    -> do not proceed to corpus ingestion

copy to RAG input FAIL
    -> report failure
    -> STOP
    -> do not invoke rag_builder

rag_builder FAIL
    -> report failure
    -> STOP
    -> do not report CORPUS REBUILT
```

Required evidence and state will be preserved according to the applicable component requirements.

## Operational Evidence

Rictus will retain applicable evidence necessary to reconstruct significant pipeline actions.

Operational evidence may include:

- Intelligence records
- Source provenance
- Module lifecycle results
- SRT candidate records
- SRT disposition
- Human transition records
- Trust Chain results
- Reported `sha256`
- RAG processing results
- Component failure results

Operational evidence is distinct from authorization.

The existence of a record proves that an event or state was recorded; it does not independently grant permission for another transition.

## Repository Boundary

Source code, project documentation, configuration examples, and applicable test material belong in the source repository.

Generated build products and ordinary runtime state remain outside normal source control unless explicitly retained as controlled evidence.

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

Intelligence records, generated SRT reports, request state, and other operational artifacts are runtime evidence rather than project source.

## Governing Principle

Rictus is designed to coordinate systems without collapsing their authority boundaries.

Evidence may move through automation.

Authority does not.

Every controlled transition remains attributable to the human authority that caused it.