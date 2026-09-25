# Lima Charlie

TeamSpeak 3 voice integration for Arma Reforger: positional direct speech, and radios that garble with
distance and terrain instead of cutting out cleanly.

Two halves that must be installed and updated together:

| Half | What it is | Where it lives |
| --- | --- | --- |
| The mod | Enfusion addon, published to the Workshop | `ArmaReforgerWorkbench/addons/Lima Charlie` |
| The plugin | TeamSpeak 3 plugin, plain C, Windows only | `Documents/Claude/Lima Charlie/plugin` |

They talk through two JSON files in the Reforger profile directory. Neither one works alone.

**Current versions:** mod protocol 9, plugin 1.0.11. A version mismatch is rejected outright, so everyone in
a session needs the same pair.

## Documentation

| Page | For |
| --- | --- |
| [installation.md](installation.md) | Getting the plugin installed and TeamSpeak configured |
| [server-setup.md](server-setup.md) | Standing up a server: TeamSpeak channel, mod, settings |
| [player-guide.md](player-guide.md) | Controls, radios, what you hear and why |
| [server-settings.md](server-settings.md) | Every server setting and how to set it |
| [how-it-works.md](how-it-works.md) | The audio model, terrain, AI hearing, Game Master |
| [developer-reference.md](developer-reference.md) | Bridge protocols, script map, building the plugin |
| [troubleshooting.md](troubleshooting.md) | Diagnostics and known problems |

## Design in one paragraph

The trust model is TFAR's: every client tells its own plugin where it is and what radios it carries, and
each receiving plugin decides for itself what it can hear. Nothing is authoritative. The game never handles
voice audio — TeamSpeak carries it, and the plugin edits the incoming streams per speaker before they are
mixed. The server's only job is to hand out a session token and a set of gameplay settings.

## Status

Built and tested by hand rather than by any automated suite on the game side. The plugin has offline tests
(`build/lc_tests.exe`, run by every build). Anything below marked *untested* has been written but never
confirmed in a live session.
