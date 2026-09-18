//------------------------------------------------------------------------------------------------
//! How obstructed the path between a listener and a speaker is, as a muffle amount from 0 (clear) to 1.
//! The plugin turns it into a low-pass filter and some attenuation.
class LC_Occlusion
{
	//! One wall, or the listener or speaker inside a different vehicle
	protected static const float SINGLE_OBSTACLE_MUFFLE = 0.6;
	//! At least two separate obstacles, e.g. rooms apart
	protected static const float MULTIPLE_OBSTACLE_MUFFLE = 0.9;
	protected static const float VEHICLE_MUFFLE = 0.5;

	protected ref TraceParam m_Trace = new TraceParam();
	protected ref array<IEntity> m_aExclude = {};

	//------------------------------------------------------------------------------------------------
	//! A null listener is a free camera with no body of its own: what is in the way still muffles, but there is
	//! no vehicle around it and nothing of its own to leave out of the trace.
	float Compute(IEntity listener, vector listenerPosition, notnull IEntity speaker, vector speakerPosition)
	{
		IEntity listenerVehicle;
		if (listener)
			listenerVehicle = CompartmentAccessComponent.GetVehicleIn(listener);

		IEntity speakerVehicle = CompartmentAccessComponent.GetVehicleIn(speaker);
		if (listenerVehicle && listenerVehicle == speakerVehicle)
			return 0;

		float muffle = 0;
		if (listenerVehicle || speakerVehicle)
			muffle = VEHICLE_MUFFLE;

		m_aExclude.Clear();
		if (listener)
			m_aExclude.Insert(listener);

		m_aExclude.Insert(speaker);
		if (listenerVehicle)
			m_aExclude.Insert(listenerVehicle);

		if (speakerVehicle)
			m_aExclude.Insert(speakerVehicle);

		int obstacles = CountObstacles(listenerPosition, speakerPosition, m_aExclude);
		if (obstacles >= 2)
			return Math.Max(muffle, MULTIPLE_OBSTACLE_MUFFLE);

		if (obstacles == 1)
			return Math.Max(muffle, SINGLE_OBSTACLE_MUFFLE);

		return muffle;
	}

	//------------------------------------------------------------------------------------------------
	//! Traces from both ends: a different first hit from each side means at least two obstacles in between.
	//! Public because AI hearing wants the same count without the muffle it maps to.
	int CountObstacles(vector from, vector to, notnull array<IEntity> exclude)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		m_Trace.Flags = TraceFlags.DEFAULT | TraceFlags.ANY_CONTACT;
		m_Trace.LayerMask = EPhysicsLayerDefs.Projectile;
		m_Trace.ExcludeArray = exclude;

		m_Trace.Start = from;
		m_Trace.End = to;
		m_Trace.TraceEnt = null;
		if (world.TraceMove(m_Trace, null) >= 1)
			return 0;

		IEntity nearListener = m_Trace.TraceEnt;

		m_Trace.Start = to;
		m_Trace.End = from;
		m_Trace.TraceEnt = null;
		if (world.TraceMove(m_Trace, null) >= 1)
			return 1;

		if (m_Trace.TraceEnt != nearListener)
			return 2;

		return 1;
	}
}
