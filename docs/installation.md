# Installation

## What every player needs

1. **TeamSpeak 3** — not TeamSpeak 5/6. The plugin is built against the TS3 plugin SDK.
2. **The Lima Charlie plugin**, installed into TeamSpeak.
3. **The mod**, subscribed from the Workshop.

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

## Your microphone

**Whatever you already use works.** Voice activation, continuous transmission, or TeamSpeak's own
push-to-talk: the plugin no longer touches your microphone. TeamSpeak decides when you are talking; the mod
only decides where that voice goes — to the people around you, and to a radio while you hold one of its
keys.

That means **you do not need an in-game key to speak to the people next to you**. Talk as you would on any
TeamSpeak channel and the people near you hear it, positioned and muffled by where you both are. The
in-game transmit keys are only for radios.

Earlier versions gated the microphone on the game's keys, which is why they demanded continuous or voice
activation. That restriction is gone.

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

The channel and its password are set server-side and pushed to every client. Players are moved into that
channel on whichever TeamSpeak server they are already connected to; the mod never connects anyone to a
TeamSpeak server. The channel defaults to **`LimaCharlie`**, so create a channel of that name and it works
untouched. Three places to change it, each overriding the one before:

1. **`Configs/LC/Settings.conf`** — shipped with the mod.
2. **The mission header** — per scenario.
3. **`$profile/LimaCharlie/server.json`** — the last word, and the only one you can edit without
   rebuilding the mod. It holds every setting, and is written out in full the first session it is missing,
   so there is always a complete file to edit.

Setting the channel to empty disables channel moves and leaves everyone where they are.

See [server-setup.md](server-setup.md) for standing a server up end to end, and
[server-settings.md](server-settings.md) for what each setting does.

## Updating

The mod and plugin must move together. `lc_game_state_parse` rejects any file whose protocol version it
does not recognise, so a mismatched pair does not degrade — it stops working entirely, in both directions.
When you publish a mod update that changes the protocol, every player needs the matching plugin before they
can play.
