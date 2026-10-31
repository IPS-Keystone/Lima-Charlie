# Coalition VON vehicle radio / intercom — approach survey 2026-10-31

- **Date:** 2026-10-31 (read-only source survey for the 3.0 comms branch, workstream B + shared foundation)
- **Source:** `References/GithubRepos/Coalition-VON`, commit `9517a39` (2026-10-20, "Ears"; CVON 3.0.1).
  Paths below are relative to `CoalitionVON/` (Enfusion addon) or `CoalitionTeamspeakPlugin/` (TS plugin).
- **Licence:** **none.** No LICENSE/COPYING file in the checkout, nothing in `readme.md`, and the GitHub API
  reports `license: null` for `CoalitionArma/Coalition-VON` (public repo). Default copyright applies: all rights
  reserved. This survey describes mechanisms only (GROUND-RULES 7). LC must not reuse their code.
- Builds on `2026-10-28-terrain-autoconnect-vehicle-comms.md` §3 (feature comparison CVON/TFAR/ACRE2). That
  survey is not repeated here.

Short names: `VRC` = `scripts/Game/Components/CVON_VehicleRadioComponent.c`, `RC` = `.../CVON_RadioComponent.c`,
`PC` = `scripts/Game/Vanilla Overrides/CVON_SCR_PlayerController.c`, `VC` = `.../CVON_SCR_VONController.c`,
`GM` = `scripts/Game/Gamemode/CVON_VONGamemodeComponent.c`, `PL` = `CoalitionTeamspeakPlugin/src/plugin.c`.

## 1. The vehicle radio

**Definition.** `CVON_VehicleRadioComponent` subclasses CVON's own item-radio script component
(`CVON_RadioComponent : ScriptComponent`, RC:11). It is *not* a vanilla `BaseRadioComponent`, so the vehicle
gets no engine transceiver (VRC:1-17). Switches on the component: has radio / intercom / speaker, passengers
on intercom, military only, start frequency (VRC:19-35). Range, radio name, icon/dial textures and which radio
UI to open come from the base class attributes (RC:14-30).

**Prefabs.** Five vanilla base prefabs are overridden at their vanilla path and GUID, each adding one
component block (fresh block ID) with range 15 000 m, speaker range 25 m, the PRC-117 UI:
`Prefabs/Vehicles/Core/{TrackedVehicle_Base, Wheeled_APC_Base, Helicopter_Base, Wheeled_Car_Base,
Wheeled_Truck_Base}.et` (e.g. `Wheeled_Car_Base.et:4-14`; `.meta` names `{1F9BB80C3EC3C891}`). Car and truck
set the military-only flag. These are delta overrides (layered by addon load order per
`Wiki/engine/config-and-prefab-layering.md`), so every vanilla or modded vehicle inheriting those bases gets it.

**Military-only.** Decided once, lazily, from the vehicle's *default* faction
(`FactionAffiliationComponent.GetDefaultAffiliatedFaction()` → `SCR_Faction.IsMilitary()`), postponed while
there is no faction manager so a too-early answer isn't cached (VRC:60-81).

**Initialisation (authority).** `OnPostInit` arms one frame; on that frame the server registers the radio in the
gamemode's speaker list and gives it the vehicle's faction key/channels (VRC:116-154). Starting frequency
order: per-vehicle attribute → faction callsign info's vehicle frequency → frequency config's → first long-range
channel → channel 1, inserted into the channel list if missing (VRC:203-258).

**"Key fill".** A vehicle keeps its prefab faction's encryption (vanilla vehicles are `US`/`USSR`, players often
another key) until a crew member presses a keybind (VC:269-290 → PC:304-320, server validates the caller
operates that radio). The server then copies that crew member's own radio channel list (long-range preferred)
or their faction's channels, and retunes to the start frequency (VRC:163-191, 272-299). A keyless vehicle is
filled automatically by the first crew member (PC:292-295).

**Which seats.** `CVON_VehicleComms.IsCrewSeat` (VRC:343-361): compartment type `PILOT` or `TURRET`; or a cargo
seat that has an attached turret / auto-connect turret (vanilla LAV-25 commander pattern, commit `f2e1460`);
or a seat whose `UIInfo` name contains "commander" (string match). Everything else is passenger.

