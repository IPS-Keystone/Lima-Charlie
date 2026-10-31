# Vehicle prefabs, compartments, action points — survey 2026-10-31

- **Date:** 2026-10-31 (read-only source survey for the 3.0 comms branch, workstream B; one paragraph for C)
- **Game version:** Arma Reforger 1.8 vanilla extract, `References/ArmaReforger/vanilla` (paths below are relative
  to it unless stated). Scripts under `scripts/Game` and `scripts/GameCode`; generated (native) API under
  `scripts/Game/generated`, `scripts/Core/generated`.
- **Searched:** every `.et` under `Prefabs/Vehicles` for `Radio|Transceiver`; vehicle base inheritance (first line
  of every vehicle `.et`); compartment classes and every script subscriber of the compartment events; action contexts
  on BTR70 / LAV25 / BRDM2 and `Vehicle_Base`; antenna / relay props; mod overrides of vanilla prefabs in
  `References/ARMods` and `References/GithubRepos/Coalition-VON`. Wiki: `GOTCHAS.md`,
  `engine/config-and-prefab-layering.md`, `enforce-script/modded-classes-and-attributes.md`,
  `game/damage-medical-actions.md`.
- **Related:** `2026-10-31-cvon-vehicle-radio-approach.md` (what Coalition VON does; approach only),
  `2026-10-28-terrain-autoconnect-vehicle-comms.md` §3.
- **Tags:** (V) verified in source here; (I) inference, not verified in game.

## 1. Vanilla vehicle radios

(V) Seven vehicle families carry a native `BaseRadioComponent` (no `SCR_RadioComponent`, no `SCR_GadgetComponent`,
no `RadioTransceiver`). None of the APCs or helicopters has one: BTR70, BRDM2, LAV25, M998/M1025 HMMWV, S105,
S1203, UH-1H, Mi-8 — grep of all `Prefabs/Vehicles/**/*.et` for `Radio|Transceiver` returns nothing for them.
No `Core/*_Base.et` has a radio either.

| Prefab | Block | Transceivers |
|---|---|---|
| `Prefabs/Vehicles/Wheeled/M151A2/M151A2.et:217` | `BaseRadioComponent "{51EAE034EF047EFA}"`, `"Turned on" 1` | `IntercomTransceiver`, freq resolution 1500 (:219-221) |
| `Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_base.et:263` | `{5AD4F813E67ECBF1}`, on | `IntercomTransceiver` (:265-267) |
| `Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_base.et:309` | `{55A093A30AC3BC4F}`, on | `IntercomTransceiver` (:311-313) |
| `Prefabs/Vehicles/Wheeled/M923A1/M923A1.et:166` | `{51EAE034CD7B7A2C}`, on | none |
| `Prefabs/Vehicles/Wheeled/M923A1/M923A1_command.et:11` | same block | `RelayTransceiver`, `"Transmitting Range" 2000` (:12-16) |
| `Prefabs/Vehicles/Wheeled/Ural4320/Ural4320.et:278` | `{51EAE036CCD0A08E}`, on | none |
| `Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_command.et:11` and `Ural4320_repair.et:4` | same block | `RelayTransceiver` 2000 m |

- (V) `IntercomTransceiver` and `RelayTransceiver` are empty native subclasses of `BaseTransceiver`
  (`scripts/Game/generated/Radio/IntercomTransceiver.c:12`, `RelayTransceiver.c:12`). No vanilla script references
  `IntercomTransceiver` at all (grep `scripts/` → only the generated file). `RelayTransceiver` is read by Conflict
  coverage: `scripts/Game/Components/Locations/SCR_CampaignMilitaryBaseComponent.c:1133-1150`
  (`GetRelayRadioRange`) — the command trucks are mobile HQ relays, not player radios.
- (V) Command trucks also get the `Radio` slot → `M923A1_combox.et` (`M923A1_command.et:69-76`), a
  `Vehicle_Command_Base` part carrying the `SCR_RadioVehicleSpawnPoint` (`VehParts/M923A1_combox_spawn.et:4`);
  no radio component in the combox itself.
- (V) Vanilla VON never lists vehicle radios. Entries are only added by `SCR_GadgetManagerComponent`
  (`scripts/Game/Components/Gadgets/SCR_GadgetManagerComponent.c:590-597`, one `SCR_VONEntryRadio` per transceiver of
  a *gadget* radio, `SetRadioEntry(transceiver, i + 1, gadgetComp)`) and the editor (`Editor/Entities/SCR_EditorManagerEntity.c:182,192`).
