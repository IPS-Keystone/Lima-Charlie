# Troubleshooting

## First checks

Both files live in `$profile/LimaCharlie/`, which for the published game is
`Documents/My Games/ArmaReforger/profile/LimaCharlie/`.

```bash
cat "$USERPROFILE/Documents/My Games/ArmaReforger/profile/LimaCharlie/plugin_state.json"
```

| Field | Should be | If not |
| --- | --- | --- |
| `pluginVersion` | matches the mod's protocol | Install the matching plugin |
| `tsConnected` | `true` | TeamSpeak is not connected to a server |
| `inGameChannel` | `true` in game | The configured channel does not exist, or the move failed |
| `peers` | number of other Lima Charlie players in the channel | 0 means nobody is pairing with you |
| `gameSeq` | rising | The game is not writing; the mod is not running |

The game log also prints a status line whenever any of it changes:

```
[LC] Plugin 0.9.0: TeamSpeak connected=1, client id=3, in game channel=1, peers=1
```

Game logs are in `Documents/My Games/ArmaReforger/logs/logs_<timestamp>/console.log`.

## Diagnostic logging

Set `m_bDiagnosticLog 1` in `Configs/LC/Settings.conf`. Every client then logs both sides of the bridge
once a second. A Game Master always gets the sending half while the editor is open, whatever the setting.

**Sending half** — the whole game state as written:

```
[LC] game_state {"v":6,"seq":362,...,"tx":2,"txRadio":"-2147482979:1",...}
```

**Receiving half** — what the plugin reports back:

```
[LC] plugin_state peers=1, selfTalking=0, talking='', radioRx='', radioHeard='4,123:1,38000,0.95;'
```

`radioHeard` is the decisive field. It lists every radio transmission that client is actually hearing, as
`playerId,radioId,frequency,quality`.

Take the setting back out afterwards — a line a second per client is noisy in a real session.

## "Nobody hears my radio"

Read `tx` and `txRadio` in your own `game_state` line while holding the key:

- **`tx` stays 0** — the transmission never resolved. Check the key is bound, the radio is switched on and
  not muted, and you are not inside the anti-spam lockout.
- **`tx` is 2 or 3 with a `txRadio` that matches an `id` in `radios[]`** — your side is correct and the
  problem is downstream. Check the listener's `radioHeard`.
- **`txRadio` names a radio you did not mean** — the wrong entry is assigned to that key.

Then on the listener: if your transmission appears in their `radioHeard`, the announcement arrived and the
problem is in the audio path. If it never appears, it never reached them — check that both are in the same
TeamSpeak channel and reporting the same session token.

**Solo testing cannot prove transmission works.** The announcement goes to your current TeamSpeak channel;
with nobody in it, the call succeeds and reaches no one. TeamSpeak showing you as talking only proves the
microphone opened.

## "I can hear them but they can't hear me", or the reverse

Reception and transmission take different paths, so an asymmetry is expected to have different causes:

- **Transmission** depends on your client resolving a transmit entry, the plugin finding that radio in
  `radios[]`, and the announcement reaching the channel.
- **Reception** depends only on the plugin reading announcements and your own radios matching frequency and
  encryption key.

Reception working proves the protocol versions match, since a version mismatch rejects the whole file.

## "My microphone never opens"

TeamSpeak's own push-to-talk fights the plugin. Set TeamSpeak to Continuous Transmission or Voice
Activation Detection. The plugin controls the mic through `CLIENT_INPUT_DEACTIVATED` and expects to be the
only thing gating it.

## Keybinds show as unbound but work

The action is missing `FilterPreset` on its `InputSourceValue`, or a row in `keyBindingMenu.conf`. Both are
needed for the menu to display a binding it can also write to.

## Installing the plugin does nothing

TeamSpeak holds `limacharlie_win64.dll` open while running. Close it completely before installing. To confirm what
is actually loaded, check the TeamSpeak log in `%APPDATA%/TS3Client/logs/` for the
`Lima Charlie <version> started` line, or read `pluginVersion` from `plugin_state.json`.

## `World doesn't contain RadioManagerEntity`

A vanilla warning, not ours. A `TransmitterTower_01_small` streamed into a world that has no game mode
setup config, so no radio manager exists. Harmless in a test world; it means the world lacks a proper game
mode, not that Lima Charlie is broken.

## Everything went quiet after a mod update

Protocol mismatch. `lc_game_state_parse` rejects any file whose version it does not recognise, so the
plugin stops reading the game entirely — no direct speech, no radio, in both directions. Update the plugin
to match the mod.

## Known fragilities

- **Terrain answers can be missed.** The plugin waits 300 ms for a clearance answer and then assumes line
  of sight. A dropped answer silently removes the terrain effect for that transmission rather than
  reporting anything.
- **Transmit state can flicker.** During a sustained hold, `tx` has been observed dropping to 1 (direct)
  for a single sample before returning to 2, which means the radio entry momentarily failed to resolve. The
  plugin's 300 ms debounce covers a gap that short, so the transmission survives, but the cause has not
  been tracked down.
- **Game Master transmission has never been confirmed working with a second player.** The sending side is
  verified correct in the log; delivery is not.
