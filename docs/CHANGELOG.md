# Changelog

All notable changes to `Rictus` will be documented in this file.

The project is currently in active development.

## [Unreleased]

### Added

- Initial STN-LABZ repository baseline.
- Windows ISO C core implementation.
- IRC communications over TLS.
- IRCv3 capability negotiation.
- SASL PLAIN authentication support.
- Configuration loading and validation.
- Session and shutdown management.
- Core logging.
- Private-message command interface.
- Core-controlled hot-load DLL module architecture.
- Module discovery, registry, inventory, lifecycle, qualification, and activation controls.
- Persistent module qualification state.
- Intelligence module.
- Approved intelligence source registry.
- External intelligence collection and parsing.
- Persistent `INT-*` intelligence records.
- Intelligence evidence retrieval through `!show INT-*`.
- Relevance-oriented operator notification designed to surface selected findings rather than reproduce general news feeds.
- Security Research Target candidate handoff through `!srt INT-*`.
- SRT candidate Markdown generation under `C:\stn-labz\reports\SRT`.
- Controlled-document preparation fields for SRT candidates, including `sha256`.
- Persistent SRT request state.
- Human-controlled Intelligence-to-Security-Research pipeline design.
- Approved `INT-*` to `SRT-*` identity transition design.
- Human accountability model for pipeline transitions, including operator identity, accountable office, timestamp, and result.
- Command lifecycle design for `!approve`, `!reject`, `!chain`, and `!rag`.
- Planned controlled application interface for coordination with `chain`, `rag_builder`, and Digit.

### Changed

- Rictus mission scope expanded from communications transport to communications and external intelligence coordination.
- Intelligence notifications changed toward concise operator alerts backed by retained evidence accessible through `!show`.
- Intelligence source strategy expanded to support multiple approved sources while filtering findings according to STN-LABZ mission relevance.
- SRT handling defined as a human-controlled transition rather than an automatic escalation.
- Candidate SRT documents retain their originating `INT-*` identity until human approval.
- Approved SRTs will receive a permanent `SRT-*` identity while preserving the originating `INT-*` identifier as provenance.
- Pipeline architecture now requires an explicit human command at every authority boundary.
- Rictus is defined as the command-and-control coordinator for specialized STN-LABZ applications rather than an implementation replacement for those applications.

### Validated

- Rictus Core qualification: 20/20 PASS.
- Windows TCP connection to IRC TLS port: PASS.
- Schannel TLS transport: PASS.
- IRCv3 capability negotiation: PASS.
- SASL PLAIN authentication: PASS.
- IRC channel join and maintained encrypted session: PASS.
- Core module discovery and loading path: PASS.
- Intelligence module load and activation: PASS.
- Intelligence source collection: PASS.
- `INT-*` record creation and persistence: PASS.
- `!show INT-*` command handling: PASS.
- Unknown-command handling: PASS.
- `!srt INT-*` request handling: PASS.
- SRT candidate report generation: PASS.
- SRT report output under `C:\stn-labz\reports\SRT`: PASS.
- Human-review placeholders in generated SRT candidate documents: PASS.

### Planned

- Persistent collision-safe `SRT-*` identity allocator.
- `!approve INT-*` command.
- `!reject INT-*` command.
- Core application execution interface.
- `!chain SRT-*` command and deterministic `chain` result handling.
- Reporting of the authoritative `sha256` returned through successful Trust Chain processing.
- `!rag SRT-*` command.
- Controlled copy of approved and successfully chained SRT documents into `C:\stn-labz\rag\input`.
- `rag_builder` invocation and `CORPUS REBUILT` result reporting.
- Future Digit corpus-update command integration.

### Operational Model

The developing human-controlled pipeline is:

```text
!show INT-*
    |
    v
Human inspection
    |
    +-- no action ----------------------> INT retained / STOP
    |
    +-- !srt INT-* --------------------> Candidate / Pending
                                             |
                                   +---------+---------+
                                   |                   |
                              !reject INT-*       !approve INT-*
                                   |                   |
                                   v                   v
                                Rejected             SRT-*
                                / STOP              Approved
                                                       |
                                                  !chain SRT-*
                                                       |
                                                  PASS / sha256
                                                       |
                                                      STOP
                                                       |
                                                   !rag SRT-*
                                                       |
                                                CORPUS REBUILT
                                                       |
                                                      STOP
                                                       |
                                            future !update_corpus
```

No successful stage automatically authorizes the next stage.

Rictus carries evidence between systems. The operator carries authority between states.