- (I) What the engine does with `IntercomTransceiver` (if anything) is unknown; nothing in script consumes it.

## 2. Base vehicle prefab hierarchy and how to attach to all vehicles of a class

(V) Hierarchy (`Prefabs/Vehicles/Core`, first line of each file):

```
Vehicle_Base.et {4085446E2B406849}               (all vehicles)
├─ Wheeled_Base.et {62F416029692CE40}
│  ├─ Wheeled_Car_Base.et {1F9BB80C3EC3C891}      M151A2, Humr_base (M998/M1025/M997), UAZ469, UAZ452, S105, S1203
│  ├─ Wheeled_Truck_Base.et {E03D5609EEA6E03D}    M923A1, Ural4320
│  └─ Wheeled_APC_Base.et {543A50BF3A42AD47}      BTR70_Base, BRDM2_base, LAV25_base
├─ TrackedVehicle_Base.et                        no vanilla children in 1.8 (grep for "Core/TrackedVehicle_Base.et" = 0);
│                                                used by mods (RHS and CVON both override it)
└─ Helicopter_Base.et {CBD8C2393BE87581}          Mi8_base, UH1H_base
```

Component block IDs shared down the whole tree (V): `SCR_BaseCompartmentManagerComponent "{20FB66C5DCB8DF72}"`
(`Vehicle_Base.et:36`), `ActionsManagerComponent "{C97BE5489221AE18}"` (`Vehicle_Base.et:162`, same ID in
`BTR70_Base.et:1712`), `EventHandlerManagerComponent "{52CAB06F9C274D14}"` (`:4`), `RplComponent` (`:183`).

Ways to attach LC state to every vehicle of a class:

1. **Delta override of the base `.et` in the LC addon** (same path, `.meta` naming the vanilla GUID, file holding
   only the new component block). (V) This is exactly what Coalition VON does for the five bases
   (`GithubRepos/Coalition-VON/CoalitionVON/Prefabs/Vehicles/Core/Wheeled_APC_Base.et` — 14 lines, one
   `CVON_VehicleRadioComponent` block; `.meta` `Name "{543A50BF3A42AD47}Prefabs/Vehicles/Core/Wheeled_APC_Base.et"`)
   and RHS does for `Vehicle_Base.et` / `TrackedVehicle_Base.et` (`ARMods/from-WeaponModels/RHS-StatusQuo_*/Prefabs/Vehicles/Core/Vehicle_Base.et`,
   7 lines, one `RHS_RadioSourceComponent`). Inherited by modded vehicles that derive from vanilla bases.
   - Wiki guidance: `engine/config-and-prefab-layering.md` "Prefab component blocks": a block whose ID matches the
     inherited chain overrides it; a **fresh ID adds a second block** (fine when no inherited component of that type
     exists, a bug when one does). `enforce-script/modded-classes-and-attributes.md:21`: "Prefer modding a
     component over overriding a prefab: two mods can both mod a class; only one wins a prefab override."
   - (I, contradicts the wiki line) Delta overrides appear to **layer** rather than one-wins: 11 mods in
     `References/ARMods` override `Prefabs/Characters/Core/Character_Base.et` (ACE Core, Medical Core/Breathing/
     Circulation, Carrying, Captives, Backblast, Defib, CVON, RHS, NV-System) and the ACE set is routinely run
     together. Not verified in game; worth one test (LC + CVON both on `Wheeled_APC_Base`, check both components
     exist). Either way, LC adding a *new* component with a fresh ID cannot clobber another mod's fields.
   - Hazard (V): M151A2, UAZ469, UAZ452, M923A1, Ural4320 already have a `BaseRadioComponent`. A base override
     adding another `BaseRadioComponent` gives those vehicles **two**; `FindComponent(BaseRadioComponent)` returns
     one of them, and Conflict's relay lookup iterates transceivers of the one it finds. Use an LC-specific class
     (e.g. `LC_VehicleRadioComponent : ScriptComponent` or a subclass) so `FindComponent` of the vanilla type stays
     unambiguous.