**Listing to the player.** CVON does not use the vanilla radial for it. Each client keeps its own radio list on
the player controller (`m_aRadios`); the vehicle radio is appended *after* inventory radios and a held handset
(PC:164-184, 270-300) and is reached by CVON's three radio keys (index 0/1/2 = SR/LR/LR2, VC:2-9, 424-470) or by
"rotate active radio", which cycles the whole list (VC:107-123). The CVON VON radial filters out anything that
is not a CVON radio entry (`CVON_SCR_VONMenu.c:71-120`), and the vehicle radio has no vanilla entry, so it is
not in the radial at all. A hint announces "Vehicle radio connected" once per vehicle (VC:998-1030). The
server's copy of `m_aRadios` deliberately leaves the vehicle radio out (PC:154-161).

**Transmission positioning.** None specific to vehicles. The server takes the *talker's character origin* at key
press as sender position (GM:262-267); receive quality uses the *listener's character origin* (VC:809-820,
1825-1831). Seated crew are at the vehicle anyway, so no antenna offset exists. Range comes from the
transmitting radio's `m_iRadioRange` looked up by its RplId (GM:242-256). The vehicle radio's RplId is the
vehicle's own `RplComponent` (RC:133-136).

**Replication / arbitration.** No lease, no owner, no exclusivity. Shared state lives as `RplProp`s on the vehicle
component (channel, frequency, time deviation, encryption key, channel list, power, speaker toggle; RC:33-75).
Clients change it through generic player-controller RPCs keyed by radio RplId (PC:591-706). Access is
*derived* each time from the caller's current seat (`GetOperatedRadio`, VRC:372-382) at the points that check it:
key fill (PC:317) and speaker toggle (PC:341). Server routing also derives it per transmission:
`GetRadioEntities`/`GetRadios` add the operated vehicle radio to whoever is in a crew seat (GM:712-721,
743-745). Any number of crew can transmit on it at once.

**Sharing settings between crew.** When any `RplProp` changes, each client's replication callback checks
"is this one of mine?" (for a vehicle radio: am I in a crew seat of it, VRC:51-57) and rewrites its own
`RadioData.json` and resends its frequency list to the server's frequency index (RC:115-129, 621-722;
PC:738-754). Volume and ear (`m_iVolume`, `m_eStereo`, RC:64-67) are plain fields, so each crew member has
their own. The respawn settings snapshot skips vehicle radios (PC:134-136).

## 2. Intercom

- **Channels:** one per vehicle, no channel selection, no UI entry. Always on: any direct speech by a member is
  intercom (voice-activated, "hot mic").
- **Membership:** `GetIntercomVehicle` (VRC:385-398): you are on the vehicle's intercom if you sit in a crew seat,
  or in any seat when the passengers flag is set. Decided **entirely on each listener's client**, 4 times a second
  in the membership pass (VC:603, 824-955): the listener's own intercom vehicle is computed once, then each
  talker's, and a talker matching it is flagged and kept in the list regardless of distance (VC:918-951). Nothing
  is replicated or arbitrated for the intercom.
- **Precedence:** when building the bridge output, intercom beats radio, speaker and direct for that talker
  (VC:1795-1802), so a crew member's radio call is heard by the rest of the crew once, as intercom. Same-vehicle
  occupants (intercom or not) also skip the hull muffle (VC:1463-1503).
- **Over the bridge:** per talker entry in `VONData.json`/`.bin`, `VONType = 2` (INTERCOM) with both ear gains
  fixed at 1.0 (VC:1089, 1799-1801). No frequency, no channel id. Readme table: readme.md "VONType" section.
- **Plugin:** `VON_INTERCOM` entries (PL:418) get a mono path, identical in both ears: 300 Hz-3.4 kHz band-pass
  (two biquad stages per side), tanh saturation, +6 dB make-up, soft limit, gain ramp; no distance, muffle or static
  (PL:25-27, 164-168, 954-955, 1362-1418, 3305-3307). Plugins older than 3.0 treat type 2 as direct.

## 3. Seat entry / exit / type detection

- **No events.** CVON never subscribes to vanilla compartment events (vanilla has e.g.
  `SCR_CompartmentAccessComponent.GetOnCompartmentEntered`, vanilla `Game/Vehicle/SCR_CompartmentAccessComponent.c:30`).
  Everything is polled or derived on demand.
