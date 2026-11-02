modded class SCR_BaseGameMode
{
	//! Radio leases as (radio RplId, player id, kind) triples, written only by the server's LC_RadioLeases.
	//! On the game mode because every machine has it and it is always streamed in, so no radio prefab needs
	//! a component of ours for its holders to be known.
	[RplProp(onRplName: "LC_OnRadioLeasesReplicated")]
	protected ref array<int> m_aLC_RadioLeases = {};

	protected ref LC_RadioLeases m_LC_RadioLeases;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Fresh token and settings for every session, including repeated Workbench play sessions
		if (Replication.IsServer())
			LC_Session.StartServerSession();
	}

	//------------------------------------------------------------------------------------------------
	LC_RadioLeases LC_GetRadioLeases()
	{
		if (!m_LC_RadioLeases)
			m_LC_RadioLeases = new LC_RadioLeases(this);

		return m_LC_RadioLeases;
	}

	//------------------------------------------------------------------------------------------------
	array<int> LC_GetRadioLeaseState()
	{
		return m_aLC_RadioLeases;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the lease state was rewritten. The onRpl handler never runs on the authority, so a host's own
	//! player is told here.
	void LC_PublishRadioLeases()
	{
		Replication.BumpMe();
		LC_GetRadioLeases().OnStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void LC_OnRadioLeasesReplicated()
	{
		LC_GetRadioLeases().OnStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer() && m_LC_RadioLeases)
			m_LC_RadioLeases.ServerReleasePlayer(playerId, "disconnected");

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled(playerId, playerEntity, killerEntity, killer);

		if (Replication.IsServer() && m_LC_RadioLeases)
			m_LC_RadioLeases.ServerReleasePlayer(playerId, "holder dead");
	}

	//------------------------------------------------------------------------------------------------
	//! The kill path a game mode component took over; leases end there too
	override protected void OnPlayerKilledHandled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilledHandled(playerId, playerEntity, killerEntity, killer);

		if (Replication.IsServer() && m_LC_RadioLeases)
			m_LC_RadioLeases.ServerReleasePlayer(playerId, "holder dead");
	}
}
