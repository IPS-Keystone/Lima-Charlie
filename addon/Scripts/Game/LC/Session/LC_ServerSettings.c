//------------------------------------------------------------------------------------------------
//! What the server tells every client about this session's setup, resolved on the server in layers, each
//! overriding the one before it:
//!
//!   1. the defaults below
//!   2. Configs/LC/Settings.conf, shipped with the mod
//!   3. the scenario's mission header, if it carries Lima Charlie settings
//!   4. $profile:LimaCharlie/server.json
//!
//! server.json has the last word because it is the only one of them a server operator can edit without
//! rebuilding and republishing the mod. Only the keys actually present in it override anything, so a file
//! holding one key changes one setting. It is written out in full the first time a session runs without
//! one, so there is always a complete file to edit.
class LC_ServerSettings
{
	protected static const string DIRECTORY = "$profile:LimaCharlie";
	protected static const string PATH = "$profile:LimaCharlie/server.json";

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
	//! Troubleshooting: every client logs what the engine's room model says about its neighbours
	bool m_bRoomDiagnosticLog;
	//! Named frequencies, packed as "frequencyKHz,colour,name;" for the session RPC
	string m_sChannelLabels;
	//! Where channel names come from: the list above, the game's own, or both
	LC_EChannelNaming m_eChannelNaming = LC_EChannelNaming.HYBRID;

