# VON entry API: runtime entries, non-radio entries, intercom and vehicle radios (survey 2026-10-31)

- **Date:** 2026-10-31
- **Game version:** vanilla extract of 1.8.0.13 (`References/ArmaReforger/vanilla`, built 2026-08-21)
- **For:** 3.0 comms branch (`feature/3.0-comms`), design doc `docs/design/3.0-comms-expansion.md` § B "Intercom in the radio menu" and § Shared foundation
- **Searched:** `scripts/Game/VON/*` (SCR_VONController, SCR_VONEntry, SCR_VONEntryRadio, SCR_VONMenu, SCR_VONEntryComponent, SCR_VONAutoTune),
  `scripts/Game/UI/HUD/SelectionMenu/*` (SCR_SelectionMenu, SCR_RadialMenu, SCR_RadialMenuDisplay, SCR_SelectionMenuDisplay, SCR_SelectionMenuEntry, SCR_VONRadialDisplay),
  `scripts/Game/UI/HUD/SCR_VonDisplay.c`, `SCR_GadgetManagerComponent.c`, `SCR_RadioComponent.c`, `SCR_GadgetComponent.c`, `SCR_EditorManagerEntity.c`,
  `generated/Radio/*`, `generated/Components/{BaseRadioComponent,VoNComponent}.c`, `generated/ECommMethod.c`,
  `Prefabs/Characters/Core/DefaultPlayerController.et`, `Prefabs/Editor/EditorManager.et`, `Prefabs/Vehicles/**` (grep radio / transceiver / intercom),
  LC `addon/Scripts/Game/LC/{Radio,Voice,UI}`, Wiki `game/von-and-radios.md`, `GOTCHAS.md`, and Coalition VON (GithubRepos) for approach only.
- **Legend:** **[V]** read in source (file:line given). **[I]** inference, not verified; check in Workbench.

Paths below are relative to `References/ArmaReforger/vanilla/scripts/Game/` unless they start with `LC/` (= `addon/Scripts/Game/LC/`) or `Prefabs/`.

---

## 1. How the entry list is built and how the menu picks up changes

### Storage and API (SCR_VONController)
- [V] `m_aEntries` (`array<ref SCR_VONEntry>`, "all entries except direct") `VON/SCR_VONController.c:67`; direct speech is a separate `m_DirectSpeechEntry` (:66). Active entry `m_ActiveEntry` (:63), `m_LongRangeEntry` (:64), `m_SavedEntry` (:65).
- [V] `AddEntry(entry)` :230-243: `m_aEntries.Insert` → `entry.InitEntry()` → if it is an `SCR_VONEntryRadio` and `IsLongRange()` sets `m_LongRangeEntry` → `m_OnEntriesChanged.Invoke(entry, true)` → **if there is no active entry, `SetEntryActive(entry)`**.
- [V] `RemoveEntry(entry)` :248-271: invokes `m_OnEntriesChanged(entry, false)` first, `DeactivateVON()` + null `m_ActiveEntry` if it was active, clears `m_LongRangeEntry`, **closes the VON radial if it is open** (:259-264), then `m_aEntries.RemoveItem`.
- [V] `GetVONEntries(inout array)` copies (:150-159); `GetVONEntryCount()` :164.
- [V] `SetEntryActive(entry, setFromMenu)` :197-224: swaps `SetActive` flags, fires `m_OnEntriesActiveChanged` (false for old, true for new), deactivates VON if the new entry is unusable, `m_VONDisplay.ShowSelectedVONHint(entry)`, and for radio entries sets `m_LongRangeEntry = entry` + `SetVONLongRange(entry.IsLongRange(), false)`.
- [V] `OnPlayerDeleted` :715-726 resets VON and `m_aEntries.Clear()` (no invokers fire).
- [V] `RpcAsk_SetActiveChannel(int)` :653-660 (server → owner) activates `m_aEntries[channelId]` by index.

