# Architecture and UI Follow-up Plan

## Phase 1 - Documentation

- [x] Describe the current runtime architecture in English
- [x] Add a message flow SVG for dashboard, leader, and follower traffic
- [x] Link the architecture docs from the repo root README and ESP32 README
- [x] Point AGENTS.md to the architecture context

## Phase 2 - Dashboard layout cleanup

- [x] Fix the follower panel HTML structure
- [x] Remove duplicated position inputs
- [x] Restore visible follower detected IDs

## Phase 3 - Command feedback polish

- [x] Make follower debug enable/disable feedback easier to see
- [x] Verify request_id and ACK rendering stay synchronized
- [x] Confirm the follower debug flag refreshes correctly in the UI

## Phase 4 - Validation

- [x] Rebuild the firmware targets
- [x] Run native tests
- [ ] Recheck the dashboard in the browser