2. **Modded class of a component already on every vehicle** (no prefab override at all). Candidates on
   `Vehicle_Base`: `SCR_BaseCompartmentManagerComponent` (script class, `scripts/GameCode/Vehicle/SCR_BaseCompartmentManagerComponent.c:13`),
   `SCR_VehicleFactionAffiliationComponent`, `SCR_VehicleDamageManagerComponent`. Lowest collision risk (wiki
   preference) but per-vehicle configuration (range, which seats, antenna offset) then has to come from a `.conf`
   keyed by prefab, not from prefab attributes. (I) `SCR_BaseCompartmentManagerComponent` also exists on turret
   and cargo child entities, so gate on "owner is the root vehicle".
3. **Adding a component at runtime** — not possible. (V) The only `CreateComponent` is Workbench editor API
   (`scripts/Core/generated/WorkbenchAPI/WorldEditorAPI.c:66`); nothing on `IEntity`. Runtime alternative is a
   helper *entity* spawned and `AddChild`ed to the vehicle (`scripts/Core/generated/Entities/IEntity.c:499`,
   `AddChild(child, TNodeId pivot, flags)`) — (I) needs its own `RplComponent` and hierarchy replication; heavier.
4. **Per-model overrides** (`BTR70_Base.et` etc.) — needed only where a position must be authored (section 4).

## 3. Compartment / seat API

Classes (V):
- `ECompartmentType { PILOT, TURRET, CARGO }` — only three (`scripts/Game/generated/ECompartmentType.c`).
- `BaseCompartmentSlot : ExtBaseCompartmentSlot` (`scripts/GameCode/Vehicle/BaseCompartmentSlot.c:1`); natives in
  `scripts/Game/generated/Vehicle/ExtBaseCompartmentSlot.c`: `GetType()` :20, `GetOccupant()` :27, `IsPiloting()` :15,
  `GetCompartmentName()` :44, `GetCompartmentUniqueName()` :71, `GetManager()` :21, `GetOwner()` :18,
  `GetPosition()` :66, `GetAttachedTurret()` :74. Subclasses `PilotCompartmentSlot`, `TurretCompartmentSlot`,
  `CargoCompartmentSlot` (+ `SCR_PatientCompartmentSlot`, `SCR_DoctorCompartmentSlot` : cargo).
- `BaseCompartmentSlot.GetVehicle()` / `GetVehicle(out int compartmentID)` (`BaseCompartmentSlot.c:83-127`) walks up
  parents to the **topmost** entity with a `BaseCompartmentManagerComponent`: the slot's owner may be a turret or cargo
  frame child.
- `CompartmentAccessComponent` (character, `scripts/Game/generated/Components/CompartmentAccessComponent.c`):
  `GetCompartment()` :20, `IsInCompartment()` :25, `static GetVehicleIn(IEntity)` :79 (root of hierarchy),
  `IsGettingIn/Out()` :64/66, `IsSwitchingSeatsAnim()` :72. `ChimeraCharacter.GetCompartmentAccessComponent()` and
  `IsInVehicle()` (`generated/Character/ChimeraCharacter.c:22,30`). Wiki already notes occupancy is replicated and
  `GetCompartment()` works on clients (`game/damage-medical-actions.md:141`).
- `BaseCompartmentManagerComponent` (`generated/Vehicle/BaseCompartmentManagerComponent.c`): `GetCompartments(out)` :26,
  `FindCompartment(slotID, mgrId)` :24, `FindCompartmentByName` :22. Script layer `SCR_BaseCompartmentManagerComponent`:
  `GetOccupants(inout)` :161, `GetOccupantsOfType` :177, `GetCompartmentsOfType` :45.

Recipes:
- **Which vehicle:** `CompartmentAccessComponent.GetVehicleIn(character)` or
  `character.GetCompartmentAccessComponent().GetCompartment().GetVehicle()`.
- **Seat type:** `slot.GetType()`. Driver/pilot = `PILOT`; note (V) UH-1H `CopilotCompartment` is also a
  `PilotCompartmentSlot` (`Helicopters/UH1H/UH1H_base.et:464`) and so is Mi-8 `Passenger_m01Cargo`
  (`Helicopters/Mi8MT/Mi8_base.et:421`); `IsPiloting()` (I) separates the one actually flying.