	//------------------------------------------------------------------------------------------------
	//! Both diagnostic switches in one value, because the settings RPC is at the vanilla limit of eight
	//! arguments. Bit 0 is the bridge log, bit 1 the room model log.
	int GetDiagnosticFlags()
	{
		int flags;
		if (m_bDiagnosticLog)
			flags |= 1;

		if (m_bRoomDiagnosticLog)
			flags |= 2;

		return flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only
	static LC_ServerSettings Resolve()
	{
		LC_ServerSettings settings = new LC_ServerSettings();
		settings.ApplyConfig(LC_Settings.Get());
		settings.ApplyFile();
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers 2 and 3: LC_Settings.Get() has already picked the mission header over the shipped config
	protected void ApplyConfig(LC_Settings configured)
	{
		if (!configured)
			return;

		m_sTeamSpeakChannel = configured.m_sTeamSpeakChannel;
		m_sTeamSpeakChannelPassword = configured.m_sTeamSpeakChannelPassword;
		m_fCleanFraction = configured.GetCleanFraction();
		m_fTerrainFactor = configured.GetTerrainFactor();
		m_fBeepFraction = configured.GetBeepFraction();
		m_bGameMasterUnlimitedRange = configured.m_bGameMasterUnlimitedRange;
		m_bAIHearing = configured.m_bAIHearing;
		m_bDiagnosticLog = configured.m_bDiagnosticLog;
		m_bRoomDiagnosticLog = configured.m_bRoomDiagnosticLog;
		m_sChannelLabels = configured.GetPackedChannelLabels();
		m_eChannelNaming = configured.m_eChannelNaming;
	}

	//------------------------------------------------------------------------------------------------
	//! Layer 4. Every key is optional; the ones present win, and which ones did is logged so a setting
	//! that is not doing what the mod's config says is traceable to this file.
	protected void ApplyFile()
	{
		if (!FileIO.FileExists(PATH))
		{
			WriteTemplate();
			return;
		}

		JsonLoadContext load = new JsonLoadContext();
		if (!load.LoadFromFile(PATH))
		{
			Print("[LC] Could not parse " + PATH + "; its settings are ignored this session", LogLevel.ERROR);
			return;
		}

		string overridden;
		if (ReadString(load, "teamspeakChannel", m_sTeamSpeakChannel))
			overridden += " teamspeakChannel";

		if (ReadString(load, "teamspeakChannelPassword", m_sTeamSpeakChannelPassword))
			overridden += " teamspeakChannelPassword";

		if (ReadPercent(load, "cleanRangePercent", 0, 95, m_fCleanFraction))
			overridden += " cleanRangePercent";

		if (ReadPercent(load, "beepRangePercent", 0, 100, m_fBeepFraction))
			overridden += " beepRangePercent";

		if (ReadPercent(load, "terrainEffectPercent", 0, 300, m_fTerrainFactor))
			overridden += " terrainEffectPercent";

		if (ReadBool(load, "gameMasterUnlimitedRange", m_bGameMasterUnlimitedRange))
			overridden += " gameMasterUnlimitedRange";

		if (ReadBool(load, "aiHearing", m_bAIHearing))
			overridden += " aiHearing";

		if (ReadBool(load, "diagnosticLog", m_bDiagnosticLog))
			overridden += " diagnosticLog";

		if (ReadBool(load, "roomDiagnosticLog", m_bRoomDiagnosticLog))
			overridden += " roomDiagnosticLog";

		if (ReadNaming(load, "channelNaming"))
			overridden += " channelNaming";

		if (ReadChannelLabels(load, "channelLabels"))
			overridden += " channelLabels";

		if (overridden.IsEmpty())
			return;

		Print("[LC] server.json overrides:" + overridden, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReadString(notnull JsonLoadContext load, string key, out string value)
	{
		if (!load.DoesKeyExist(key))
			return false;

		string read;
		if (!load.ReadValue(key, read))
			return false;

		value = read;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReadBool(notnull JsonLoadContext load, string key, out bool value)
	{
		if (!load.DoesKeyExist(key))
			return false;

		bool read;
		if (!load.ReadValue(key, read))
			return false;

		value = read;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The file holds percentages, matching the mod's config; everything downstream wants a fraction
	protected static bool ReadPercent(notnull JsonLoadContext load, string key, float minimum, float maximum, out float fraction)
	{
		if (!load.DoesKeyExist(key))
			return false;

		float percent;
		if (!load.ReadValue(key, percent))
			return false;

		fraction = Math.Clamp(percent, minimum, maximum) / 100;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "LC_ONLY", "HYBRID" or "VANILLA_ONLY", or the number of one of them
	protected bool ReadNaming(notnull JsonLoadContext load, string key)
	{
		if (!load.DoesKeyExist(key))
			return false;

		string read;
		if (!load.ReadValue(key, read))
			return false;

		read.TrimInPlace();
		if (read.Compare("LC_ONLY", false) == 0 || read == "0")
		{
			m_eChannelNaming = LC_EChannelNaming.LC_ONLY;
			return true;
		}

		if (read.Compare("HYBRID", false) == 0 || read == "1")
		{
			m_eChannelNaming = LC_EChannelNaming.HYBRID;
			return true;
		}

		if (read.Compare("VANILLA_ONLY", false) == 0 || read == "2")
		{
			m_eChannelNaming = LC_EChannelNaming.VANILLA_ONLY;
			return true;
		}

		Print("[LC] server.json channelNaming '" + read + "' is not LC_ONLY, HYBRID or VANILLA_ONLY; ignored", LogLevel.WARNING);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! "45.5,RED,COMMAND;38,GREEN,MEDEVAC" - megahertz, colour name, channel name. An empty string is a
	//! deliberate "no named channels", which is why it counts as an override like any other value.
	protected bool ReadChannelLabels(notnull JsonLoadContext load, string key)
	{
		if (!load.DoesKeyExist(key))
			return false;

		string read;
		if (!load.ReadValue(key, read))
			return false;

		m_sChannelLabels = LC_ChannelLabels.PackFromText(read);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the settings this session resolved, so an operator has every key in front of them rather
	//! than having to look them up. Written once: from then on the file is the last word, so a later
	//! change to the mod's own config will not move a setting this file already pins.
	//!
	//! Written by hand rather than through JsonSaveContext, which puts the whole object on one line. This
	//! file exists to be edited, so it gets one setting per line, in the order the settings are documented.
	protected void WriteTemplate()
	{
		FileIO.MakeDirectory(DIRECTORY);

		FileHandle file = FileIO.OpenFile(PATH, FileMode.WRITE);
		if (!file)
		{
			Print("[LC] Could not write " + PATH, LogLevel.WARNING);
			return;
		}

		// Whole percentages, so the file reads as something a person wrote
		int cleanPercent = Math.Round(m_fCleanFraction * 100);
		int beepPercent = Math.Round(m_fBeepFraction * 100);
		int terrainPercent = Math.Round(m_fTerrainFactor * 100);

		array<string> settings = {};
		settings.Insert(TextSetting("teamspeakChannel", m_sTeamSpeakChannel));
		settings.Insert(TextSetting("teamspeakChannelPassword", m_sTeamSpeakChannelPassword));
		settings.Insert(NumberSetting("cleanRangePercent", cleanPercent));
		settings.Insert(NumberSetting("beepRangePercent", beepPercent));
		settings.Insert(NumberSetting("terrainEffectPercent", terrainPercent));
		settings.Insert(FlagSetting("gameMasterUnlimitedRange", m_bGameMasterUnlimitedRange));
		settings.Insert(FlagSetting("aiHearing", m_bAIHearing));
		settings.Insert(FlagSetting("diagnosticLog", m_bDiagnosticLog));
		settings.Insert(FlagSetting("roomDiagnosticLog", m_bRoomDiagnosticLog));
		settings.Insert(TextSetting("channelNaming", NamingName(m_eChannelNaming)));
		settings.Insert(TextSetting("channelLabels", LC_ChannelLabels.UnpackToText(m_sChannelLabels)));

		file.WriteLine("{");
		int last = settings.Count() - 1;
		for (int i = 0; i <= last; i++)
		{
			// Every line but the last carries the separating comma, so the file stays valid JSON
			string line = "    " + settings[i];
			if (i < last)
				line += ",";

			file.WriteLine(line);
		}

		file.WriteLine("}");
		file.Close();

		Print("[LC] Wrote " + PATH + " with this session's settings; edit it to override the mod's config", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static string TextSetting(string key, string value)
	{
		return "\"" + key + "\": \"" + Escape(value) + "\"";
	}

	//------------------------------------------------------------------------------------------------
	protected static string NumberSetting(string key, int value)
	{
		return "\"" + key + "\": " + value.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected static string FlagSetting(string key, bool value)
	{
		string text = "false";
		if (value)
			text = "true";

		return "\"" + key + "\": " + text;
	}

	//------------------------------------------------------------------------------------------------
	//! A channel name or label can hold anything a person typed into the mod's config
	protected static string Escape(string value)
	{
		string escaped = value;
		escaped.Replace("\\", "\\\\");
		escaped.Replace("\"", "\\\"");
		return escaped;
	}

	//------------------------------------------------------------------------------------------------
	protected static string NamingName(LC_EChannelNaming naming)
	{
		if (naming == LC_EChannelNaming.LC_ONLY)
			return "LC_ONLY";

		if (naming == LC_EChannelNaming.VANILLA_ONLY)
			return "VANILLA_ONLY";

		return "HYBRID";
	}
}
