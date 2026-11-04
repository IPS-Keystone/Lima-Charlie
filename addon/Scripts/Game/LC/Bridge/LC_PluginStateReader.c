//------------------------------------------------------------------------------------------------
//! One incoming radio transmission the plugin is playing, and which of our radios it arrives on
class LC_HeardTransmission
{
	string m_sRadioId;
	int m_iFrequency;
	float m_fQuality;
	//! Regular diagnostics: when it was first heard, and the worst quality since
	int m_iStartTick;
	float m_fWorstQuality;
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
	//! Different states read this launch. A file left over from an earlier session reads once and then
	//! never changes, so one state alone does not mean the plugin is running.
	protected int m_iSeqsRead;
	protected int m_iNextDiagnosticTick;

	protected string m_sPluginVersion;
	protected bool m_bTeamSpeakConnected;
	protected int m_iTeamSpeakClientId;
	protected bool m_bInGameChannel;
	protected int m_iPeers;
	protected bool m_bSelfTalking;
	//! TeamSpeak is holding the microphone shut: its local mute, its mute toggle, or a muted speaker
	protected bool m_bMicMuted;
	protected ref set<int> m_TalkingPlayers = new set<int>();
	//! Transmitting player id -> position the plugin needs the terrain clearance from
	protected ref map<int, vector> m_mRadioRequests = new map<int, vector>();
	//! Transmitting player id -> the radio we are hearing them on, for the VON display
	protected ref map<int, ref LC_HeardTransmission> m_mHeard = new map<int, ref LC_HeardTransmission>();
	//! The previous read's transmissions, swapped with m_mHeard so a start and an end can be told apart
	protected ref map<int, ref LC_HeardTransmission> m_mHeardBefore = new map<int, ref LC_HeardTransmission>();
	//! Regular diagnostics
	protected bool m_bDiagnostics;
	protected bool m_bDiagRunning;
	protected int m_iDiagStates;
	protected int m_iDiagReceptions;
	protected ref array<string> m_aSplit = {};
	protected ref array<string> m_aFields = {};

