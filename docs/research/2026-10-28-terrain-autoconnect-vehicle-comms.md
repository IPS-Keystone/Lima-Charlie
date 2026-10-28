# Terrain, TeamSpeak auto-connect, vehicle comms — survey 2026-10-28

Sources: LC `hearing-hook`/`radio-tuning` branches; Coalition VON 3.0.1 (`References/GithubRepos/Coalition-VON`,
`9517a39`, 2026-10-20); ACRE2 `c920bac`; TFAR `d76d8b3`; 506 IRRU Enhanced Radio 1.0.38. Nothing here is
implemented yet.

## 1. Terrain

### What LC does now (TFAR's model)
- Game (`LC_Terrain.Clearance`): samples ground every 25 m; if the line of sight is blocked, returns how far
  the **middle of the path** must rise for both halves to clear the ground, clamped 10–250 m.
- Plugin (`lc_radio_effective_distance`): `d + 7h + 7h·d/2000`, then quality from `effective / range`
  (garble starts at the clean fraction, 0.35 by default; dead at 1.0).

### Why it hurts long-range radios
1. **The clearance blows up for obstacles near either end.** For an obstacle `h` above the line of sight at
   fraction `t` of the path, the middle must rise `h / 2t`. A ridge 100 m from you on a 10 km path needs
   50× its height, so almost any hill close to you hits the 250 m cap and the maximum penalty.
2. **The `d/2000` term scales the penalty with distance.** At 10 km a capped ridge costs 1750 m × 6 = 10.5 km.

Example, 20 km radio, ridge 100 m from you (`clean` = garble starts, `dead` = nothing):

| Model | 10 m ridge clean / dead | 30 m ridge clean / dead |
|---|---|---|
| No terrain | 7.0 / 20.0 km | 7.0 / 20.0 km |
| **Current** (TFAR formula + LC clearance) | 3.6 / 9.7 km | 2.8 / 9.7 km |
| TFAR without the `d/2000` term | 5.3 / 18.3 km | 5.3 / 18.3 km |
| TFAR with the obstacle's real height above line of sight | 6.7 / 19.3 km | 6.1 / 17.9 km |
| Knife-edge dB (ITU-R P.526, 50 MHz) → range, path-loss exponent 3.5 | 3.4 / 9.7 km | 2.1 / 6.1 km |
| same, exponent 4 | 3.7 / 10.7 km | 2.5 / 7.1 km |

### Alternatives found
- **ACRE2** (`extensions/src/ACRE2Arma/signal/models`): a dB link budget (transmit power, antenna gain,
  free-space loss, receiver sensitivity). Settings: *Arcade*, *LOS Simple* (free-space + an ITU knife-edge
  loss summed over every terrain peak on the path), *LOS Multipath* (default: also adds paths reflected
  off nearby peaks, so signal leaks around hills), *Longley-Rice ITM* (experimental; full irregular terrain
  model, ~1800 lines). Needs real transmit power and frequency per radio, not a "range".
- **506 IRRU**: free-space loss + up to 3 knife-edge diffraction losses (Fresnel parameter ν, 4/3-earth
  bulge), loss in dB mapped to quality by fixed thresholds (75/85/92/100 dB). No per-radio range.
- **Coalition VON 3.0.1**: no terrain term found in its scripts or plugin; distance only.
- **Physics is just as harsh at VHF**: a real knife-edge model blocks as much as the current one (rows above).
  "Feels reasonable" therefore means a deliberate gameplay model, not a more physical one.

### Options for LC
A. **Fix the geometry, keep TFAR's shape**: measure the obstacle's real height above the line of sight
   (the highest, or the sum of the worst few), drop or soften the `d/2000` term. Smallest change: one game
   function and one plugin line. Server `terrainEffectPercent` still scales it.
B. **Terrain as a share of range**: the penalty becomes a fraction of the radio's own range, capped (e.g. a
   ridge can take at most 40 % of it), so a 30 km set behind a hill still reaches far while a 2 km set is
   still stopped. Combine with A's geometry.
C. **dB link budget** (ACRE LOS Simple / 506): most realistic, frequency-dependent, but every radio would
   need a power figure instead of a range, and it is no gentler.