- **Commander vs gunner:** both are `TURRET`, each on its own turret child entity, all slots named
  `TurretCompartment`. (V) Vanilla marks commanders with `CompartmentUniqueName "CommanderTurret"`:
  `BTR70/VehParts/Turret/BTR70_turret_commander.et:48`, `BRDM2/VehParts/Turrets/BRDM2_turret_commander.et:83`,
  `LAV25/VehParts/Turret/LAV25_sight_commander.et:31`, `Generic/VehParts/TPKU/TPKU_turret_base.et:82`,
  `BTR70/VehParts/Turret/PeriscopeDummy_base.et:41`. Other unique names: `hatch_gunner` (BRDM2 gunner),
  `Left/Right/Front/RearTurretCompartment` (helicopter door guns). So: `TURRET && GetCompartmentUniqueName() ==
  "CommanderTurret"` → commander, other `TURRET` → gunner. Modded vehicles may not follow this; LC should keep a
  per-prefab override list in its config.
- **All occupants:** root vehicle's `SCR_BaseCompartmentManagerComponent.GetOccupants(arr)`. (V) The root manager's
  `GetCompartments` includes child-entity slots: `GetVehicle(out compartmentID)` finds a turret's slot in the
  *root* manager's list (`BaseCompartmentSlot.c:120-124`, doc "ID of the compartment relative to the vehicle"),
  enabled by `RegisterCompartments 1` on the turret slot (`BTR70_Base.et:1552-1557`).

Events (V):
- Character side: native `event OnCompartmentEntered/Left(IEntity targetEntity, BaseCompartmentManagerComponent
  manager, int mgrID, int slotID, bool move)` (`CompartmentAccessComponent.c:84-85`), re-exposed by
  `SCR_CompartmentAccessComponent` as `GetOnCompartmentEntered()` / `GetOnCompartmentLeft()` (bare `ScriptInvoker`,
  `scripts/Game/Vehicle/SCR_CompartmentAccessComponent.c:30-41`, invoked :66-67, :156-157) and player-only typed
  `GetOnPlayerCompartmentEnter()` / `Exit()` (:46, :55; invoked only when `GetPlayerIdFromControlledEntity != 0`,
  :84-89). `targetEntity` can be the child entity (turret / cargo frame), not the root
  (`AI/Components/SCR_AIGroupUtilityComponent.c:644` comment "ignoring attachments such as turrets"); resolve with
  `manager.FindCompartment(slotID, mgrID).GetVehicle()`. `move` is the seat-switch flag (I — named so; no vanilla
  consumer branches on it).
- Vehicle side: `EventHandlerManagerComponent` on the vehicle raises `"OnCompartmentEntered"` / `"OnCompartmentLeft"`
  with `(IEntity vehicle, BaseCompartmentManagerComponent manager, IEntity occupant, int managerID, int slotID)`
  (`UI/HUD/WeaponInfo/SCR_WeaponInfo_MultiWeaponTurret.c:262,696`; also
  `ScenarioFramework/Actions/SCR_ScenarioFrameworkActionOnCompartmentEnteredOrLeft.c:76-78`,
  `Weapon/SCR_AnimatedBeltComponent.c:46-47`). Lets one per-vehicle component track occupants without hooking
  characters. Wiki lists the event names (`enforce-script/biki-official-notes.md:120`).
- Where they fire: **server** — (I, strong) AI subscribes (`AI/Components/SCR_AICombatComponent.c:1144`,
  `SCR_AIInfoComponent.c:145`) and AI runs server-side. **Owning client** — (I, strong) local HUD / hints subscribe
  (`SCR_WeaponInfo_MultiWeaponTurret.c:696`, `Campaign/SCR_CampaignFeedbackComponent.c:1962`).
  **Other clients (proxies)** — (I, weak) visual-only consumers subscribe (`SCR_FlashlightComponent.c:285`, animated
  ammo belt), which would be pointless if proxies never got it. Not verified: test with a print on a dedicated
  server + 2 clients before relying on it; the safe design is server-authoritative occupancy → replicated intercom mask,
  or each client reading `GetOccupants()` (replicated) when it builds `players[]`.

## 4. Antenna points and an exterior "infantry phone" action

