# Server settings

All settings are server-side. They are read on the server and pushed to each client with the session, so
nothing here can be changed by a player.

## Where to put them

Resolved once per session, in this order — the first that exists wins as a whole object, not field by field:

1. **The scenario's mission header.** `SCR_MissionHeader` carries an `LC_Settings` block, so one scenario
   can override the defaults for itself. Logs `Settings taken from the mission header`.
2. **`Configs/LC/Settings.conf`** in the mod. The normal place.
3. **Built-in defaults**, if the config fails to load. Logs a warning.

Because it is whole-object, a mission header that sets one field takes the attribute defaults for every
other field — it does not inherit from `Settings.conf`.

An empty `LC_Settings { }` means every setting is at its default. That is the shipped state.

To change one, add just that line:

```
LC_Settings {
 m_fCleanRangePercent 50
}
```

## The settings

| Setting | Default | Range | Effect |
| --- | --- | --- | --- |
| `m_fCleanRangePercent` | 35 | 0–95 | Share of a radio's range that stays perfectly clear. Past it the signal garbles out to the edge. Never changes the range itself. |
| `m_fBeepRangePercent` | 90 | 0–100 | How far out you hear other people's start and end beeps. 0 silences them entirely. |
| `m_fTerrainEffectPercent` | 100 | 0–300 | How much hills cost. 100 is the TFAR model, 0 ignores terrain, 200 makes a ridge twice as expensive. |
| `m_aChannelLabels` | empty | — | Named frequencies. One entry per net you want labelled. |
| `m_eChannelNaming` | `HYBRID` | — | Where channel names come from: `LC_ONLY`, `HYBRID`, `VANILLA_ONLY`. |
| `m_bGameMasterUnlimitedRange` | on | — | Game Masters transmit and receive without range or terrain limits while the editor is open. |
| `m_bDiagnosticLog` | off | — | Every client logs its side of the bridge once a second. Troubleshooting only. |
| `m_bRoomDiagnosticLog` | off | — | Every client logs what the engine's room model says about its neighbours, once a second. Troubleshooting only, and far quieter than the line above. |
| `m_bAIHearing` | on | — | Enemy AI turn towards players speaking out loud. |
| `m_sTeamSpeakServer` | empty | — | Server players are moved to. Empty leaves everyone where they are. |
| `m_sTeamSpeakChannel` | empty | — | Channel players are moved into. Empty disables channel moves. |
| `m_sTeamSpeakChannelPassword` | empty | — | Password for that channel, if it has one. |

## Clean range, in practice

This is the most consequential setting. It decides where degradation begins as a share of each radio's
range, and the curve from there to the edge is linear.

- **Lower** (10–25%) — radios garble early and most of the map sounds rough. Good for a gritty feel, bad
  for anyone trying to pass grid references.
- **Default** (35%) — clear inside a third of range, audibly degrading beyond that.
- **Higher** (70–90%) — effectively clean radios that fall apart only near the edge.
- **95%** is the cap. There is always a garbled band, because a hard cliff at the range edge sounds like a
  bug rather than a radio.

## Channel labels

Each entry gives a frequency a name and a colour. The list is empty by default — there are no built-in
names.

```
LC_Settings {
 m_aChannelLabels {
  LC_ChannelLabel {
   m_fFrequencyMHz 45.5
   m_sName "COMMAND"
   m_eColour RED
  }
  LC_ChannelLabel {
   m_fFrequencyMHz 38
   m_sName "MEDEVAC"
   m_eColour GREEN
  }
 }
}
```

The frequency is in MHz exactly as it reads on the radio, so `45.5` matches a radio tuned to 45.500 MHz.
Colours are `WHITE`, `RED`, `ORANGE`, `YELLOW`, `GREEN`, `CYAN`, `BLUE`, `PURPLE`. Commas and semicolons
are stripped from names, because the list is flattened to a delimited string for the trip to each client.

## Naming modes

`HYBRID` is the default and usually what you want: your named nets get their names, and anything you have
not named falls back to the game's own labels for the platoon net, playable group callsigns and task nets.

`LC_ONLY` hides the game's labels entirely, so an unnamed frequency shows as a bare frequency. Use it when
you want the radio display to reflect only your own net structure.

`VANILLA_ONLY` ignores your list altogether, which is mostly useful for turning the feature off without
deleting the list.

## Diagnostic logging

Off by default and noisy — one line per second per client, plus a second line on the receive side. Turn it
on only while investigating something, and take it back out afterwards: the sending half is the whole
game state, which with 30 players nearby is around 3 KB a line.

See [troubleshooting.md](troubleshooting.md) for how to read the output.