D. **ACRE multipath or Longley-Rice**: far too heavy for a per-transmission game-side trace.

Recommendation: A + B.

## 2. Coalition VON automatic TeamSpeak connection (3.0.1)
- **Server config**: `TeamspeakServerIP` and `TeamspeakServerPassword` in its server JSON, read on the
  server into `CVON_VONGamemodeComponent` and **replicated to every client** as `[RplProp]`.
- **Game → plugin**: the client writes them into `$profile:/VONServerData.json` (`TSServerIp`,
  `TSServerPassword`, `InGameName`).
- **Plugin** (`plugin.c` `try_auto_connect`, worker thread): while in game and the address differs from the
  last one handled:
  1. If the *current* server tab is already connected to that IP, mark it handled and stop.
  2. Otherwise a Windows `MessageBox` (Yes/No, topmost) asks "Do you want to connect TeamSpeak to: Server,
     Nickname".
  3. No → remembered, never asked again until the address changes.
  4. Yes → closes the previous auto-connection it made, then `guiConnect(PLUGIN_CONNECT_TAB_CURRENT, ...)`
     with the server password and the **in-game name as nickname**; the existing channel move follows.
- Weak points to avoid in an LC version:
  - The `MessageBox` blocks the plugin's worker thread while it is open, so voice routing stops updating.
  - It is easy to miss behind a fullscreen game.
  - Only the current tab is checked, so a player connected in another tab is prompted anyway.
  - A hostname in the config never equals `CONNECTION_SERVER_IP`, so it would prompt every session.
  - The server password goes to every client (unavoidable if they are to connect; LC already does this for
    the channel password).
- LC shape that fits what we have:
  - `tsServer` / `tsServerPassword` in `server.json` + Workbench settings next to `tsChannel`.
  - The question asked **in game** (LC status notice / HUD, accept with a key), the answer passed to the
    plugin in `game_state.json`.
  - The plugin checks **all** server tabs (resolving the hostname), connects with `guiConnect` in a new tab,
    then runs the existing channel move. Nothing blocks the audio worker.

## 3. Vehicle radios and intercoms

| | Coalition VON 3.0.1 | TFAR | ACRE2 |
|---|---|---|---|
| Vehicle radio | `CVON_VehicleRadioComponent` on tracked, wheeled APC, helicopter, car and truck bases (cars and trucks military only). Crew seats only; it appears at the end of their radio list, tunable like any radio. Starts on a per-vehicle frequency; "key fill" (keybind) copies a crew member's encryption and channels | Per-vehicle config (`tf_hasLRradio`, radio type); the LR radio a player gets depends on seat (driver / gunner / commander / copilot settings) | "Racks" (`AcreRacks`): named mounts per vehicle with a mounted radio (e.g. PRC-117F in a VRC-103), `allowedPositions` per rack, mount/unmount own radios, racks wired to intercoms |
| Speaker | Optional loudspeaker, off by default, crew toggles; heard within 15 m around the vehicle | — | Racks "hearable" from outside (`isRackHearable`) |
| Intercom | One crew intercom; crew hear each other at any distance between seats; passengers optional | Intercom channels (ACE self-action, first 20 reserved); `tf_isolatedAmount` muffles voices between inside and outside unless turned out | Several intercoms per vehicle (crew, passenger), `allowedPositions` / `limitedPositions` / `masterPositions`, broadcast, PTT or voice activation per station, per-station volume, GUI |
| External | Manpack: a backpack radio whose handset someone beside the carrier can grab (cord length) and whose speaker the carrier can switch on | External intercom: phone on the vehicle exterior, and wireless intercom with suitable headgear | Infantry phone on the vehicle exterior (ringing, speaker) |

Fit for LC (wishlist "vehicle intercom component"): a component like CVON's (radio for crew seats, optional
speaker, one crew intercom) is the natural first step on vanilla's `BaseRadioComponent`; ACRE's multi-intercom
stations and racks are the full version. LC already muffles voices through vehicle hulls (`LC_Occlusion` mount
types), which covers TFAR's isolation.
