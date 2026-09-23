# Server setup

Standing up Lima Charlie on a server, start to finish. For what each setting does, see
[server-settings.md](server-settings.md); for a player's side of it, [player-guide.md](player-guide.md).

## What the server actually does

Very little. It hands each joining client a session token and the settings below, and that is the whole of
its involvement in voice. Audio never touches the game server: TeamSpeak carries it, and each listener's
own plugin decides what they can hear.

Two consequences worth knowing before you plan anything:

- **The session token** changes every time the server session restarts. Plugins only pair players reporting
  the same token, so two Reforger servers can share one TeamSpeak server without hearing each other.
- **Everyone needs both halves**, and matching ones. A player with the mod but no plugin hears nothing and
  is heard by nobody. A player with a plugin of the wrong protocol is in the same position — the mismatch
  is rejected outright rather than degrading.

## 1. The TeamSpeak side

You need a TeamSpeak 3 server your players can reach. The mod never connects anyone to one; it only moves
people between channels on the server they are already connected to.

Create a channel named **`LimaCharlie`**. That is the shipped default, so with a channel of that name there
is nothing else to configure. Give it a password only if you want one, and put that in the settings below.

Recommendations for that channel:

- **No codec starvation.** Voice quality is whatever TeamSpeak gives you; the plugin only filters it.
- **Let it hold everyone.** Every player in the session sits in this one channel, and the plugin decides
  who is audible. A channel limit smaller than your player count will silently leave people out of the
  voice session.
- **Tell players to use continuous transmission or voice activation**, not TeamSpeak's own push-to-talk.
  The plugin holds the microphone open and shut to follow the game, and TeamSpeak's push-to-talk fights it.

## 2. The mod

Subscribe the server to the Workshop mod and add it to the server's mod list as usual. Nothing in the addon
needs editing to get going.

## 3. Settings

Everything is server-side. Players cannot change any of it.

Resolved in layers, each overriding the one before:

1. Built-in defaults
2. `Configs/LC/Settings.conf` in the mod
3. The scenario's mission header, if it carries an `LC_Settings` block
4. `$profile/LimaCharlie/server.json`

**`server.json` is where you should work.** It is the only layer you can edit without rebuilding and
republishing the mod, and it wins over the others. It is written out in full the first time a session runs
without one, so start the server once and then edit the file that appears:

```
<server profile>/LimaCharlie/server.json
```

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

Only the keys present in the file override anything, so you can cut it down to the two or three you care
about. Each key that takes effect is named in the log at session start:

```
[LC] server.json overrides: cleanRangePercent channelLabels
```

**The catch:** a key in this file pins that setting. If a later mod update changes a default, your server
keeps your value. Delete a key to hand that setting back to the mod, or delete the file to hand back all of
them — it is written again next session.

### Naming your nets

`channelLabels` gives frequencies names and colours, shown on the VON display whenever anyone transmits or
receives on them. One string, `megahertz,colour,name` per channel, semicolons between:

```json
"channelLabels": "45.5,RED,COMMAND;38,GREEN,MEDEVAC;50.2,CYAN,LOGI"
```

Colours: `WHITE`, `RED`, `ORANGE`, `YELLOW`, `GREEN`, `CYAN`, `BLUE`, `PURPLE`. A malformed entry is
skipped with a warning and the rest still load.

`channelNaming` decides what happens to frequencies you have not named: `HYBRID` (default) falls back to
the game's own platoon and task net labels, `LC_ONLY` shows nothing for them, `VANILLA_ONLY` ignores your
list entirely.

### Settings worth thinking about

| Setting | Why you might move it |
| --- | --- |
| `cleanRangePercent` | The big one. 35 means clear inside a third of a radio's range, garbling beyond. Raise it for forgiving comms, lower it for a rough feel. |
| `terrainEffectPercent` | 0 ignores hills altogether; 200 makes a ridge twice as costly. Worth lowering on very hilly maps if your players cannot hold a net. |
| `gameMasterUnlimitedRange` | On by default, so a Game Master can reach anyone. Turn off to hold them to the same ranges as everyone else. |
| `aiHearing` | Enemy AI turning towards players who speak out loud. Off if you do not want voice to have tactical consequences. |

## 4. Checking it works

At session start the server logs its resolved settings:

```
[LC] Settings: TeamSpeak channel 'LimaCharlie', clean radio range 35 percent, beep range 90 percent, terrain effect 100 percent, AI hearing 1
```

On a client, the plugin reports itself once it pairs up:

```
[LC] Plugin 1.0.4: TeamSpeak connected=1, client id=7, in game channel=1, peers=3
```

`in game channel=1` means the move into your channel worked. `peers` is how many other Lima Charlie players
that client has paired with — if it stays at 0 with other people in the channel, they are on a different
token or a different protocol.

**Test with two people.** One player alone proves the microphone opens and nothing else: radio
announcements go to the TeamSpeak channel, so with nobody there to hear them they succeed and reach no one.

## 5. Game Masters

A Game Master with the editor open listens from the camera, not from a body, and hears by the normal rules.
With `gameMasterUnlimitedRange` on they also transmit to the whole map on whatever radio they hold.

A Game Master with no character at all still hears. A dead player hears nothing.

## Troubleshooting

[troubleshooting.md](troubleshooting.md) covers the diagnostic logs, including the two server switches
(`diagnosticLog`, `roomDiagnosticLog`) and how to read what they print. Both are noisy; leave them off for
normal play.
