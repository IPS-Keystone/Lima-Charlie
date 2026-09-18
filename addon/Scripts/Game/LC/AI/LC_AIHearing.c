//------------------------------------------------------------------------------------------------
//! AI noticing players who speak. The server broadcasts a danger event where the speaker is; each AI that
//! receives it turns to look, the way vanilla AI react to a vehicle horn.
//!
//! Walls do not silence a voice, they shorten how far it carries: each AI shrinks the radius by what is in
//! the way between it and the speaker, so a shout still reaches the street outside while a normal voice
//! stops at the room next door.
class LC_AIHearing
{
	//! Speech never carries further than this, whatever the voice level
	protected static const float MAX_RADIUS_M = 120;
	//! Share of the radius a voice keeps through one wall, and through two or more
	protected static const float SINGLE_OBSTACLE_RANGE = 0.35;
	protected static const float MULTIPLE_OBSTACLE_RANGE = 0.15;

	protected static ref LC_Occlusion s_Occlusion;
	protected static ref array<IEntity> s_aExclude = {};

	//------------------------------------------------------------------------------------------------
	//! Server only
	static void Broadcast(notnull IEntity speaker, float radius)
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld || radius <= 0)
			return;

		LC_VoiceDangerEvent voiceEvent = new LC_VoiceDangerEvent();
		voiceEvent.SetDangerType(EAIDangerEventType.Danger_DoorMovement);
		voiceEvent.SetPosition(speaker.GetOrigin());
		voiceEvent.SetVictim(speaker);
		voiceEvent.SetRadius(Math.Min(radius, MAX_RADIUS_M));
		aiWorld.RequestBroadcastDangerEvent(voiceEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! One AI's reaction to hearing a voice. Returns true if it reacted.
	static bool React(notnull SCR_AIUtilityComponent utility, notnull LC_VoiceDangerEvent voiceEvent)
	{
		if (!utility.m_LookAction)
			return false;

		vector noisePosition = voiceEvent.GetPosition();
		if (vector.Distance(noisePosition, utility.GetOrigin()) > voiceEvent.GetRadius())
			return false;

		// Only an enemy voice is worth turning to; own side chatter is ignored
		SCR_ChimeraAIAgent agent = SCR_ChimeraAIAgent.Cast(utility.GetOwner());
		if (!agent || !agent.IsPerceivedEnemy(voiceEvent.GetVictim()))
			return false;

		if (!CarriesThroughCover(agent, voiceEvent))
			return false;

		utility.m_LookAction.LookAt(noisePosition, SCR_AILookAction.PRIO_DANGER_EVENT);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the voice still reaches this AI once what is between them is taken off its range. Only runs for
	//! AI already within the unobstructed radius, so it costs at most two traces per enemy nearby per second.
	protected static bool CarriesThroughCover(notnull SCR_ChimeraAIAgent agent, notnull LC_VoiceDangerEvent voiceEvent)
	{
		IEntity listener = agent.GetControlledEntity();
		IEntity speaker = voiceEvent.GetVictim();
		if (!listener || !speaker)
			return true;

		IEntity listenerVehicle = CompartmentAccessComponent.GetVehicleIn(listener);
		IEntity speakerVehicle = CompartmentAccessComponent.GetVehicleIn(speaker);
		if (listenerVehicle && listenerVehicle == speakerVehicle)
			return true;

		// Traced ear to ear rather than from the feet, so the ground between two standing characters is not cover
		vector listenerPosition = GetEarPosition(listener);
		vector speakerPosition = GetEarPosition(speaker);

		s_aExclude.Clear();
		s_aExclude.Insert(listener);
		s_aExclude.Insert(speaker);
		if (listenerVehicle)
			s_aExclude.Insert(listenerVehicle);

		if (speakerVehicle)
			s_aExclude.Insert(speakerVehicle);

		if (!s_Occlusion)
			s_Occlusion = new LC_Occlusion();

		int obstacles = s_Occlusion.CountObstacles(listenerPosition, speakerPosition, s_aExclude);

		// Sitting in a vehicle is a wall of its own, on top of anything the trace found
		if (listenerVehicle || speakerVehicle)
			obstacles++;

		if (obstacles <= 0)
			return true;

		float factor = MULTIPLE_OBSTACLE_RANGE;
		if (obstacles == 1)
			factor = SINGLE_OBSTACLE_RANGE;

		return vector.Distance(listenerPosition, speakerPosition) <= voiceEvent.GetRadius() * factor;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector GetEarPosition(notnull IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
			return character.EyePosition();

		return entity.GetOrigin();
	}
}