How vanilla places actions (V):
- `ActionsManagerComponent "{C97BE5489221AE18}"` → `ActionContexts { UserActionContext "{guid}" { ContextName "...";
  Position PointInfo { PivotID "<bone>"; Offset; Angles }; Radius; Height; Omnidirectional; LineOfSightCheck } }`
  and `additionalActions { <ActionClass> "{guid}" { ParentContextList { "<context>" } ... } }`. Example bone-pinned
  context: `BTR70_Base.et` `hatch_l01` with `PivotID "v_hatch_L01"`; whole-vehicle context with no name, offset
  `0 1.1581 0`, `Height 8` (`BTR70_Base.et:1714-1719`). `Vehicle_Base.et:162-177` only adds `additionalActions`
  (refuel/repair) with no contexts; contexts are authored per model.
- (V) Both vanilla APCs already have an exterior rear context `push_rear`: `BTR70_Base.et:2189` (offset
  `0 1.2796 -3.6657`), `LAV25_base.et:1829` (offset `0 1.3262 -2.8211`, radius 0.2, height 2.525,
  `Omnidirectional 0`). BRDM2 has `bumper_Rear`. No vanilla tanks exist in 1.8.
- (V) No runtime API to add contexts or actions: `BaseActionsManagerComponent`
  (`scripts/Game/generated/UserAction/BaseActionsManagerComponent.c`) only reads (`GetContext`, `GetContextList`,
  `GetActionsList`, `FindAction`); `UserActionContext` exposes `GetTransformationWorld`, `GetOrigin`, radius etc.
- (V) Mod precedent for adding an action point to a prop with no inherited actions manager: ACE Radio overrides
  `Prefabs/Props/Core/Antenna_Base.et` with a fresh `ActionsManagerComponent` block, one `UserActionContext
  "default"` (offset `0 1.3 0`) and `ACE_Radio_CopyKeyUserAction` with `ParentContextList { "default" }`
  (`ARMods/ACERadioDev_*/Prefabs/Props/Core/Antenna_Base.et`).

Options for LC:
- **Infantry phone on APCs:** per-model delta override (`BTR70_Base.et`, `LAV25_base.et`, `BRDM2_base.et`) inside
  block `{C97BE5489221AE18}`, adding only `additionalActions { LC_InfantryPhoneAction "{fresh}" { ParentContextList
  { "push_rear" } } }` — reuses the vanilla rear context, no new context. (I) new GUIDs inside an `.et` array append,
  by analogy with the verified `.conf` rule; check in Workbench with `prefab_inspect`.
- Where no suitable context exists (modded tanks), add a `UserActionContext` with a fresh GUID in the same block, or
  let vehicle mods opt in by putting the context + action on their own prefab (LC documents the context name).
- **Generic fallback with no per-model edit:** an action on the whole-vehicle context whose `CanBeShownScript` checks
  the user is behind the hull (dot of `user - vehicle` with `-vehicle.forward`) and within N m. (I) Coarser, but
  covers every vehicle from one `Vehicle_Base` override.
- **Antenna point:** no vanilla antenna bone or point info on vehicles. Only HMMWV variants have a cosmetic
  `Antenna_rear` slot → `M998/VehParts/M998_antenna_rear.et` (`Humr_base.et:1098`, enabled in
  `M998_uncovered_base.et:13-16`, `M998_covered_base.et:16-18`), a destructible hit zone with no radio logic.
  LC should use an `[Attribute]` local offset on its vehicle radio component (default `0 2.5 0`) transformed by
  the vehicle's world transform (wiki GOTCHA: `local.Multiply4(worldTransform)` style), optionally a bone name via
  `PivotID`-like attribute for models that have one.

## 5. Vanilla relay / antenna props (for workstream C)

(V) Radio coverage in vanilla is `RelayTransceiver` on a radio component, read by Conflict
(`SCR_CampaignMilitaryBaseComponent.c:1133-1150`). Carriers: Conflict bases
`Prefabs/Systems/MilitaryBase/ConflictBase_Base.et:27-31` / `ConflictMilitaryBase.et:28-32` /
`ConflictControlPoint.et:4-6` (`SCR_CoverageRadioComponent : ScriptedRadioComponent`, `scripts/Game/Components/SCR_CoverageRadioComponent.c:6`,
range 1000 on bases, inherited on the control point), `Prefabs/Structures/Infrastructure/Towers/TransmitterTower_01/TransmitterTower_01_base.et:92-95`
(`ScriptedRadioComponent` + `RelayTransceiver` 5000 m), and the command trucks (section 1, 2000 m). Antenna props
inherit `Prefabs/Props/Core/Antenna_Base.et` (`SCR_AntennaServicePointComponent`, `ScriptedRadioComponent` with no
transceiver, `RplComponent`; children such as `Props/Military/Antennas/Antenna_R161_Base.et`, `Antenna_RC292_Base.et`,
`Antenna_R142_base.et`); `Props/Military/Radios/KS_RadioStation.et` adds a 10 s "Reconfigure relay" action
(`#AR-Campaign_Action_ReconfigureRelay`). ACE Radio and RHS both override `Antenna_Base.et` (action / radio source).

