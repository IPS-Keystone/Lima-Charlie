//------------------------------------------------------------------------------------------------
//! Server-side session: a token unique to this game session plus the server settings.
//! Plugins only pair TeamSpeak clients that report the same token, so players from different
//! Reforger servers sharing a TeamSpeak channel never hear each other.
class LC_Session
{
	protected static ref LC_Session s_ServerSession;

	protected string m_sToken;
	protected ref LC_ServerSettings m_Settings;

	//------------------------------------------------------------------------------------------------
	//! Starts a fresh session. Called on the server when the game mode initialises.
	static void StartServerSession()
	{
		s_ServerSession = new LC_Session();
		s_ServerSession.m_sToken = GenerateToken();
		Print("[LC] Session started", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static LC_Session GetServerSession()
	{
		if (!s_ServerSession)
			StartServerSession();

		return s_ServerSession;
	}

	//------------------------------------------------------------------------------------------------
	string GetToken()
	{
		return m_sToken;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolved on first use rather than at session start, so anything that supplies settings (a compat
	//! addon, a mission header) has finished loading by the time we read them
	LC_ServerSettings GetSettings()
	{
		if (!m_Settings)
		{
			m_Settings = LC_ServerSettings.Resolve();
			int cleanPercent = Math.Round(m_Settings.m_fCleanFraction * 100);
			int terrainPercent = Math.Round(m_Settings.m_fTerrainFactor * 100);
			int beepPercent = Math.Round(m_Settings.m_fBeepFraction * 100);
			Print(string.Format("[LC] Settings: TeamSpeak channel '%1', clean radio range %2 percent, beep range %3 percent, terrain effect %4 percent, AI hearing %5",
				m_Settings.m_sTeamSpeakChannel, cleanPercent, beepPercent, terrainPercent, m_Settings.m_bAIHearing), LogLevel.NORMAL);
		}

		return m_Settings;
	}

	//------------------------------------------------------------------------------------------------
	//! Unix time plus 48 random bits. Math.RandomInt misbehaves with large ranges (it produced negative
	//! values for 0..1e9), so the random part is built from 16-bit pieces.
	protected static string GenerateToken()
	{
		string token = System.GetUnixTime().ToString();
		for (int i = 0; i < 3; i++)
		{
			token += "-" + Math.RandomInt(0, 65536).ToString();
		}

		return token;
	}
}