### Invokers
- [V] `GetOnEntriesChangedInvoker()` (:75) — `(SCR_VONEntry entry, bool isAdded)`, fired by Add/Remove only.
- [V] `GetOnEntriesActiveChangedInvoker()` (:81) — `(entry, bool isActive)`, fired by `SetEntryActive`.
- [V] `GetOnVONActiveToggled()` (:87) — direct toggle.
- [V] Subscribers in vanilla: `SCR_VONMenu.Init` subscribes **only** to EntriesActiveChanged (`VON/SCR_VONMenu.c:452`). `SCR_VONAutoTune.Init` "subscribes" to EntriesChanged with `Insert(OnEntriesChanged)` (`VON/SCR_VONAutoTune.c:265`), but no method of that name exists in SCR_VONAutoTune; the only `OnEntriesChanged` is the body-less typedef prototype at `SCR_VONController.c:16`. [I] So nothing in vanilla actually reacts to entries being added/removed.

### Who adds/removes entries
- [V] `SCR_GadgetManagerComponent.OnItemAdded` (`Components/Gadgets/SCR_GadgetManagerComponent.c:548-600`): for gadget types in `m_iRadiosFlags` (RADIO | RADIO_BACKPACK, :141) it takes `SCR_RadioComponent.GetRadioComponent()` and adds **one `SCR_VONEntryRadio` per transceiver**: `new` → `SetRadioEntry(radioComp.GetTransceiver(i), i + 1, gadgetComp)` → `m_VONController.AddEntry` (:593-598). Hooked to `SCR_InventoryStorageManagerComponent.m_OnItemAddedInvoker` (:37).
- [V] `OnItemRemoved` :606-643: removes every `SCR_VONEntryRadio` whose `GetGadget() == gadgetComp` (:630-639).
- [V] `RegisterVONEntries` :940-974 / `UnregisterVONEntries` :977-997: bulk add/remove, run on `OnControlledByPlayer` (:666-688), init (:1096) and `OnPlayerDeath` (:657).
- [V] `SCR_EditorManagerEntity` open (`Editor/Entities/SCR_EditorManagerEntity.c:159-195`): adds entries for the **editor manager's own** `BaseRadioComponent` (`Prefabs/Editor/EditorManager.et:4` + `SCR_RadioComponent` :159), with `SetChannelTextOverwrite("GM …")`; close removes them (:244-253). This is the one vanilla precedent of radio entries that are not inventory items.

### How the radial menu picks up changes
- [V] The radial is rebuilt **only on open**: `SCR_VONMenu.OnInputOpenMenu` (`VON/SCR_VONMenu.c:150-209`) bails if the controller has 0 entries (:185), then `m_RadialMenu.ClearEntries()`, `GetVONEntries`, `AddRadialEntry(entry)` for each (:191-199), plus a disabled dummy when count < 8 and odd (:201-207).
- [V] `AddRadialEntry` (:81-91) does `entry.SetName(entry.GetDisplayText())`, adds to the radial, and calls **`entry.InitEntry()` again** (InitEntry runs on every menu open, not just on `AddEntry`).
- [V] While open: `SCR_VONController.AddEntry` does **not** touch the radial → a new entry is invisible until the menu is reopened. `RemoveEntry` **closes** the menu. Perform-on-close is off for VON (`Prefabs/Characters/Core/DefaultPlayerController.et:488` `m_bPerformOnClose 0`, :489 `m_bCloseOnPerform 1`), so the forced close does not activate the hovered entry.
- [V] The radial itself can redraw live: `SCR_SelectionMenu.AddEntry/RemoveEntry` (`UI/HUD/SelectionMenu/SCR_SelectionMenu.c:489-505, 575-586`) fire `InvokeOnUpdateEntries`, and `SCR_SelectionMenuDisplay.OnMenuEntriesUpdate` (:240-247) recreates widgets when the list changed; `SCR_RadialMenuDisplay.OnMenuEntriesUpdate` (:317-325) only while open. So `m_VONMenu.AddRadialEntry(entry)` (public) or `GetRadialMenu().RemoveEntry(entry)` on an open menu would redraw it. [I] Not exercised by vanilla for VON; the odd-count dummy would not be re-balanced.
- [V] Per-entry visuals refresh via `m_RadialMenu.UpdateEntries()` (`SCR_SelectionMenu.c:549-558`: invoke + `entry.Update()` each), called on select (`SCR_VONMenu.c:296`), perform (:258), active change (:303), toggle (:372, delayed 150 ms) and mute (:416). LC already has `LC_RefreshMenu()` (`LC/Voice/LC_VONController.c:141-149`).
- [V] The centre label is `entry.GetName()` set on **selection change** (`RadialMenu/SCR_RadialMenuDisplay.c:396-401`); the name itself is only set in `AddRadialEntry`. [I] An entry that changes its text on scroll must call `SetName` itself, and the centre label may still lag until the next hover change.

