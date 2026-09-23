modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (!System.IsConsoleApp() && GetGame().GetPlayerController() == this)
			LC_Client.Start(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Client asks the server for the session token and TeamSpeak settings
	void LC_RequestSession()
	{
		Rpc(RpcAsk_LC_RequestSession);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LC_RequestSession()
	{
		LC_Session session = LC_Session.GetServerSession();
		LC_ServerSettings settings = session.GetSettings();
		Rpc(RpcDo_LC_ReceiveSettings, settings.m_fCleanFraction, settings.m_fBeepFraction, settings.m_fTerrainFactor, settings.m_bAIHearing, settings.m_bGameMasterUnlimitedRange, settings.GetDiagnosticFlags(), settings.m_sChannelLabels, settings.m_eChannelNaming);
		Rpc(RpcDo_LC_ReceiveSession, session.GetToken(), settings.m_sTeamSpeakChannel, settings.m_sTeamSpeakChannelPassword);
	}

	//------------------------------------------------------------------------------------------------
	//! Client reports that it is speaking out loud, so AI near the character can notice the noise
	void LC_ReportVoice(float radius)
	{
		Rpc(RpcAsk_LC_ReportVoice, radius);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void RpcAsk_LC_ReportVoice(float radius)
	{
		if (!LC_Session.GetServerSession().GetSettings().m_bAIHearing)
			return;

		IEntity character = GetControlledEntity();
		if (character)
			LC_AIHearing.Broadcast(character, radius);
	}

	//------------------------------------------------------------------------------------------------
	//! Gameplay settings the server owns, sent just before the session so they are in place when the
	//! client starts writing radio state for the plugin
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LC_ReceiveSettings(float cleanFraction, float beepFraction, float terrainFactor, bool aiHearing, bool gameMasterUnlimitedRange, int diagnosticFlags, string channelLabels, int channelNaming)
	{
		LC_Client client = LC_Client.Get();
		if (client)
			client.OnSettingsReceived(cleanFraction, beepFraction, terrainFactor, aiHearing, gameMasterUnlimitedRange, diagnosticFlags, channelLabels, channelNaming);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LC_ReceiveSession(string token, string teamSpeakChannel, string teamSpeakChannelPassword)
	{
		LC_Client client = LC_Client.Get();
		if (client)
			client.OnSessionReceived(token, teamSpeakChannel, teamSpeakChannelPassword);
	}
}
