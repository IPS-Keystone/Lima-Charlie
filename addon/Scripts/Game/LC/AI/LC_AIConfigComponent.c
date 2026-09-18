modded class SCR_AIConfigComponent
{
	//------------------------------------------------------------------------------------------------
	//! Voice events are handled here rather than in the reaction table, which lives on AI prefabs
	override bool PerformDangerReaction(SCR_AIUtilityComponent utility, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		LC_VoiceDangerEvent voiceEvent = LC_VoiceDangerEvent.Cast(dangerEvent);
		if (voiceEvent && utility)
			return LC_AIHearing.React(utility, voiceEvent);

		return super.PerformDangerReaction(utility, dangerEvent, dangerEventCount);
	}
}