## 2. Can an entry exist without a transceiver? What breaks?

**Yes.** `SCR_VONEntry : SCR_SelectionMenuEntry` (`VON/SCR_VONEntry.c:3`) has no transceiver at all; every hook is a no-op default: `InitEntry` :13, `SetUsable/IsUsable` :18-28 (default usable), `SetActive/IsActive`, `SetMuted`, `SetSelected` :55, `AdjustEntry(int)` :61 (cycle-channel input), `AdjustEntryModif(int)` :67 (tune/scroll input), `ToggleEntry` :72 (menu action), `GetDisplayText` :77 (`m_sText`), `GetIconResource` :84, `GetVONMethod` :91 (**DIRECT**), `Update` :98. Vanilla itself puts a plain `SCR_VONEntry` in the radial (the dummy, `SCR_VONMenu.c:203`).

### Input paths in the open menu (SCR_VONMenu) — all safe for a non-radio entry [V]
- Scroll (`VONMenuTuneFrequency`) → `ActionTuneFrequency` :336 → `OnAdjustEntry(±1, true)` :309 → `entry.AdjustEntryModif(input)`. Cycle (`VONMenuCycleChannel`) → `AdjustEntry(input)`. The radio-only follow-up is behind a cast (:325-331). **But** `OnAdjustEntry` always calls `m_Display.SetFrequenciesVisible(true)` (:314) → the squad/platoon frequency table pops up for any entry.
- `VONMenuAction` → `OnMenuToggle` :362 → `ToggleEntry()` on any entry, radio follow-up behind a cast.
- `VONMenuMuteAction` → `OnMenuMuteToggle` :385 → returns for non-radio.
- Hover → `OnEntrySelected` :263: non-radio goes to the `else` branch (hides frequency table and preview) — safe.
- Disabled entries (`Enable(false)`) can still be hovered (`RadialMenu/SCR_RadialMenu.c:254-257` sets the selection without checking `IsEnabled`) but not performed (`SCR_SelectionMenu.c:373`); they are drawn at reduced opacity (`SCR_SelectionMenuEntryComponent.c:127-139`).

### What breaks if a non-radio entry is in `m_aEntries` [V]
1. **Perform (click / perform key) makes it the active entry**: `OnEntryPerformed` → `m_VONController.SetEntryActive(entry, true)` (`SCR_VONMenu.c:254-259`). Also `AddEntry` makes it active if nothing is (`SCR_VONController.c:241-242`), and `RpcAsk_SetActiveChannel` by index. Consequences while it is active:
   - `ShowSelectedVONHint` shows "Direct talk" (`UI/HUD/SCR_VonDisplay.c:544-547`, because `GetVONMethod()` is DIRECT).
   - The radio key (`VONChannel`) → `SetVONBroadcast` → `ActivateVON(CHANNEL)` → `GetEntryByTransmitType(CHANNEL)` returns this entry (:604-618) → `SetActiveTransmit` sets `ECommMethod.DIRECT` (:623-641) while `m_eVONType = CHANNEL`. The radio key becomes a second direct-speech key; LC's `LC_GetTransmitType()` reports CHANNEL while `LC_GetTransmitEntry()` returns null.
   - `TransceiverCycle` does nothing (casts `m_ActiveEntry` to radio and returns, :480-482).
