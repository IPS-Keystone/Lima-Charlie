modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Fresh token and settings for every session, including repeated Workbench play sessions
		if (Replication.IsServer())
			LC_Session.StartServerSession();
	}
}
