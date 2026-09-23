//------------------------------------------------------------------------------------------------
//! Server side Lima Charlie settings. Defaults ship in Configs/LC/Settings.conf; a scenario can override them
//! for itself through the mission header (see LC_MissionHeader).
//!
//! These are read on the server and sent to each client with the session, so nothing here is safe to
//! read on a remote client.
[BaseContainerProps(configRoot: true)]
class LC_Settings
{
	protected static const ResourceName CONFIG_PATH = "{6A5C69DD20000060}Configs/LC/Settings.conf";

	[Attribute(defvalue: "35", uiwidget: UIWidgets.Slider, params: "0 95 5", desc: "How much of a radio's range stays perfectly clear, as a percentage. Past this the signal garbles more and more out to the edge of range, which is unchanged. Lower values mean garbled radios much sooner, higher values keep them clear almost to the edge.")]
	float m_fCleanRangePercent;

	[Attribute(defvalue: "90", uiwidget: UIWidgets.Slider, params: "0 100 5", desc: "How far out you still hear the beeps at the start and end of someone else's transmission, as a percentage of radio range. Below 100 a transmission from the far edge arrives as speech with no beeps, so a beep means the signal is worth listening to. 0 silences other people's beeps entirely.")]
	float m_fBeepRangePercent;

	[Attribute(defvalue: "100", uiwidget: UIWidgets.Slider, params: "0 300 10", desc: "How strongly hills and ridges between two radios cut the signal, as a percentage. 100 is the TFAR model, 0 ignores terrain entirely so only distance matters, 200 makes a ridge twice as costly. Terrain shortens the real range as well as garbling sooner.")]
	float m_fTerrainEffectPercent;

	[Attribute(desc: "Named frequencies. Each entry gives a frequency in MHz a name and a colour, shown beside it on the VON display whenever anyone transmits or receives on it. Empty by default; add an entry per net you want named.")]
	ref array<ref LC_ChannelLabel> m_aChannelLabels;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LC_EChannelNaming), desc: "Where channel names come from. LC_ONLY uses the named frequencies above and leaves everything else unnamed. HYBRID falls back to the game's own names, which label the platoon net, each playable group by its callsign and Conflict task nets. VANILLA_ONLY ignores the list above entirely.")]
	LC_EChannelNaming m_eChannelNaming;

	[Attribute(defvalue: "1", desc: "Game Masters transmit at unlimited range while the editor is open, ignoring distance and terrain, so they can reach anyone tuned to the frequency from wherever the camera is. Receiving is unaffected: a Game Master hears radios by the normal rules. Turn off to hold them to the same range as everyone else.")]
	bool m_bGameMasterUnlimitedRange;

	[Attribute(defvalue: "1", desc: "AI turn towards players they hear speaking out loud. Radio traffic is never audible to AI, and friendly AI ignore voices either way.")]
	bool m_bAIHearing;

	[Attribute(defvalue: "0", desc: "Troubleshooting only. Every client writes a line a second to its console log: what the game told the plugin, and what the plugin reports hearing back. Leave off for normal play: with a full server it is several kilobytes a second of log on every client.")]
	bool m_bDiagnosticLog;

	[Attribute(defvalue: "0", desc: "Troubleshooting only. Every client logs one line a second about the engine's room model: which room it thinks each nearby player is in, how open the doorways of the building you are in are, how much of the mapping of that building type is done, and whether each player's muffling came from the rooms or from a trace. Far less noisy than the setting above.")]
	bool m_bRoomDiagnosticLog;

	[Attribute(defvalue: "", desc: "TeamSpeak server players are moved to. Leave empty to leave everyone on the server they are already connected to.")]
	string m_sTeamSpeakServer;

	[Attribute(defvalue: "", desc: "TeamSpeak channel every player is moved into while in game. Leave empty to disable automatic channel moves.")]
	string m_sTeamSpeakChannel;

	[Attribute(defvalue: "", desc: "Password of that channel, if it has one.")]
	string m_sTeamSpeakChannelPassword;

	//------------------------------------------------------------------------------------------------
	//! Server only. The mission header wins, then the config shipped with the mod, then the attribute
	//! defaults. Resolved once per session rather than cached for the launch, so a scenario change is
	//! picked up without restarting.
	static LC_Settings Get()
	{
		LC_Settings settings = GetFromMissionHeader();
		if (settings)
			return settings;

		settings = SCR_ConfigHelperT<LC_Settings>.GetConfigObject(CONFIG_PATH);
		if (settings)
			return settings;

		Print("[LC] Could not load " + CONFIG_PATH + "; using built in defaults", LogLevel.WARNING);
		return CreateDefaults();
	}

	//------------------------------------------------------------------------------------------------
	//! The settings this scenario carries, or null when it carries none
	protected static LC_Settings GetFromMissionHeader()
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return null;

		LC_Settings settings = header.LC_GetSettings();
		if (settings)
			Print("[LC] Settings taken from the mission header", LogLevel.NORMAL);

		return settings;
	}

	//------------------------------------------------------------------------------------------------
	//! Attribute defaults only apply to containers the engine deserialises, so a hand made one sets its own
	static LC_Settings CreateDefaults()
	{
		LC_Settings settings = new LC_Settings();
		settings.m_fCleanRangePercent = 35;
		settings.m_fBeepRangePercent = 90;
		settings.m_fTerrainEffectPercent = 100;
		settings.m_bGameMasterUnlimitedRange = true;
		settings.m_bAIHearing = true;
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	//! Share of a radio's range that stays clean, as the plugin wants it
	float GetCleanFraction()
	{
		return Math.Clamp(m_fCleanRangePercent, 0.0, 95.0) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! Share of a radio's range within which other people's transmission beeps are heard
	float GetBeepFraction()
	{
		return Math.Clamp(m_fBeepRangePercent, 0.0, 100.0) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! The named frequencies, flattened for the trip to each client
	string GetPackedChannelLabels()
	{
		return LC_ChannelLabels.Pack(m_aChannelLabels);
	}

	//------------------------------------------------------------------------------------------------
	//! Multiplier on the terrain the signal has to climb over
	float GetTerrainFactor()
	{
		return Math.Clamp(m_fTerrainEffectPercent, 0.0, 300.0) / 100;
	}
}