- **Client:** the 4 Hz membership pass calls `RefreshVehicleComms`, which re-derives the operated radio and
  adds/removes it from the list on change (VC:832-834, 998-1005). Also run on (re)possession via
  `InitializeRadios` (PC:151-162).
- **Server:** re-derived at each RPC that needs it and at each transmission's routing (see §1).
- **APIs:** `CompartmentAccessComponent.GetCompartment()` for the local/authoritative character; for proxies of
  other players a fallback walks `GetRootParent()` → `SCR_BaseCompartmentManagerComponent.GetCompartments()` and
  matches `GetOccupant()` (VRC:308-338; their comment says the direct call isn't reliable for remote characters).
  Seat type: `BaseCompartmentSlot.GetType()` (`ECompartmentType`), `GetAttachedTurret()`, `GetAutoConnectTurret()`,
  `GetUIInfo().GetName()`; vehicle from `GetVehicle()`. A modded `Vehicle` caches its compartment manager in
  `EOnInit` (`CVON_SCR_Vehicle.c:1-10`) for the same-vehicle muffle check.

## 4. Other features (one line each)

- **Infantry phone:** none.
- **Vehicle speaker:** yes. Off by default (commit `03e67c0`). Crew toggle by keybind; server-validated (PC:322-349).
  Server, per transmission, finds each player's *nearest* active speaker on the same frequency/time/key, within
  speaker range + 10 m, skipping players already receiving it and the transmitting radio itself, and sends them a
  SPEAKER entry by owner RPC (GM:334-406, PC:523-555). Plays from vehicle origin + 1.5 m local up (VRC:109-113),
  with signal quality from vehicle-to-sender distance (VC:1832-1836). Client keeps whichever is louder, speaker or
  the sender's direct voice (VC:1803-1819). Plugin: radio chain then the positioned direct path (PL:29-31, 1420+).
- **Manpack / backpack radio:** a long-range CVON item radio whose carrier wears a backpack with any radio component
  (`CVON_Manpack.c:15-54`; a gamemode flag drops the backpack requirement). Speaker range 15 m from the carrier.
- **Borrowed handset:** yes, the only real lease in CVON. Server RPC with distance check (3 m cord), one holder per radio,
  one handset per player, holder replicated as an `RplId` `RplProp`, server polls every 0.5 s and releases on cord
  distance, holder death, or the radio stopping being a manpack (PC:222-268; RC:287-383; `CVON_Manpack.c:11`). Holder's
  client adds/removes the radio from its list on the replication callback (PC:188-218). Owner keeps using it too.
- **Relays / retransmission:** none. (There is "listen to another player's radios", a spectator/Zeus feature,
  GM:646-665, RC:668-700; not a relay.)
- **Jamming:** none. **SATCOM:** none. (grep for jam/relay/satcom/repeater finds nothing.)

## 5. Pitfalls and bugs visible in their approach

1. **Prefab coverage by base override.** Only five base prefabs; anything else (vehicles from mods with their own
   bases, static weapons, boats/aircraft if added) has no comms. Conversely every inheriting vehicle, including modded
   ones, silently gets a 15 km radio and 25 m speaker; per-vehicle tuning needs a further override. Two comms mods
   overriding the same bases would both add components (fresh IDs), so CVON and LC must never be loaded together.
2. **Seat classification is heuristic.** The "commander" substring test on the seat's UI name is localisation-dependent
   (a `#AR-…` key or translated text may not contain it); the turret-link rule was added late (`f2e1460`, 2026-10-12).
3. **Unvalidated tuning RPCs.** Channel, frequency, time deviation, power and add-channel RPCs (PC:591-706) only check
   the RplId resolves. Any client can retune or power off any radio, vehicle radios included, from anywhere. Only key
   fill and speaker toggle check the seat. The transmit RPC (PC:489-493, GM:236-331) trusts the client's container
   (frequency/key) and doesn't check the sender may use the radio id. That's fine under "client-trusted audio" for honest
   clients, but it is not an access model.
4. **Radio list order is fragile.** The vehicle radio stays "last" only by convention. Rotate-active reorders the whole
   client list (VC:107-123) while the server's list (which excludes the vehicle radio) is addressed by index for
   volume/ear pushes (PC:708-732), so the indices can drift. With three inventory radios the vehicle radio is on no key
   until rotated.
5. **Not in the radial.** The CVON radial lists only vanilla radio entries that carry a CVON component, so the vehicle
   radio cannot be picked there. CVON players reach it only by keys.
6. **4 Hz polling** for seat changes: up to 250 ms where a just-seated player isn't on intercom or has no vehicle
   radio. Proxy seat lookup needs the fallback scan because the access component's answer isn't trustworthy for remote
   characters.
7. **One voice per talker** (a TS plugin processes each client's stream once): intercom swallows the crew member's radio
   call, and speaker vs direct is a louder-wins choice (VC:1795-1819). No mixing.
8. **Transmit position = talker's character origin at key press**, fixed for the whole transmission unless the sender is
   streamed (VC:1827-1830). It's harmless for crew but means no antenna concept: a manpack on the ground or a borrowed
   handset transmits from the holder.
9. **Minor:** the speaker-toggle hint guesses the new state before replication (VC:259-263). Speaker registration
   runs on one authority frame and is skipped if the gamemode isn't ready (VRC:126-137); toggling the speaker
   re-registers it (RC:275-283). Military check uses the *default* faction, while vanilla's
   `SCR_VehicleFactionAffiliationComponent` re-affiliates vehicles with their occupants.
10. **History:** the whole vehicle/manpack feature landed between 2026-10-12 and 2026-10-19 (`a79e6a1` "3.0 test",
    `7f82d98`, `e823c49` "Manpack fixes" rewrote ~900 lines, `f2e1460`, `03e67c0`); `ff5d597` fixed vanilla radios
    leaking into their radial. It's young code with no tests and no issue references in commit messages.

## What LC should do the same / differently

**Same (approach only, our own code):**
- Derive vehicle-radio access from the seat on the server at every point that matters (RPC handlers, routing), with no
  persistent per-player grant. That's cheap and survives disconnects, death and seat swaps for free.
- Shared radio state (frequency, key/channels, power, speaker) replicated on the vehicle; per-listener volume/ear local.
- Each crew client reacts to the replicated change by rewriting its own bridge state, so a retune by one crew member
  reaches all.
- Vehicle radio starts on a configurable net with a faction fallback; some explicit "fill with my key/channels" step
  for vehicles whose prefab faction isn't the players'.
- Intercom membership decided on the listener's client from seat data (no network cost); a distinct bridge type and a
  distinct plugin DSP (band-limited, centred, no occlusion/distance).
- Seat classification that treats a cargo seat linked to a turret as a commander seat.

**Differently:**
- **Validate every radio-state RPC** (tune, power, channel list) against the foundation-2 access rule (held, seated crew,
  handset holder), not only key fill and speaker.
- **Antenna position:** send the radio's position (vehicle antenna point) in `RTX`, not the talker's head, per foundation 1.
  Key terrain links by radio.
- **Seat config on the component** (explicit allowed compartment types / slot indices per prefab, TFAR/ACRE style) rather
  than a localised-name heuristic. Keep the turret-link rule as the default.
- **Listing:** put the vehicle radio (and intercom entry) into the *vanilla* `SCR_VONController` entries / radial that LC
  already uses, so it is tunable from the radial. Don't keep a parallel radio list with index conventions. Still to verify
  on Duke's PC: runtime add/remove of entries and radial redraw.
- **Event-driven seat changes** (`SCR_CompartmentAccessComponent` entered/left invokers) with a slow poll as a safety
  net, instead of a 4 Hz poll only.
- **Intercom channels** crew/cargo/misc with a selectable channel per occupant (design §B), and an infantry phone
  lease. CVON has neither.
- **Coverage:** prefer adding the component without overriding vanilla base prefabs where the layering allows (design §B).
  If base overrides are unavoidable, also provide per-vehicle overrides with sane ranges, and document that LC and
  CVON are mutually exclusive.
- **Mixing:** decide explicitly whether a crew member on intercom who also transmits on radio is heard twice (intercom +
  radio). CVON collapses it to one path. LC's plugin path split (direct / radio / intercom) can choose.
- **Military check:** consider current affiliation (or a component flag) rather than default faction only. Civilian
  vehicles can still have no comms via a flag.
