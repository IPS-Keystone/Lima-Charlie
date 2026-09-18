//------------------------------------------------------------------------------------------------
//! What the server tells every client about this session's setup. LC_Settings is the source of truth;
//! $profile:LimaCharlie/server.json is still read when it leaves the TeamSpeak channel empty, so servers set up
//! before the settings config existed keep working.
class LC_ServerSettings
{
	protected static const string PATH = "$profile:LimaCharlie/server.json";

	string m_sTeamSpeakServer;
	string m_sTeamSpeakChannel;
	string m_sTeamSpeakChannelPassword;
	//! Share of a radio's range that is heard perfectly before garbling starts
	float m_fCleanFraction = 0.35;
	//! Multiplier on terrain clearance, so ridges cost more or less range
	float m_fTerrainFactor = 1;
	//! Share of range within which other people's transmission beeps are heard
	float m_fBeepFraction = 0.9;
	//! Game Masters transmit without a range or terrain limit while the editor is open
	bool m_bGameMasterUnlimitedRange = true;
	bool m_bAIHearing = true;
	//! Troubleshooting: every client logs what it sends the plugin and what it hears back
	bool m_bDiagnosticLog;
	//! Named frequencies, packed as "frequencyKHz,colour,name;" for the session RPC
	string m_sChannelLabels;
	//! Where channel names come from: the list above, the game's own, or both
	LC_EChannelNaming m_eChannelNaming = LC_EChannelNaming.HYBRID;

	//------------------------------------------------------------------------------------------------
	//! Server only
	static LC_ServerSettings Resolve()
	{
		LC_ServerSettings settings = new LC_ServerSettings();

		LC_Settings configured = LC_Settings.Get();
		if (configured)
		{
			settings.m_sTeamSpeakServer = configured.m_sTeamSpeakServer;
			settings.m_sTeamSpeakChannel = configured.m_sTeamSpeakChannel;
			settings.m_sTeamSpeakChannelPassword = configured.m_sTeamSpeakChannelPassword;
			settings.m_fCleanFraction = configured.GetCleanFraction();
			settings.m_fTerrainFactor = configured.GetTerrainFactor();
			settings.m_fBeepFraction = configured.GetBeepFraction();
			settings.m_bGameMasterUnlimitedRange = configured.m_bGameMasterUnlimitedRange;
			settings.m_bAIHearing = configured.m_bAIHearing;
			settings.m_bDiagnosticLog = configured.m_bDiagnosticLog;
			settings.m_sChannelLabels = configured.GetPackedChannelLabels();
			settings.m_eChannelNaming = configured.m_eChannelNaming;
		}

		if (settings.m_sTeamSpeakChannel.IsEmpty())
			settings.ReadLegacyFile();

		return settings;
	}

	//------------------------------------------------------------------------------------------------
	//! The old server.json, kept as a fallback. Missing is the normal case now, so it is not created.
	protected void ReadLegacyFile()
	{
		if (!FileIO.FileExists(PATH))
			return;

		JsonLoadContext load = new JsonLoadContext();
		if (!load.LoadFromFile(PATH))
		{
			Print("[LC] Could not parse " + PATH, LogLevel.ERROR);
			return;
		}

		string teamSpeakServer;
		string teamSpeakChannel;
		string teamSpeakChannelPassword;
		load.ReadValue("teamspeakServer", teamSpeakServer);
		load.ReadValue("teamspeakChannel", teamSpeakChannel);
		load.ReadValue("teamspeakChannelPassword", teamSpeakChannelPassword);

		if (teamSpeakChannel.IsEmpty())
			return;

		m_sTeamSpeakServer = teamSpeakServer;
		m_sTeamSpeakChannel = teamSpeakChannel;
		m_sTeamSpeakChannelPassword = teamSpeakChannelPassword;
		Print("[LC] TeamSpeak channel taken from " + PATH + "; set it in Configs/LC/Settings.conf instead", LogLevel.WARNING);
	}
}
