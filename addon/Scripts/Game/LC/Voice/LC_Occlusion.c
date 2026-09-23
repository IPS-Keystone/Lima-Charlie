//------------------------------------------------------------------------------------------------
//! How obstructed the path between a listener and a speaker is, as a muffle amount from 0 (clear) to 1.
//! The plugin turns it into a low-pass filter and some attenuation.
class LC_Occlusion
{
	//! One wall, or one vehicle hull
	protected static const float SINGLE_OBSTACLE_MUFFLE = 0.6;
	//! At least two separate obstacles, e.g. rooms apart, or two hulls
	protected static const float MULTIPLE_OBSTACLE_MUFFLE = 0.9;
	//! Anything narrower than this across is a prop rather than cover: posts, bollards, signs, trunks, and
	//! people standing in the way
	protected static const float NARROW_M = 0.6;
	//! How many of those a single trace steps past before giving up and calling it cover
	protected static const int MAX_SKIPS = 2;
	protected static const float SKIP_STEP_M = 0.25;

	protected ref TraceParam m_Trace = new TraceParam();
	protected ref array<IEntity> m_aExclude = {};

	//------------------------------------------------------------------------------------------------
	//! A null listener is a free camera with no body of its own: what is in the way still muffles, but there
	//! is nothing of its own to leave out of the trace.
	//!
	//! Vehicles are not treated as a special case and are not left out of the trace. Sitting in one used to
	//! carry a flat muffle, which was wrong for everything you sit on rather than in: a mortar, a technical's
	//! bed, an open jeep, a hatch you are turned out of. A hull that is really between two people blocks the
	//! trace like any other wall, and an open mount does not block it at all.
	float Compute(IEntity listener, vector listenerPosition, notnull IEntity speaker, vector speakerPosition)
	{
		IEntity listenerVehicle;
		if (listener)
			listenerVehicle = CompartmentAccessComponent.GetVehicleIn(listener);

		IEntity speakerVehicle = CompartmentAccessComponent.GetVehicleIn(speaker);
		if (listenerVehicle && listenerVehicle == speakerVehicle)
			return 0;

		m_aExclude.Clear();
		if (listener)
			m_aExclude.Insert(listener);

		m_aExclude.Insert(speaker);

		int obstacles = CountObstacles(listenerPosition, speakerPosition, m_aExclude);
		if (obstacles >= 2)
			return MULTIPLE_OBSTACLE_MUFFLE;

		if (obstacles == 1)
			return SINGLE_OBSTACLE_MUFFLE;

		return 0;
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

		IEntity nearListener = FirstCover(world, from, to);
		if (!nearListener)
			return 0;

		IEntity nearSpeaker = FirstCover(world, to, from);
		if (!nearSpeaker)
			return 1;

		if (nearSpeaker != nearListener)
			return 2;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! The first thing between these two points that counts as cover, or null if nothing does.
	//!
	//! A lamp post, a bollard, a sign, a tree trunk or a person standing in the way blocked the trace and
	//! read as a whole wall, which made open ground sound like a building. Anything narrow in plan is
	//! stepped past and the trace carries on behind it. Only a couple of skips, so a thicket does still
	//! muffle, and the cost is one trace in the common case of nothing in the way at all.
	protected IEntity FirstCover(notnull BaseWorld world, vector from, vector to)
	{
		vector start = from;
		for (int skip = 0; skip <= MAX_SKIPS; skip++)
		{
			m_Trace.Start = start;
			m_Trace.End = to;
			m_Trace.TraceEnt = null;
			float fraction = world.TraceMove(m_Trace, null);
			if (fraction >= 1)
				return null;

			IEntity hit = m_Trace.TraceEnt;
			if (!IsNarrow(hit))
				return hit;

			// Resume just past it, along the remaining path
			vector remaining = to - start;
			vector at = start + remaining * fraction;
			float length = remaining.Length();
			if (length <= SKIP_STEP_M)
				return null;

			start = at + remaining * (SKIP_STEP_M / length);
			if (vector.DistanceSq(start, to) <= SKIP_STEP_M * SKIP_STEP_M)
				return null;
		}

		// Too many narrow things in a row to be open ground
		return m_Trace.TraceEnt;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this is too slight in plan to be cover. Measured on the wider of its two horizontal sides,
	//! so a fence panel or a wall section still counts while a post does not.
	protected bool IsNarrow(IEntity entity)
	{
		if (!entity)
			return false;

		vector mins;
		vector maxs;
		entity.GetWorldBounds(mins, maxs);
		float width = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]);
		return width < NARROW_M;
	}
}