2. **Crash: `SCR_RadioComponent.RadioToggle`** (`Components/Gadgets/SCR_RadioComponent.c:64-85`) loops over all VON entries and does `SCR_VONEntryRadio.Cast(entry)` then `radioEntry.GetTransceiver().GetRadio()` **without a null check** → NULL pointer as soon as anyone uses the radio on/off inspection action (`UserActions/Inspection/SCR_RadioToggleUserAction.c:18`) while a non-radio entry is listed. Must be fixed by LC (modded `RadioToggle`) before any non-radio entry ships.
3. Everything else that walks the list null-checks the cast: `SelectFirstUnmutedEntry` :276-298, `SetVONLongRange` :397-406, `TransceiverCycle` :489-496, gadget manager removal :634-639 / :990-996, `SCR_VONMenu.GetGroupedRadioIndex/GetGroupedEntries` :103-147, `SCR_VONAutoTune.FindFirstRadio` (`VON/SCR_VONAutoTune.c:173-199`), and LC's `LC_GetRadioEntries` (`LC/Voice/LC_VONController.c:92-101`, radios with a transceiver only).
4. `GetGroupedRadioIndex` returns an index into `m_aEntries` and uses it as a radial segment index (`SCR_VONMenu.c:274`) — still consistent, because the radial is built in `m_aEntries` order. [I] A non-radio entry placed in the list just shifts indices consistently.

