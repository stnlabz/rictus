# Contributing to Rictus

## Purpose

Contributions to `Rictus` must preserve the project's deterministic, modular, traceable, and human-controlled design.

Rictus is the STN-LABZ communications, command-and-control, and external intelligence coordination agent.

Contributions must preserve the separation between communications infrastructure, module capabilities, specialized STN-LABZ applications, and human authority.

## Development Expectations

Contributions will:

- Keep the Rictus core focused on communications, orchestration, module governance, and shared host services.
- Place specialized mission behavior in clearly defined modules.
- Preserve the hot-load DLL module architecture.
- Preserve source and intelligence provenance.
- Preserve explicit human authority over pipeline state transitions.
- Avoid automatic advancement across human-controlled pipeline boundaries.
- Avoid adding external dependencies without documented justification and authorization.
- Treat configuration as data rather than hard-coded environment state.
- Use explicit failure handling.
- Fail closed when required state, evidence, authorization, or component results cannot be established.
- Preserve deterministic behavior across component boundaries.
- Include tests for new or changed behavior.
- Update affected documentation when behavior, architecture, commands, or interfaces change.

## Source Style

The primary implementation language is ISO C.

Windows development currently targets Visual Studio.

Code will favor:

- Small functions
- Explicit ownership
- Bounded memory handling
- Clear return values
- Defensive input validation
- Minimal global state
- Predictable execution paths
- Explicit component boundaries
- Deterministic failure behavior

Platform-specific implementation will remain separated from portable core behavior where practical.

## Core and Module Boundaries

The Rictus core owns shared system responsibilities including:

- Configuration
- Communications
- IRC session management
- Command routing
- Module discovery
- Module registry and inventory
- Module lifecycle
- Module qualification control
- Module activation
- Logging
- Shared host services
- Controlled application execution interfaces

Modules own bounded mission-specific capabilities.

The Intelligence module, for example, owns intelligence collection, parsing, relevance processing, retained intelligence records, and Security Research Target candidate handoff behavior.

Modules will not bypass core-controlled lifecycle, qualification, or activation requirements.

## Module Development

Creation of a new module requires an approved STN-LABZ Module Creation Request before implementation begins.

Bug fixes, maintenance changes, and ordinary revisions to an existing module do not require creation of a new Module Creation Request.

All modules remain subject to the established core-controlled module qualification requirements.

Required qualification tests cannot be bypassed, suppressed, or replaced by the module under test.

A modified module must complete the qualification requirements applicable to that revision before qualification is restored.

## Human-Controlled Pipeline

Contributions affecting the Intelligence-to-Corpus pipeline must preserve explicit human authorization between stages.

The developing command lifecycle is:

```text
!show INT-*       Inspect retained intelligence evidence
!srt INT-*        Create an SRT candidate
!approve INT-*    Approve candidate and assign SRT identity
!reject INT-*     Reject candidate
!chain SRT-*      Invoke Trust Chain processing
!rag SRT-*        Authorize corpus ingestion

Future:
!update_corpus    Authorize Digit corpus update