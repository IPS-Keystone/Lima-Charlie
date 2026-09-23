# Server settings

All settings are server-side. They are read on the server and pushed to each client with the session, so
nothing here can be changed by a player.

## Where to put them

Resolved once per session, in layers, each overriding the one before it:

1. **Built-in defaults.**
2. **`Configs/LC/Settings.conf`** in the mod. Where the mod author sets things.
3. **The scenario's mission header.** `SCR_MissionHeader` carries an `LC_Settings` block, so one scenario
   can override the mod for itself. Logs `Settings taken from the mission header`. Whole-object: a header
   that sets one field takes the attribute defaults for every other field, rather than inheriting from
   `Settings.conf`.
4. **`$profile/LimaCharlie/server.json`** — the last word, and the only one a server operator can change
   without rebuilding and republishing the mod. Key by key: the keys present override, everything else
   falls through to the layers above. Logs `server.json overrides: ...` naming each key that took effect.

An empty `LC_Settings { }` means every setting is at its default. That is the shipped state.

To change one in the mod, add just that line:

```
LC_Settings {
 m_fCleanRangePercent 50
}
```

## server.json

Every setting above also lives here, under a plain name. The file is **written out in full the first time
a session runs without one**, holding whatever that session resolved, so there is always a complete file to
edit:

```json
{
 "teamspeakChannel": "LimaCharlie",
 "teamspeakChannelPassword": "",
 "cleanRangePercent": 35,
 "beepRangePercent": 90,
 "terrainEffectPercent": 100,
 "gameMasterUnlimitedRange": true,
 "aiHearing": true,
 "diagnosticLog": false,
 "roomDiagnosticLog": false,
 "channelNaming": "HYBRID",
 "channelLabels": ""
}
```

- Percentages, not fractions, matching the mod's config.
- `channelNaming` takes `LC_ONLY`, `HYBRID` or `VANILLA_ONLY`.
- `channelLabels` is one string, `megahertz,colour,name` per channel, separated by semicolons:
  `"45.5,RED,COMMAND;38,GREEN,MEDEVAC"`. Colours are `WHITE`, `RED`, `ORANGE`, `YELLOW`, `GREEN`, `CYAN`,
  `BLUE`, `PURPLE`. A malformed entry is skipped with a warning; the rest of the list still loads.
- Delete a key to hand that setting back to the mod's config. Delete the file to hand all of them back —
  it will be written again next session.

**The generated file pins those values.** A setting the mod changes later will not move on a server whose
`server.json` already names it. The `server.json overrides:` log line lists exactly which ones are pinned.

Anything the file cannot be parsed as is ignored for the whole session, with an error in the log, rather
than half-applied.

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
| `m_sTeamSpeakChannel` | `LimaCharlie` | — | Channel players are moved into, on whatever TeamSpeak server they are already on. Empty disables channel moves. |
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