### What a subclass should override (intercom-style entry)
| Method | Why |
|---|---|
| `InitEntry()` | runs on `AddEntry` and on every menu open; `SetCustomLayout(...)` (vanilla `{033302D7C8158EF8}UI/layouts/HUD/VON/VONEntry.layout` or LC's `LC_VONEntry.layout`) so `m_EntryComponent` is an `SCR_VONEntryComponent`; set `m_sText` |
| `AdjustEntryModif(int)` | scroll: cycle the state, update `m_sText`, `SetName(m_sText)`, play a UI sound |
| `AdjustEntry(int)` | cycle-channel input: same as above (or no-op) |
| `ToggleEntry()` | menu action key: e.g. jump to "No Intercom" / back |
| `GetDisplayText()` | base returns `m_sText`; only override if computed |
| `GetIconResource()` | optional |
| `Update()` | `SCR_VONEntryComponent.Cast(m_EntryComponent)` then `SetTransceiverText / SetFrequencyText / SetChannelText / SetPowerIcon / SetActiveIcon / SetMuteIcon` (`VON/SCR_VONEntryComponent.c`; setters do **not** null-check their widgets, so the layout must be the VON one); bail if the component is null (first Update fires before it exists, cf. `SCR_VONEntryRadio.c:299-301`) |
| `GetVONMethod()` | leave DIRECT (intercom rides direct speech) |
| `IsUsable()` | leave true, or false when the vehicle intercom is dead |

And outside the entry: block it from ever becoming active (see §5).

## 3. How LC hooks VON today, and how the new entries fit

### Current LC hooks [V]
- `LC/Voice/LC_VONController.c` (modded `SCR_VONController`):
  - `ActivateVON(entry, type)` :6-49 — vanilla copy with `SetCapture(false)` (TeamSpeak carries voice; vanilla state, UI and inputs stay). Encryption check null-safe on `GetTransceiver()` (:30).
  - `OnDelete` :54-61 stops `LC_Client`; `ActionVONTransceiverCycle` :64-68 adds a UI sound.
  - Accessors: `LC_GetTransmitType` :72, `LC_GetTransmitEntry` :82 (radio cast), `LC_GetRadioEntries` :92 (radio entries with a transceiver, vanilla order — PTT slots index into this), `LC_CanTransmitOn` :105, `LC_GetHoveredRadioEntry` :127, `LC_RefreshMenu` :141.
- `LC/Radio/LC_VONEntryRadio.c` (modded `SCR_VONEntryRadio`): `InitEntry` swaps to `LC_VONEntry.layout` (:7-11); `Update` (:15-42) adds ear/volume, TX key and beep-set line and LC channel names; `LC_SetFrequency` (:75-87) tunes and updates the cached text immediately.
- `LC/Radio/LC_VONMenu.c` (modded `SCR_VONMenu`, `[BaseContainerProps()]` repeated): `ActionTuneFrequency` (:7-14) swallows the scroll while `LC_RadioVolume` (Ctrl) is held, else `super`.
- `LC/Radio/LC_VONEntryComponent.c`: extra `RadioDetailsText` widget, null-safe.
- `LC/Radio/LC_Radio.c`: radio key = `RplComponent.Id()` of `transceiver.GetRadio().GetOwner()` (:11-27), duplex from `SCR_RadioComponent` on the owner (:38-53), `IsLongRange` guards `GetGadget()` (:56-59), `BuildJson` for `radios[]` (:84-118).
- `LC/UI/LC_VonDisplay.c` / `LC_VonDisplayFeed.c`: channel names on the HUD, drive the display from LC radio entries.
- Nothing in LC subscribes to `GetOnEntriesChangedInvoker`; `LC_Client.RefreshRadioEntries` (`LC/LC_Client.c:220-226`) re-reads the list.

### LC_VONEntryIntercom — fit
- `class LC_VONEntryIntercom : SCR_VONEntry` with an enum state (NONE / CREW / CARGO / MISC) and a reference to the vehicle (or its intercom component). `AdjustEntryModif(±1)` cycles No Intercom → Crew → Cargo → Misc (clamped or wrapping — Duke's call), writes the local intercom mask that `game_state.json` publishes (`self` + `players[]`), `SetName`, sound. No transmit key: `GetVONMethod()` stays DIRECT and the entry is never made active.
- Because it is not an `SCR_VONEntryRadio`, LC's radio list, PTT slots, `radios[]` and HUD feed ignore it automatically (all go through `LC_GetRadioEntries`).
- The design doc's "removed on leaving" is compatible, but `RemoveEntry` closes an open radial (above). Getting out of a vehicle while the menu is open closing it is acceptable; seat changes should **update the existing entry's default**, not remove/re-add it.

### Vehicle radio as an ordinary radio entry while seated
- [V] Script requirements of `SCR_VONEntryRadio`: `SetRadioEntry(notnull BaseTransceiver, int number, SCR_GadgetComponent gadget)` (`VON/SCR_VONEntryRadio.c:62-67`). The transceiver can come from a `BaseRadioComponent` on **any entity** (editor precedent). But the gadget is effectively mandatory: `IsLongRange()` does `m_GadgetComp.GetType()` unguarded (:56-59) and is called from `AddEntry` itself (`SCR_VONController.c:235`); `GetUIInfo()` does `m_GadgetComp.GetOwner()` unguarded (:45-52, called by `SCR_VonDisplay.ShowSelectedVONHint` :552); `SCR_VONMenu.OnEntrySelected` does `selected.GetGadget().GetOwner()` for the 3D preview (:279). A null gadget = NULL pointer on add.
- [V] No owner / inventory checks anywhere in script: `ActivateVON` checks life state, cross-faction encryption key and (via `SetVONBroadcast`) `IsUsable()` only (`SCR_VONController.c:538-581`, :341-365). `SetActiveTransmit` hands the transceiver to `VoNComponent.SetTransmitRadio` (:631). Whether the **engine** accepts a transceiver that is not in the speaker's hierarchy is not visible in script — [I] the editor works with transceivers on `SCR_EditorManagerEntity`, but there the `SCR_VoNComponent` is on the same entity (`SCR_EditorManagerEntity.c:150-156`). For LC this matters little: LC never starts capture, and reception is computed by the plugin from `radios[]`.
- [V] Frequency/power are engine-replicated: `BaseTransceiver.SetFrequency` "Supports proxies and server" (`generated/Radio/BaseTransceiver.c`), `BaseRadioComponent.SetTransceiverFrequency` "Set frequency … and sync with server", `SetRange` "Has effect only on owner (server)". The editor code notes `SetPower()` "needs to check if the owner of the radio is a active game master" (`SCR_EditorManagerEntity.c:167-168`) → **the engine does ownership/permission checks on at least `SetPower`**. [I] A passenger tuning a server-owned vehicle radio from a proxy may be silently ignored; Coalition VON routes vehicle-radio state through player-controller RPCs keyed by RplId (approach only, `GithubRepos/Coalition-VON/.../CVON_SCR_PlayerController.c:270-340`).
- Where to put the radio: see recommendation (child radio entity rather than a second `BaseRadioComponent` on the vehicle).

## 4. Vanilla support for vehicle intercom / vehicle radios
- [V] **No VON-level support.** `EVONTransmitType` is NONE / DIRECT / CHANNEL / LONG_RANGE (`SCR_VONController.c:3-9`); `ECommMethod` is DIRECT / SQUAD_RADIO / DIRECT_AI / GAME_MASTER_DIRECT / GAME_MASTER_RADIO (`generated/ECommMethod.c`). No "vehicle"/"compartment"/"intercom" in `VON/*.c`, `SCR_RadioComponent.c`, `GameCode/Network/SCR_VoNComponent.c`, or `Sounds/VON/von.acp`.
- [V] **But vanilla vehicles already carry radios:**
  - `IntercomTransceiver : BaseTransceiver` exists as an engine class with no members (`generated/Radio/IntercomTransceiver.c`); **no script references it**.
  - `BaseRadioComponent` with an `IntercomTransceiver` ("Turned on" 1) on `Prefabs/Vehicles/Wheeled/M151A2/M151A2.et:217-223` ("Frequency resolution" 1500), `UAZ469/UAZ469_base.et:263-268`, `UAZ452/UAZ452_base.et:309-314`.
  - `BaseRadioComponent` with no transceivers ("Turned on" 1) on `M923A1/M923A1.et:166-168` and `Ural4320/Ural4320.et:278-280`; the `_command` variants add a `RelayTransceiver` ("Transmitting Range" 2000) for Conflict's mobile HQ (`M923A1_command.et:11-17`, `Ural4320_command.et:11-15`), read by `Campaign/SCR_CampaignMobileAssemblyComponent.c:91-108` via `truck.FindComponent(BaseRadioComponent).GetTransceiver(0)`.
  - Tracked, wheeled APCs and helicopters: no `BaseRadioComponent` in their prefabs (grep of `Prefabs/Vehicles`).
  - None of these radios has an `SCR_RadioComponent` gadget, so none ever becomes a VON entry. [I] The intercom transceivers look like unfinished engine plumbing.
- [V] Seat events for driving add/remove: `SCR_CompartmentAccessComponent.GetOnCompartmentEntered/Left()` (`Vehicle/SCR_CompartmentAccessComponent.c:30-44`, invoked :64-67 and :154-157 with `(targetEntity, manager, mgrID, slotID, move)`).

---

## Recommended design for LC_VONEntryIntercom and vehicle radio entries

1. **One reconciler, not event-driven add/remove.** A small LC client routine (in `LC_Client`'s tick or on compartment enter/leave + controlled-entity change) computes the wanted LC entries (intercom: in a vehicle with an LC intercom, or holding its phone; vehicle radio: seated in an allowed compartment) and `AddEntry`/`RemoveEntry`s the difference. This also survives `OnPlayerDeleted` clearing the list and death/possession (where the gadget manager unregisters its own entries).
2. **Never let LC_VONEntryIntercom be active.** In `modded SCR_VONController`: `override void SetEntryActive(SCR_VONEntry entry, bool setFromMenu = false) { if (LC_VONEntryIntercom.Cast(entry)) return; super.SetEntryActive(entry, setFromMenu); }`. That covers menu perform, `AddEntry`'s auto-activate and the index RPC. Keep it `Enable(true)` so it is not dimmed; the click still closes the menu (`m_bCloseOnPerform`). Optionally make perform cycle too by overriding `OnPerform()` on the entry.
3. **Fix `SCR_RadioComponent.RadioToggle`** (modded, null-check the cast) in the same change that introduces the intercom entry.
4. **Modded `SCR_VONMenu.OnAdjustEntry`**: for `LC_VONEntryIntercom`, call `AdjustEntryModif/AdjustEntry`, `SetName`, `m_RadialMenu.UpdateEntries()`, and skip `m_Display.SetFrequenciesVisible(true)`. Keep LC's Ctrl-scroll volume guard in `ActionTuneFrequency` (maybe Ctrl-scroll = intercom volume later).
5. **Insert position.** `AddEntry` appends; put the intercom **first or last** consistently. Since PTT slots index `LC_GetRadioEntries()` (radios only), position does not shift TX keys. [I] Last is least surprising.
6. **Live redraw.** On add while the menu is open, also call `m_VONMenu.AddRadialEntry(entry)` (public) or accept "visible on next open". Removal closes the menu (vanilla behaviour) — fine for getting out.
7. **Vehicle radio = an LC radio child entity on the vehicle**, built from a radio item base so it has `RplComponent`, `BaseRadioComponent`, `SCR_RadioComponent` (gadget; set `m_iRadioCategory` MANPACK if it should behave as the long-range set) and `InventoryItemComponent` (UIInfo name/icon for the HUD hint and radial preview). Add it as a normal `SCR_VONEntryRadio` with `SetRadioEntry(tx, i+1, gadget)` while seated. Reasons: the vanilla entry needs a gadget; a second `BaseRadioComponent` on the vehicle entity collides with the existing intercom/relay one (`FindComponent` returns the first, `SCR_RadioComponent.OnPostInit` binds the first, `SCR_CampaignMobileAssemblyComponent` reads `GetTransceiver(0)`); `LC_Radio.GetRadioKey` needs an `RplComponent` on the radio owner; the existing LC radio code (duplex, beep, ear, volume, channel names) then works unchanged. Optionally subclass `LC_VONEntryVehicleRadio : SCR_VONEntryRadio` only to mark "vehicle" in `radios[]` (source kind + antenna position, Shared foundation 1) and to route tuning (next point).
8. **Tuning/power via server RPC.** Override the vehicle entry's `AdjustEntryModif / AdjustEntry / ToggleEntry` (and LC's `LC_SetFrequency` path) to ask the server through LC's player-controller component (seat check = the lease of foundation 2), updating the entry's cached `m_iFrequency`/`m_sText` immediately as `LC_SetFrequency` does. Use the direct `SetFrequency` only if Workbench shows proxies can tune a vehicle-owned radio.

## Risks / to verify in Workbench
- [ ] Whether `BaseTransceiver.SetFrequency` / `BaseRadioComponent.SetPower` from a passenger's proxy changes a vehicle radio (server-owned, or driver-owned during physics). Test with two clients: driver and passenger tune; check on the other client and on the server.
- [ ] Engine reaction to `VoNComponent.SetTransmitRadio(vehicleTransceiver)` on the character's VoN component (log noise? ignored?). LC does not capture, but the call still happens in `SetActiveTransmit`.
- [ ] `RadioManagerEntity` registers the vehicle child radio's transceivers globally; [I] harmless for LC (plugin decides reception), but vanilla-VON fallback players would hear it.
- [ ] Spawning/attaching the child radio entity to vanilla vehicles without overriding vanilla prefabs (`Wiki/engine/config-and-prefab-layering.md`): slot via modded base prefab vs server-side spawn + `AddChild`; replication of the hierarchy to JIP clients.
- [ ] `SCR_GadgetComponent` behaviour on an entity that is never in an inventory (`OnPostInit` only hooks `InventoryItemComponent.m_OnParentSlotChangedInvoker`, `Components/Gadgets/SCR_GadgetComponent.c:425-432`; dropped radios already live like this, so [I] low risk). Make sure the gadget manager never "picks it up" (it only reacts to inventory events).
- [ ] Centre label after scroll: does `SetName` + `UpdateEntries()` refresh `m_wSelectedName`, or only the next hover change?
- [ ] The odd-count dummy entry when an entry is added to an open menu.
- [ ] `RemoveEntry` closing the menu on seat change if the reconciler removes the vehicle radio while the menu is open (e.g. moving from commander to cargo).
- [ ] `SCR_VONAutoTune.FindFirstRadio` would also find a vehicle radio with an `SCR_RadioComponent` of matching type/key and could auto-tune it on respawn/possession; decide if that is wanted (give the vehicle radio its own `ERadioType` or key to avoid it).
- [ ] Long-range semantics: a MANPACK-category vehicle radio becomes `m_LongRangeEntry`, replacing a worn backpack radio's slot (`AddEntry` overwrites, `RemoveEntry` nulls it without restoring the backpack). Vanilla has a single LRR slot.
- [ ] Encryption: vanilla blocks transmitting on a radio whose key ≠ faction key unless cross-faction is allowed (`ActivateVON`); a vehicle captured from the enemy keeps its key (cf. CVON's "key fill").

## Engine facts worth wiking
(For `Wiki/game/von-and-radios.md`.)
- `SCR_VONController.AddEntry/RemoveEntry` are the runtime API; `AddEntry` auto-activates the first entry; `RemoveEntry` closes an open VON radial; the radial is rebuilt only on open (`SCR_VONMenu.OnInputOpenMenu`) and `InitEntry` runs again on every open.
- `GetOnEntriesChangedInvoker` has no real vanilla subscriber (AutoTune inserts the body-less typedef `OnEntriesChanged`).
- A plain `SCR_VONEntry` subclass works in the menu (scroll → `AdjustEntryModif`, cycle → `AdjustEntry`, action → `ToggleEntry`), but menu perform makes it the active entry and the radio key then sends direct speech; block it in `SetEntryActive`.
- Vanilla NULL-pointer: `SCR_RadioComponent.RadioToggle` assumes every VON entry is an `SCR_VONEntryRadio`.
- `SCR_VONEntryRadio` needs a non-null `SCR_GadgetComponent` (`IsLongRange` is called inside `AddEntry`; `GetUIInfo`, radial preview); the transceiver may live on any entity (editor manager precedent).
- Disabled selection-menu entries can be hovered but not performed; they render dimmed.
- Vanilla vehicles: `IntercomTransceiver` on M151A2 / UAZ469 / UAZ452, empty `BaseRadioComponent` on M923A1 / Ural, `RelayTransceiver` 2000 m on their `_command` variants (Conflict MHQ); no script uses `IntercomTransceiver`; no APC/tank/heli radios. No vehicle/intercom notion in `EVONTransmitType` / `ECommMethod`.
- Engine checks radio ownership for at least `BaseRadioComponent.SetPower` (editor comment); `SetFrequency` "supports proxies and server", `SetRange` server only.
