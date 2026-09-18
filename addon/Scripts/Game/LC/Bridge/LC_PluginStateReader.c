//------------------------------------------------------------------------------------------------
//! One incoming radio transmission the plugin is playing, and which of our radios it arrives on
class LC_HeardTransmission
{
	string m_sRadioId;
	int m_iFrequency;
	float m_fQuality;
}

//------------------------------------------------------------------------------------------------
//! Reads $profile:LimaCharlie/plugin_state.json, which the TeamSpeak plugin rewrites at least every 250 ms
class LC_PluginStateReader
{
	protected static const string PATH = "$profile:LimaCharlie/plugin_state.json";
	//! Fast enough that radio terrain requests are answered well within the plugin's wait
	protected static const int POLL_MS = 50;
	//! No new plugin state for this long means the plugin is not running
	protected static const int STALE_MS = 3000;
	//! How often the plugin's own view goes to the log while diagnostics are on
	protected static const int DIAGNOSTIC_INTERVAL_MS = 1000;

	protected int m_iNextPollTick;
	protected int m_iLastSeq = -1;
	protected int m_iLastSeqTick;
	protected int m_iNextDiagnosticTick;

	protected string m_sPluginVersion;
	protected bool m_bTeamSpeakConnected;
	protected int m_iTeamSpeakClientId;
	protected bool m_bInGameChannel;
	protected int m_iPeers;
	protected bool m_bSelfTalking;
	protected ref set<int> m_TalkingPlayers = new set<int>();
	//! Transmitting player id -> position the plugin needs the terrain clearance from
	protected ref map<int, vector> m_mRadioRequests = new map<int, vector>();
	//! Transmitting player id -> the radio we are hearing them on, for the VON display
	protected ref map<int, ref LC_HeardTransmission> m_mHeard = new map<int, ref LC_HeardTransmission>();
	protected ref array<string> m_aSplit = {};
	protected ref array<string> m_aFields = {};

	//------------------------------------------------------------------------------------------------
	//! \param diagnostic logs what the plugin reports hearing, once a second
	void Update(int now, bool diagnostic = false)
	{
		if (now < m_iNextPollTick)
			return;

		m_iNextPollTick = now + POLL_MS;
		if (!FileIO.FileExists(PATH))
			return;

		JsonLoadContext load = new JsonLoadContext();
		if (!load.LoadFromFile(PATH))
			return;

		int seq;
		if (!load.ReadValue("seq", seq) || seq == m_iLastSeq)
			return;

		m_iLastSeq = seq;
		m_iLastSeqTick = now;

		string pluginVersion;
		bool teamSpeakConnected;
		int teamSpeakClientId;
		bool inGameChannel;
		int peers;
		bool selfTalking;
		string talking;
		string radioRx;
		string radioHeard;
		load.ReadValue("pluginVersion", pluginVersion);
		load.ReadValue("tsConnected", teamSpeakConnected);
		load.ReadValue("tsClientId", teamSpeakClientId);
		load.ReadValue("inGameChannel", inGameChannel);
		load.ReadValue("peers", peers);
		load.ReadValue("selfTalking", selfTalking);
		load.ReadValue("talking", talking);
		load.ReadValue("radioRx", radioRx);
		load.ReadValue("radioHeard", radioHeard);

		if (pluginVersion != m_sPluginVersion || teamSpeakConnected != m_bTeamSpeakConnected || teamSpeakClientId != m_iTeamSpeakClientId || inGameChannel != m_bInGameChannel || peers != m_iPeers)
		{
			Print(string.Format("[LC] Plugin %1: TeamSpeak connected=%2, client id=%3, in game channel=%4, peers=%5", pluginVersion, teamSpeakConnected, teamSpeakClientId, inGameChannel, peers), LogLevel.NORMAL);
		}

		m_sPluginVersion = pluginVersion;
		m_bTeamSpeakConnected = teamSpeakConnected;
		m_iTeamSpeakClientId = teamSpeakClientId;
		m_bInGameChannel = inGameChannel;
		m_iPeers = peers;
		m_bSelfTalking = selfTalking;
		UpdateTalkingPlayers(talking);
		UpdateRadioRequests(radioRx);
		UpdateHeard(radioHeard);
		LogDiagnostic(now, diagnostic, peers, selfTalking, talking, radioRx, radioHeard);
	}

	//------------------------------------------------------------------------------------------------
	//! The receiving half of the game_state dump. radioHeard lists every radio transmission this client is
	//! actually hearing, so a transmission that never appears there never reached this client at all.
	protected void LogDiagnostic(int now, bool enabled, int peers, bool selfTalking, string talking, string radioRx, string radioHeard)
	{
		if (!enabled)
		{
			m_iNextDiagnosticTick = 0;
			return;
		}

		if (now < m_iNextDiagnosticTick)
			return;

		m_iNextDiagnosticTick = now + DIAGNOSTIC_INTERVAL_MS;
		Print(string.Format("[LC] plugin_state peers=%1, selfTalking=%2, talking='%3', radioRx='%4', radioHeard='%5'", peers, selfTalking, talking, radioRx, radioHeard), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateTalkingPlayers(string talking)
	{
		m_TalkingPlayers.Clear();

		m_aSplit.Clear();
		talking.Split(",", m_aSplit, true);
		foreach (string id : m_aSplit)
		{
			m_TalkingPlayers.Insert(id.ToInt());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "playerId,x,y,z;playerId,x,y,z;"
	protected void UpdateRadioRequests(string radioRx)
	{
		m_mRadioRequests.Clear();

		m_aSplit.Clear();
		radioRx.Split(";", m_aSplit, true);
		foreach (string request : m_aSplit)
		{
			m_aFields.Clear();
			request.Split(",", m_aFields, false);
			if (m_aFields.Count() != 4)
				continue;

			int playerId = m_aFields[0].ToInt();
			if (playerId <= 0)
				continue;

			m_mRadioRequests.Set(playerId, Vector(m_aFields[1].ToFloat(), m_aFields[2].ToFloat(), m_aFields[3].ToFloat()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "playerId,radioId,frequency,quality;playerId,radioId,frequency,quality;"
	protected void UpdateHeard(string radioHeard)
	{
		m_mHeard.Clear();

		m_aSplit.Clear();
		radioHeard.Split(";", m_aSplit, true);
		foreach (string entry : m_aSplit)
		{
			m_aFields.Clear();
			entry.Split(",", m_aFields, false);
			if (m_aFields.Count() != 4)
				continue;

			int playerId = m_aFields[0].ToInt();
			if (playerId <= 0)
				continue;

			LC_HeardTransmission heard = new LC_HeardTransmission();
			heard.m_sRadioId = m_aFields[1];
			heard.m_iFrequency = m_aFields[2].ToInt();
			heard.m_fQuality = m_aFields[3].ToFloat();
			m_mHeard.Set(playerId, heard);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Radio transmissions we can currently hear, by transmitting player id
	map<int, ref LC_HeardTransmission> GetHeardTransmissions()
	{
		return m_mHeard;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPluginRunning(int now)
	{
		return m_iLastSeq >= 0 && now - m_iLastSeqTick < STALE_MS;
	}

	//------------------------------------------------------------------------------------------------
	bool IsSelfTalking()
	{
		return m_bSelfTalking;
	}

	//------------------------------------------------------------------------------------------------
	//! True while the plugin hears any peer talking in TeamSpeak
	bool IsAnyPlayerTalking()
	{
		return !m_TalkingPlayers.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Radio transmitters the plugin is receiving and needs terrain clearance for
	map<int, vector> GetRadioRequests()
	{
		return m_mRadioRequests;
	}
}
