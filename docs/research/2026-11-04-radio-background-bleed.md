# Radio background bleed: battlefield sound heard behind a radio transmission

Date: 2026-11-04. Engine 1.8.0.13. Lima Charlie 2.0.6 code (bugfix-2.0.6). Research only: no code changed.

**Status: shelved by Duke (2026-11-04).** Not a focus, and there is no good source for the sound samples
option A needs. Kept for reference; reopen only if a sample source turns up or option B becomes attractive.

The idea: when someone talks on the radio, you hear what is going on around them (their own rifle,
an MG next to them, an explosion, the engine of the vehicle they are in), carried through the radio
with the same band, garble and cut-off as their voice.

## 1. Prior art

| Source | What it is | Credibility |
|---|---|---|
| TFAR issue #1440, "option to include ambient sounds when transmitting" (github.com/michail-nikolaev/task-force-arma-3-radio/issues/1440, 2018) | The feature request. TFAR devs (Dorbedo, dedmen) turned it down as impossible. The workaround suggested in the thread: give Arma the list of units being heard on radio, watch `FiredNear` on them, play a "gunshot through radio" sample. Nobody built it there. | Primary (maintainers) |
| SH Battle Radio, by K1REL (Reforger Workshop 6A4539AB94F37633) | Vanilla-VON mod. From search-engine copies of its description: "environmental background noise", sender's surroundings (combat, weather, vehicles), static on weak signal, vanilla radio left working when the sender is out of the receiver's replication area. **The page is gone (404, 2026-11-04) and there is no Wayback snapshot**, so the mechanism can't be read. | Second-hand snippet |
| Bent-Rad Living Warzone (EFF33C504B897941, 1.1.5, APL) | Distant war ambience and AI chatter. Not tied to a sender. Not relevant to the mechanism. | Workshop page |

**Not found:** the Arma 3 TFAR/ACRE add-on Duke remembers. Searched (2026-11-04): Steam Workshop, BI
forums, GitHub repos and code (`FiredNear` + TFAR/ACRE event APIs), the TFAR and ACRE2 trees in
`References/GithubRepos`. Neither TFAR nor ACRE2 core has it. **Name or link needed from Duke** to read
how it does it (see §5).

Why the TFAR devs said no (applies to us too): TeamSpeak carries **one** voice stream per client, and
it carries no game sound. Mixing the sender's game audio into their mic (loopback capture) would make
everyone near them in direct speech hear the battle twice, and would feed back their own received
radios, UI sounds and music. So the sound has to be **made up at the receiver** from information the
sender sends. That is the only design worth building.

## 2. What Lima Charlie already gives us

- **Radio DSP is per speaker, in the plugin.** `lc_audio_process` (plugin/src/lc_audio.c:386) runs per
  TeamSpeak client on their incoming voice; `radio_tick` applies the band-pass, drive, sample-and-hold,
  bit-crush, dropouts and noise, then the headset and the radio limiter. Anything added to `x` before
  `radio_tick`, on the radio path only, is coloured exactly like the voice, follows the radio's ear and
  channel volume, and **stops when the voice stops** (TeamSpeak only calls this while it is delivering
  that client's audio). Direct-speech listeners are untouched: they hear the real game sound already.
- **Plugin-to-plugin messaging exists.** Transmissions are announced with `sendPluginCommand` to the
  current channel (`send_radio_announcement`, plugin/src/lc_core.c:398). Events can ride the same route,
  sender to receiver, without touching the game server and without needing the sender to be in the
  receiver's replication bubble.
- **Sample playback exists** (`lc_sounds.c`: WAV decode, cache, mixing). Today it plays beeps to the
  listener; a per-speaker "bleed voice" is new but small.

## 3. What Reforger lets the sender's game see

All found in vanilla 1.8.0.13 source; runtime behaviour untested (U) unless noted.

- **Shots.** `SCR_MuzzleEffectComponent.GetOnWeaponFired()` (GameCode/Effects/SCR_MuzzleEffectComponent.c:13)
  and the `"OnProjectileShot"` handler on a character's `EventHandlerManagerComponent` (used by
  ScenarioFramework and the tutorial). The muzzle effect runs where the shot is shown, i.e. on every
  client that has the shooter streamed in (U), so the sender's client sees shots near them, not only
  their own. Suppressed or not: `BaseWeaponStatsManagerComponent.GetShotSound` (Wiki/game/audio-mixing.md).
- **Explosions.** `BaseProjectileEffect.OnEffect` (`SCR_ExplosionAmmoEffect`), `SCR_ExplosiveChargeComponent`.
  Where they run on a remote client is (U).
- **Vehicles.** `Vehicle*Simulation.EngineIsOn()`, helicopter `RotorGetRPM`; the sender's own
  compartment says which vehicle they are in.
