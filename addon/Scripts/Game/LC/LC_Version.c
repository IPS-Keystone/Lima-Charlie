//------------------------------------------------------------------------------------------------
//! The mod's own version, sent to the plugin and shown on a player's TeamSpeak info panel beside the
//! plugin version, so a mismatched pair is visible without reading a log.
//!
//! Bump this with the Workshop version. The bridge protocol is versioned separately in LC_GameStateWriter,
//! because a release can change one without the other.
class LC_Version
{
	static const string VERSION = "1.0.11";
}
