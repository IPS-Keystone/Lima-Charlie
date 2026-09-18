# Installation

## What every player needs

1. **TeamSpeak 3** — not TeamSpeak 5/6. The plugin is built against the TS3 plugin SDK.
2. **The Lima Charlie plugin**, installed into TeamSpeak.
3. **The mod**, subscribed from the Workshop.
4. **A microphone mode that is not TeamSpeak push-to-talk** (see below).

Windows only. The plugin is a 64-bit DLL; there is no Linux or Mac build, and console crossplay is out of
scope.

## Installing the plugin

Double-click `LimaCharlie_<version>.ts3_plugin` with TeamSpeak closed. TeamSpeak holds
`limacharlie_win64.dll` open while it runs, so installing over a running client silently fails or errors.

If the installer produces empty files — TeamSpeak's installer has done this with zips lacking directory
entries — copy the contents of `plugin/build/package/plugins/` into
`%APPDATA%/TS3Client/plugins/` by hand instead. That directory needs:

```
%APPDATA%/TS3Client/plugins/
  limacharlie_win64.dll
  limacharlie/sounds/<set>/*.wav
```

Then enable it in TeamSpeak under **Tools → Options → Addons → Plugins**.

To confirm the version actually loaded, check the TeamSpeak client log for
`Lima Charlie <version> started`, or look at the `pluginVersion` field in
`plugin_state.json` (see [troubleshooting.md](troubleshooting.md)).

## TeamSpeak voice activation

**Do not use TeamSpeak's own push-to-talk.** The plugin opens and closes your microphone itself, following
the game's transmit keys, by toggling `CLIENT_INPUT_DEACTIVATED`. If TeamSpeak is also gating your mic on a
key, the two fight and you transmit nothing.

Set TeamSpeak to **Continuous Transmission**, or to **Voice Activation Detection** if you would rather have
a noise gate. Either works; the plugin still decides when the mic is live.

## Channel handling

The plugin moves you into the configured TeamSpeak channel when you join the game, and back to wherever you
were when you leave. The channel is named by the server, and can be password protected.

You do not need to be in the right channel beforehand — joining the game moves you. If the channel does not
exist, no move happens and you stay put, which means you will not hear anyone.

## Sessions and cross-server safety

The server issues a session token, and plugins only pair TeamSpeak clients reporting the same token. Two
groups playing on different Reforger servers can share one TeamSpeak channel without hearing each other.

The token changes every time the server session restarts.

## For server operators

The TeamSpeak server, channel and password are set server-side and pushed to every client. Two places to
put them, in priority order:

1. **The mission header** — per scenario, overrides everything else.
2. **`Configs/LC/Settings.conf`** — shipped with the mod, the normal place.

A legacy third option still works: `$profile/LimaCharlie/server.json` with `teamspeakServer`, `teamspeakChannel`
and `teamspeakChannelPassword`. It is only read when the configured channel is empty, and logs a warning
each launch telling you to move it into `Settings.conf`. It exists so servers set up before the settings
config existed keep working.

See [server-settings.md](server-settings.md) for the full list.

## Updating

The mod and plugin must move together. `lc_game_state_parse` rejects any file whose protocol version it
does not recognise, so a mismatched pair does not degrade — it stops working entirely, in both directions.
When you publish a mod update that changes the protocol, every player needs the matching plugin before they
can play.
