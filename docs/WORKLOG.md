# Work log — Lima Charlie

The running journal (`_Rules/PROJECT-DEFAULTS.md` → Working loop). Read the top at the start of a
session; update after each meaningful step and before ending one. Never rewrite or delete old entries.

## Done
- 2.0.4 released (mod + plugin), protocol 6; radio keys open the TeamSpeak mic, transmit-key picker (CHANGELOG.md).
- Direct speech with voice levels, occlusion (room model + traces), room reverb, radios with garble and
  terrain, AI hearing, Game Master support (docs/how-it-works.md).
- Verified in multiplayer earlier: attenuation, muffling, range switching (2-player test, 2026-09-14).

## To do
- 3.0 comms expansion (feature branch `claude/gallant-knuth-zna9uk`): manpacks/speakers/handsets, vehicle
  radios/infantry phone/intercom, EW/jamming/SATCOM. Plan + open questions: docs/design/3.0-comms-expansion.md.
  Order: shared foundation + vehicles first, then manpacks, then EW/relays/SATCOM. Testing local only.
  Next: survey vanilla vehicle radios and the VON entry add/remove API (needs References/ on Duke's PC);
  then the foundation (radio source/position, leases, one quality function).
- Bugfixes keep shipping from `main` as 2.x.y; merge `main` into the feature branch after each release.
- Not yet tested: retail game client with BattlEye.
- Decide whether `WorkshopCoverPicture.*` and the new `PROJECT.md` / `docs/research/` / this file get committed.

## Touch-ups
- Terrain answers can be missed: plugin assumes line of sight after 300 ms without reporting it (docs/troubleshooting.md, Known fragilities).
- Wishlist not started: simplex/duplex, vehicle intercom component, babble languages, relays/satcom, EMP.

---

## 2026-10-28
- Duke's answers recorded in docs/design/3.0-comms-expansion.md (Decisions): local testing; foundation +
  vehicles first; owner keeps transmitting while a handset is borrowed; intercom on direct speech with a
  No Intercom option, as a 4-option cycling entry in the VON menu; jammers as GM props; SATCOM a placeable
  inventory item. Designed relays and SATCOM as one relay path; explained direction finding. Added Duke's
  survey docs/research/2026-10-28-terrain-autoconnect-vehicle-comms.md to the branch.
- Created the 3.0 feature branch `claude/gallant-knuth-zna9uk` from `main` 71b7e2b (cloud session) and wrote
  docs/design/3.0-comms-expansion.md: scope, branch strategy for parallel bugfix releases, shared foundation
  (radio source/position, server leases for borrowed/vehicle radios, one quality function), per-workstream
  plan, provisional bridge changes, open questions. No code yet.
- Note: GitHub `main` lacks the local survey `docs/research/2026-10-28-terrain-autoconnect-vehicle-comms.md`
  (referenced from the workspace wiki). (Later the same day: Duke attached it; now on this branch.)

## 2026-10-19
- Migrated into `AReforgerWorkspace/Projects/Lima Charlie Non-Enfusion` (repo root, `addon` junction);
  `git status` clean against HEAD ee92f7b. Research notes moved to `docs/research/`. Worklog seeded from
  PROJECT.md, CHANGELOG.md and docs/ — no new development this session.