	//------------------------------------------------------------------------------------------------
	//! \param regular logs radio receptions starting and ending, and the plugin going quiet or coming back
	//! \param comprehensive also logs everything the plugin reports, once a second
	void Update(int now, bool regular = false, bool comprehensive = false)
	{
		m_bDiagnostics = regular;
		if (regular)
		{
			bool running = IsPluginRunning(now);
			if (running != m_bDiagRunning)
			{
				m_bDiagRunning = running;
				if (running)
					Print("[LC] Plugin answering", LogLevel.NORMAL);
				else if (m_iLastSeq >= 0)
					Print("[LC] Plugin stopped answering: no new plugin_state for " + STALE_MS.ToString() + " ms", LogLevel.WARNING);
			}
		}

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
		if (m_iSeqsRead < 2)
			m_iSeqsRead++;
		m_iDiagStates++;

		string pluginVersion;
		bool teamSpeakConnected;
		int teamSpeakClientId;
		bool inGameChannel;
		int peers;
		bool selfTalking;
		bool micMuted;
		string talking;
		string radioRx;
		string radioHeard;
		load.ReadValue("pluginVersion", pluginVersion);
		load.ReadValue("tsConnected", teamSpeakConnected);
		load.ReadValue("tsClientId", teamSpeakClientId);
		load.ReadValue("inGameChannel", inGameChannel);
		load.ReadValue("peers", peers);
		load.ReadValue("selfTalking", selfTalking);
		load.ReadValue("micMuted", micMuted);
		load.ReadValue("talking", talking);
		load.ReadValue("radioRx", radioRx);
		load.ReadValue("radioHeard", radioHeard);

		if (pluginVersion != m_sPluginVersion || teamSpeakConnected != m_bTeamSpeakConnected || teamSpeakClientId != m_iTeamSpeakClientId || inGameChannel != m_bInGameChannel || peers != m_iPeers || micMuted != m_bMicMuted)
		{
			Print(string.Format("[LC] Plugin %1: TeamSpeak connected=%2, client id=%3, in game channel=%4, peers=%5, mic muted=%6", pluginVersion, teamSpeakConnected, teamSpeakClientId, inGameChannel, peers, micMuted), LogLevel.NORMAL);
		}

		m_sPluginVersion = pluginVersion;
		m_bTeamSpeakConnected = teamSpeakConnected;
		m_iTeamSpeakClientId = teamSpeakClientId;
		m_bInGameChannel = inGameChannel;
		m_iPeers = peers;
		m_bSelfTalking = selfTalking;
		m_bMicMuted = micMuted;
		UpdateTalkingPlayers(talking);
		UpdateRadioRequests(radioRx);
		UpdateHeard(radioHeard, now);
		LogDiagnostic(now, comprehensive, peers, selfTalking, talking, radioRx, radioHeard);
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
	protected void UpdateHeard(string radioHeard, int now)
	{
		map<int, ref LC_HeardTransmission> before = m_mHeardBefore;
		m_mHeardBefore = m_mHeard;
		m_mHeard = before;
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

			if (!m_bDiagnostics)
				continue;

			// Carried over from the previous read while it is the same transmission on the same radio
			LC_HeardTransmission previous = m_mHeardBefore.Get(playerId);
			if (previous && previous.m_sRadioId == heard.m_sRadioId && previous.m_iFrequency == heard.m_iFrequency)
			{
				heard.m_iStartTick = previous.m_iStartTick;
				heard.m_fWorstQuality = Math.Min(previous.m_fWorstQuality, heard.m_fQuality);
				continue;
			}

			heard.m_iStartTick = now;
			heard.m_fWorstQuality = heard.m_fQuality;
			m_iDiagReceptions++;
			Print(string.Format("[LC] rx start player %1 on radio %2 %3 kHz, quality %4", playerId, heard.m_sRadioId, heard.m_iFrequency, heard.m_fQuality.ToString(-1, 2)), LogLevel.NORMAL);
		}

		if (!m_bDiagnostics)
			return;

		foreach (int endedId, LC_HeardTransmission ended : m_mHeardBefore)
		{
			LC_HeardTransmission still = m_mHeard.Get(endedId);
			if (still && still.m_sRadioId == ended.m_sRadioId && still.m_iFrequency == ended.m_iFrequency)
				continue;

			int length = now - ended.m_iStartTick;
			Print(string.Format("[LC] rx end player %1 on radio %2 after %3 ms, worst quality %4", endedId, ended.m_sRadioId, length, ended.m_fWorstQuality.ToString(-1, 2)), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Regular diagnostics: plugin states read and radio receptions started since the last call
	void TakeStats(out int states, out int receptions)
	{
		states = m_iDiagStates;
		receptions = m_iDiagReceptions;
		m_iDiagStates = 0;
		m_iDiagReceptions = 0;
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
		return m_iSeqsRead >= 2 && now - m_iLastSeqTick < STALE_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether TeamSpeak is holding our own microphone shut, so nobody can hear us whatever we do in game
	bool IsMicMuted()
	{
		return m_bMicMuted;
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
	//! Version the plugin last reported, empty until it has been heard from
	string GetPluginVersion()
	{
		return m_sPluginVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether TeamSpeak itself is connected to a server. Meaningless unless IsPluginRunning.
	bool IsTeamSpeakConnected()
	{
		return m_bTeamSpeakConnected;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the plugin has us in the channel the server asked for
	bool IsInGameChannel()
	{
		return m_bInGameChannel;
	}

	//------------------------------------------------------------------------------------------------
	//! Other Lima Charlie players the plugin has paired with in this channel
	int GetPeers()
	{
		return m_iPeers;
	}

	//------------------------------------------------------------------------------------------------
	//! True while the plugin hears this player talking in TeamSpeak
	bool IsPlayerTalking(int playerId)
	{
		return m_TalkingPlayers.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Changes whenever a new plugin state has been read, so readers of it can skip frames with nothing new
	int GetSeq()
	{
		return m_iLastSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Radio transmitters the plugin is receiving and needs terrain clearance for
	map<int, vector> GetRadioRequests()
	{
		return m_mRadioRequests;
	}
}
