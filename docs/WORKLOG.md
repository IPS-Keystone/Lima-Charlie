# Work log — Lima Charlie

The running journal (`_Rules/PROJECT-DEFAULTS.md` → Working loop). Read the top at the start of a
session; update after each meaningful step and before ending one. Never rewrite or delete old entries.

## Done
- 2.0.5 released 2026-10-29 (tag v2.0.5): hearing hook for ear-pro mods, louder radios + limiter, terrain as shares per hill. Protocol still 9.
- 2.0.4 released (mod + plugin), protocol 9 (was mis-noted as 6); radio keys open the TeamSpeak mic, transmit-key picker (CHANGELOG.md).
- Direct speech with voice levels, occlusion (room model + traces), room reverb, radios with garble and
  terrain, AI hearing, Game Master support (docs/how-it-works.md).
- Verified in multiplayer earlier: attenuation, muffling, range switching (2-player test, 2026-09-14).

## To do
- 3.0 comms expansion (feature branch `feature/3.0-comms`): manpacks/speakers/handsets, vehicle
  radios/infantry phone/intercom, EW/jamming/SATCOM. Plan + open questions: docs/design/3.0-comms-expansion.md.
  Order: shared foundation + vehicles first, then manpacks, then EW/relays/SATCOM. Testing local only.
  `main` 2.0.5 merged in 2026-10-31. Next: survey (vehicle radios, VON entry API, compartments), then the
  foundation (radio source/position, leases, one quality function).
  Handover with current state, collisions (`radio-tuning`) and next steps: docs/design/3.0-handover.md.
- Bugfixes keep shipping from `main` as 2.x.y; merge `main` into the feature branch after each release.
- Not yet tested: retail game client with BattlEye.
- Decide whether `WorkshopCoverPicture.*` and the new `PROJECT.md` / `docs/research/` / this file get committed.

## Touch-ups
- Terrain answers can be missed: plugin assumes line of sight after 300 ms without reporting it (docs/troubleshooting.md, Known fragilities).
- Wishlist not started: simplex/duplex, vehicle intercom component, babble languages, relays/satcom, EMP.

---

## 2026-10-29
- Wrote docs/design/3.0-handover.md. Found Duke's `radio-tuning` branch (2.0.5, unmerged) rewrites the
  terrain/quality code the groundwork touches and adds the same survey file: wait for it to reach `main`
  and merge `main` here before the plugin-side groundwork.

## 2026-10-28
- Duke's second round recorded (design doc, "Second round"): branch renamed to `feature/3.0-comms`, test
  builds 3.0.N; any seat any intercom channel; jammers player-switchable/destroyable and jam transmissions
  from inside their radius with noise; SATCOM no delay, forced half-duplex, duplicates your radios, Link
  Radio action (non-exclusive, auto-unlink), jammer-immune, unlimited TX / normal RX; relays player + GM
  placeable on the vanilla relay prop; direction finding is a stretch goal for another branch.
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

## 2026-10-28
- OTAC Ear Pro compat survey (2026-10-27): coexists with LC as is, no shared files/classes; its audio
  shapes the game mixer only. Agreed route: generic LC hook + a separate compat addon on the OTAC side.
- Checked local vs GitHub: local `main` = origin/main 71b7e2b (2.0.4 + docs), 9 commits past tag v2.0.1;
  GitHub's latest release is still v2.0.1 (2.0.2-2.0.4 never got GitHub releases). Nothing uncommitted.
- Built the hook on `hearing-hook`, answering OTAC's review (zero = bypass before clamps, per-source
  clearing, combination rules, 1 s expiry on `System.GetTickCount()`, room reverb heard through the
  headset, radio separate, no protocol bump). Contract in docs/developer-reference.md.
  Evidence: `plugin\build.bat` clean at /W4, "All checks passed" incl. new parse + headset DSP tests;
  `reforger_check_enforce` clean; Workbench log 07:45 Game module OK (5697 files) with LC loaded.
- Open: in-game test with a caller; tune with the OTAC side (High voice gain, drive amount).
- Branch `radio-tuning` (on top of `hearing-hook`): radio gain 0.8 -> 1.85 with a soft limiter (a04a4ea).
  Measured through lc_audio: full-volume radio was 5-6 dB under a talker beside you; now +2.1 dB at speech
  level, +1.5 raised, level at a shout, no clipping. Tests pass. Not yet heard in game.
- Survey written: docs/research/2026-10-28-terrain-autoconnect-vehicle-comms.md (terrain options A-D, CVON
  3.0.1 auto-connect, vehicle radios/intercoms in CVON/TFAR/ACRE2). Waiting on Duke: terrain option, go for
  auto-connect, vehicle comms scope.
- Radio gain 1.85 -> 1.72 (+18% over a voice beside you at speaking level), channels default to 80% (11405a3).
- Terrain A+B built, **uncommitted** on `radio-tuning`, awaiting Duke's OK on the numbers: game clearance is
  now the worst ground's height above the line of sight (no near-end blow-up, no 10 m minimum); plugin takes
  share = 0.5 * (1 - e^(-h/60)) of the range (no d/2000 term). Tests pass, Workbench compile OK 09:34.
  Docs (how-it-works Terrain, LC_Settings terrain description "100 is the TFAR model") still to update.
- Terrain revised (still **uncommitted**): game sends every hill above line of sight (`links[].hills`, top 8,
  `clearance` = highest kept for old plugins); plugin combines them as shares of what is left, per hill
  0.8 * (1 - e^(-h/95)), total cap 90%. Small hills as before, big ones stronger (250 m: 49% -> 74%).
  Build + tests pass, Workbench compile OK 09:44. Awaiting Duke's OK on the numbers; docs still to do.
- Terrain approved by Duke and committed with docs (how-it-works Terrain table, developer-reference links[],
  player guide, server settings/setup, in-game terrain setting description, CHANGELOG).

## 2026-10-29
- Duke is about to publish 2.0.5 (hearing hook + radio-tuning). Hook files unchanged since bb57f84; radio
  path now ends in the radio soft limiter after channel volume.
- OTAC handover revision 2 written for the OTAC side (outside the repo, Documents/Claude/
  LC-OTAC-EarPro-Handover.md): final contract, answers to OTAC's 28 Oct review, example rewritten for
  their static WriteSignals/UpdateLocalHearing (signatures to verify against their source), joint tests.

## 2026-10-29
- Release notes since 2.0.1: docs/release-notes-2.0.5.md; CHANGELOG 2.0.3 now lists the server.json log line.
- Clean plugin build from radio-tuning 46a874e: plugin/build/LimaCharlie_2.0.5.ts3_plugin (package.ini 2.0.5,
  DLL carries 2.0.5, vanilla beeps excluded), all checks passed. Not yet tested in game.
- New on GitHub, not ours this session: feature/3.0-comms and claude/gallant-knuth-zna9uk (3.0 plan docs only).