- **Mixer loudness at the sender's ears (best find).** `Sounds/_SharedData/Variables/GlobalVariables.conf`
  defines global audio variables with `source Amplitude`: `Submix_L1`..`Submix_L4` (L3 vehicles, L4
  shots/explosions/impacts), `Explosions_SubMix`, `Explosions_Acp`, `Vehicles_Impacts`, plus `GInterior`,
  `GVehicleInterior`, `GIsThirdPersonCam`. Script reads them with
  `AudioSystem.GetVariableValue(name, "{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf")`
  (vanilla does this for `GInterior`, `GRoomSize`). These already include distance, suppressors,
  occlusion and interiors. Whether the **Amplitude** ones return live values to script is (U): one
  Workbench print while firing settles it. Caveat: the listener is the camera, so third person and
  the GM camera skew it (`GIsThirdPersonCam` tells us).

## 4. Design options

**A. Plugin-side bleed from sender events (recommended).**
1. Sender's game, only while they hold a radio key: collects shots (type: pistol/rifle/MG/cannon,
   suppressed, distance), explosions (size, distance) and their vehicle state (engine on, type, RPM),
   gated by a loudness threshold so only what a real handset mic would pick up gets through; optionally
   scaled by `Submix_L4`/`L3` if those read live.
2. Written into `game_state.json` (new optional block; old plugins ignore it, no protocol bump needed if
   it stays additive, same rule as the hearing hook).
3. Sender's plugin batches the events into a short command (e.g. every 100-200 ms, only when there is
   something) alongside the transmission announcement.
4. Receiver's plugin schedules each event as a sample on that speaker's bleed voice, level from distance
   and size, a fixed delay to sit behind the voice's jitter buffer, and adds it to `x` before
   `radio_tick`. Vehicle beds loop while the announcement says "in vehicle". The radio limiter squashes
   loud bangs the way a real radio's AGC does.
- Needs: a small sample set the plugin can ship (our own recordings or CC0; **not** extracted vanilla
  sounds, which can't go into a TeamSpeak plugin; TFAR/ACRE sounds don't cover gunshots). Roughly:
  3 small-arms classes x near/far, suppressed, MG, cannon, 2-3 explosion sizes, car/truck/APC/helo loops.
- Pros: exact radio chain, ends with the transmission, works at any distance, game server untouched,
  fits the TFAR-style trust model. Cons: generic samples, not each modded weapon's own sound.

**B. Game-side playback at the receiver.** Receiver's game plays the real vanilla/modded weapon sound
event in 2D through a band-passed `.acp`, when its plugin reports "X is heard on radio" plus X's events.
Pros: real weapon sounds. Cons: separate chain from the voice (no garble, terrain, dropouts, ear
routing), timing via two extra file hops, start/stop drift against the voice, `.acp` authoring work.
Only worth it if A's samples sound too generic.

**C. Envelope only.** Sender streams `Submix_L4`/`L3` levels; receiver plugin shapes band-passed noise
bursts with them. Cheapest content, but 20 Hz sampling cannot resolve single shots at MG rates, and it
sounds like hiss, not gunfire. Useful as the loudness input to A, not on its own.

**Rejected: real audio capture** (loopback of the sender's game mixed into the mic): double-hearing in
direct speech, feedback of received radios, music and UI; the TFAR devs' reason.

## 5. Unknowns and cheapest oracles

| Unknown | Oracle |
|---|---|
| Do `Submix_L4`/`L3`/`Explosions_SubMix` return live values to script? | Workbench play, print `GetVariableValue` at 10 Hz while firing and driving (one `.c`, ClaudeDevTools) |
| Does `GetOnWeaponFired` fire on a remote client for another player's/AI's shot? | Same test with an AI firing nearby; then 2-client MP |
| Is TeamSpeak delivering voice frames continuously while the radio key is held (silent gaps)? | TS log counter in `lc_audio_process` during a held, silent transmission |
| Does a 5-10 Hz `sendPluginCommand` stream trip the server's anti-flood? | 2-client test with the batch rate raised; TS server log |
| How the Arma 3 mod Duke remembers does it | Its name or link from Duke |
| Realism dial (handset mics and noise-cancelling headsets suppress a lot) | Duke's ear test on a reference render (render the plugin chain offline first, as with the radio tuning) |

## 6. Fit and size

A 3.x feature on its own branch, after 3.0 (vehicle radios and intercoms there decide what "in a
vehicle" means). Server settings: on/off, per category (shots, explosions, vehicles), level. Rough size:
game side ~300 lines Enforce (collector + writer block), plugin ~400 lines C (command, scheduler, bleed
voice, tests) plus the sample set, which is the long pole.
