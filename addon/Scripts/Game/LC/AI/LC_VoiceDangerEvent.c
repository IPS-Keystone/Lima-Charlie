//------------------------------------------------------------------------------------------------
//! Danger event broadcast when a player speaks out loud, so AI within earshot notice the noise.
//! The danger type is only there to satisfy the engine: LC_AIHearing handles the event before vanilla's
//! reaction table is consulted, and the door reaction it would otherwise hit ignores events without a door.
class LC_VoiceDangerEvent : AIDangerEvent
{
	protected float m_fRadius;

	//------------------------------------------------------------------------------------------------
	void SetRadius(float radius)
	{
		m_fRadius = radius;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}
}
