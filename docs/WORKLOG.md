# Work log — Lima Charlie

The running journal (`_Rules/PROJECT-DEFAULTS.md` → Working loop). Read the top at the start of a
session; update after each meaningful step and before ending one. Never rewrite or delete old entries.

## Done
- 2.0.4 released (mod + plugin), protocol 9 (was mis-noted as 6); radio keys open the TeamSpeak mic, transmit-key picker (CHANGELOG.md).
- Direct speech with voice levels, occlusion (room model + traces), room reverb, radios with garble and
  terrain, AI hearing, Game Master support (docs/how-it-works.md).
- Verified in multiplayer earlier: attenuation, muffling, range switching (2-player test, 2026-09-14).

## To do
- Branch `hearing-hook` (2.0.5, testing): `LC_Hearing` hook for ear-pro mods (OTAC Ear Pro compat first).
  Built + offline tests pass; Workbench Game module compiles. Not yet run in game: needs a compat (or a
  test caller) and a 2-client TeamSpeak listening test. Release number (2.0.5 vs 2.1.0) is Duke's call.
- Next planned major: electronic warfare (own branch, merged with the major release).
- Not yet tested: retail game client with BattlEye.
- Decide whether `WorkshopCoverPicture.*` and the new `PROJECT.md` / `docs/research/` / this file get committed.

## Touch-ups
- Terrain answers can be missed: plugin assumes line of sight after 300 ms without reporting it (docs/troubleshooting.md, Known fragilities).
- Wishlist not started: simplex/duplex, vehicle intercom component, babble languages, relays/satcom, EMP.

---

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

## 2026-11-03 (bugfix-2.0.6)
- Unit session logs (2.0.5, 4 clients) triaged before reading the external handover:
  docs/investigations/2026-11-03-session-logs.md. Confirmed: GM channel scramble = positional TX-key default
  over vanilla `m_aEntries`, reordered by vanilla `RemoveEntry`'s unordered `RemoveItem` (Tim's log, editor close);
  radio stutter contributor = OTAC `OT_VonDisplay` `m_wRole` VM exceptions on the "+N" VON slot, re-raised by our
  20 Hz feed (748 in the logs). Code hot spots (rooms Bellman-Ford per write, 117-area apartments; traces) not
  measured. Reverb fade in big spaces is by design. Terrain not captured. Nothing committed in the project.
- External FPS handover read and compared (investigation doc, "Comparison" section). Verified their room-scanner
  no-Finish bug, per-trace collider volume under room diagnostics, outdoor use of indoor graphs, LC reset at the
  20:47 reload; Linstone's plugin is 2.0.1; Parko's clock is UTC+10:30. Did their open diagnostic-flag audit: flags off
  = no optional work. Revised: mapped layouts add area-wide solver work over a session (fits "runway fixes it").
- Layout change (Duke): `addon\` (Workbench "Lima Charlie") now holds the **2.0.x line**; branch `bugfix-2.0.6` from
  `main` (= v2.0.5 + release docs). The 3.0 work lives in Workbench "Lima Charlie DEV", junctioned as `addon-dev\`
  (local only, in `.git/info/exclude`). DEV matched feature/3.0-comms 17bf712 at the switch (only two .edds.meta
  trailing newlines differ). feature/3.0-comms is 9 commits ahead of origin (unpushed).
- **2.0.6 fixes** on `bugfix-2.0.6` (both halves `2.0.6`, protocol 9):
  - scramble: modded `SCR_VONController.RemoveEntry` puts back the entry vanilla's `RemoveItem` swapped in;
    `LC_GetRadioEntries` lists GM editor entries last.
  - VON overflow: `LC_VonDisplay.LC_IsStuckOverflow` (additional and never active); the feed skips those speakers.
  - rooms: `Build` finishes on grid exhaustion, cap inside the loop; per-area adjacency at `Finish`; `LC_RoomPaths`
    cache (8 slots, keyed subscene + layout + source area, LRU), portals re-read every 250 ms, search
    (queue-based) only on a cost change.
  - diag: collider volume only when the room line is due.
  - plugin: per-player muffle and room glide (`lc_direct_voice_ease`, tau 200 ms) in `publish_voice`.
  - Evidence: plugin build /W4 clean, "All checks passed", `LimaCharlie_2.0.6.ts3_plugin`; enforce_check 37 files
    clean; Workbench (ClaudeDevTools → Lima Charlie) logs_2026-11-03_22-54-22 `22:54:28.683 Module: Game; loaded
    5697x files; 11155x classes`, 0 `SCRIPT (E)`.
  - NOT tested in play or MP: editor close and radio drop order, overflow with OTAC, apartment frame times, muffle
    glide by ear.