## Recommendations for LC

- Vehicle radio = an **LC-owned component class** (not a second `BaseRadioComponent`), added by delta overrides of
  `Wheeled_Car_Base`, `Wheeled_Truck_Base`, `Wheeled_APC_Base`, `TrackedVehicle_Base`, `Helicopter_Base` (fresh block
  ID, attributes: range, power, allowed seats, antenna offset, military-only, intercom on/off). Leave vanilla
  `BaseRadioComponent`/`RelayTransceiver` on command trucks untouched (Conflict uses them). Alternative if Duke
  wants zero prefab overrides: `modded class SCR_BaseCompartmentManagerComponent` + a `.conf` keyed by prefab.
- If LC wants vehicle radios to flow through its existing `SCR_VONEntryRadio` / `BaseTransceiver` path, it needs a
  native transceiver to point at; check on Duke's PC whether `SCR_VONEntryRadio.SetRadioEntry(transceiver, idx,
  null gadget)` works for a non-gadget radio before choosing between "native radio component on vehicle" and
  "LC entry class with its own frequency state".
- Seat role: `GetType()` + `GetCompartmentUniqueName() == "CommanderTurret"`, with a config override list per prefab
  for modded vehicles. Crew = PILOT + TURRET; cargo = CARGO (default intercom channels).
- Occupancy: subscribe once per vehicle to the vehicle `EventHandlerManagerComponent` `"OnCompartmentEntered/Left"`
  on the server; release leases there. Clients build intercom masks from replicated `GetOccupants()` each
  `game_state.json` tick rather than trusting proxy events.
- Infantry phone: reuse `push_rear` on BTR70/LAV25 (`bumper_Rear` on BRDM2) via per-model delta overrides adding one
  action; document a context name for third-party vehicle mods; optional `Vehicle_Base` fallback with a
  "behind the hull" `CanBeShownScript`.
- Test early: one Workbench check that an LC override of `Wheeled_APC_Base` and CVON's override of the same file both
  survive (`prefab_inspect` on BTR70), to settle the layering question.

## Engine facts worth wiking

- Vanilla 1.8 vehicle radios: `BaseRadioComponent` with `IntercomTransceiver` on M151A2/UAZ469/UAZ452, empty on
  M923A1/Ural, `RelayTransceiver` 2000 m on command (and Ural repair) trucks; none on APCs or helicopters; VON lists
  only gadget radios (`SCR_GadgetManagerComponent.c:590-597`). `IntercomTransceiver` unused by script.
- Vehicle base tree + shared block IDs (`{20FB66C5DCB8DF72}` compartment manager, `{C97BE5489221AE18}` actions
  manager); `TrackedVehicle_Base` has no vanilla children in 1.8.
- Commander seat = `TurretCompartmentSlot` with `CompartmentUniqueName "CommanderTurret"`; `ECompartmentType` has only
  PILOT/TURRET/CARGO; copilots are `PilotCompartmentSlot`.
- Root vehicle compartment manager lists child turret/cargo slots (`RegisterCompartments 1`); compartment events'
  `targetEntity` may be the child; use `slot.GetVehicle()` / `GetVehicleIn()`.
- Vehicle `EventHandlerManagerComponent` events `OnCompartmentEntered/Left(vehicle, manager, occupant, mgrID, slotID)`.
- No runtime API to add components (only Workbench `CreateComponent`) or action contexts/actions.
- Delta prefab overrides at vanilla GUID are used by many co-loaded mods on the same file (11 on `Character_Base.et`);
  the wiki line "only one wins a prefab override" probably applies to *the same block*, not the whole file — verify
  and correct `enforce-script/modded-classes-and-attributes.md:21`.
