# Instructions

These instructions are for AI assistants working in this project.

Always open `@AGENTS.md` when the request:
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

## Language Policy

All code (code, documentation, etc.) MUST be written in **English** only. This ensures consistency, maintainability, and ease of collaboration for all contributors.

Chat and interactive discussions with the AI assistant may continue in French if desired, but all committed files and code must remain in English.

## Coding Rules

Coding rules (formatting, SOLID design guidance language policy and related guidelines, KISS/DRY) have been consolidated into a dedicated document for the firmware subtree.

Refer to: `CODING_RULES.md` for the authoritative, project-level coding rules. All contributors and assistants must follow the rules documented there when modifying code or OpenSpec files.

**Profile / mode identifiers:** never compare raw `uint8_t` profile values (e.g. `4U`) in feature code. Use `ControllerOperationProfile` from `ESP32/src/common/controller/controller_operation_profile.h` or named constants from `src/Config/*.h`.

If you make changes to coding rules, update `CODING_RULES.md`.

## Ponytail (minimal code policy)

For feature work and refactors, follow `.cursor/rules/ponytail.mdc`: simplest working solution, stdlib/native first, no speculative abstractions. Deliberate shortcuts use a `ponytail:` comment with ceiling and upgrade path. Debt scan: grep `ponytail:` in the repo.

## Architecture Context

Before changing the firmware, command flow, telemetry, or dashboard UI, read the root README and the architecture documentation:

- [README.md](README.md)
- [ESP32/docs/architecture/README.md](ESP32/docs/architecture/README.md)
- [ESP32/docs/CODE_CONTEXT_INDEX.md](ESP32/docs/CODE_CONTEXT_INDEX.md)

Use the architecture README and diagram to understand how messages move between the dashboard, leader board, and follower board.

## Structural Check Execution Paths

To avoid path mistakes and wasted iterations, always run structural checks with an explicit project root.

- From repository root (`SoArm101-WaveshareEsp32/`):
	- `python ESP32/tools/check_structural_limits.py --project-root ESP32`
- From firmware root (`SoArm101-WaveshareEsp32/ESP32`):
	- `python tools/check_structural_limits.py --project-root .`

Do not run `python tools/check_structural_limits.py` from repository root.

When validating a multi-file change in firmware/dashboard:
- Run structural checks first.
- Then run `pio run -e leader` and `pio run -e follower`.
- Then run dashboard Python syntax checks.