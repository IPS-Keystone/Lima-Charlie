//------------------------------------------------------------------------------------------------
//! How obstructed the path between a listener and a speaker is, as a muffle amount from 0 (clear) to 1.
//! The plugin turns it into a low-pass filter and some attenuation.
class LC_Occlusion
{
	//! One wall, or one vehicle hull
	protected static const float SINGLE_OBSTACLE_MUFFLE = 0.6;
	//! At least two separate obstacles, e.g. rooms apart, or two hulls
	protected static const float MULTIPLE_OBSTACLE_MUFFLE = 0.9;
	//! Anything narrower than this across is a prop rather than cover: posts, bollards, signs, bins, trunks
	protected static const float NARROW_M = 0.8;

	protected ref TraceParam m_Trace = new TraceParam();
	protected ref array<IEntity> m_aExclude = {};

	//! What last blocked a trace, for the diagnostic line: null means terrain or other world geometry
	protected IEntity m_CoverEntity;
	protected bool m_bCoverIsWorld;
	protected float m_fCoverWidth;
	protected int m_iSkipped;

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

		m_CoverEntity = null;
		m_bCoverIsWorld = false;
		m_fCoverWidth = 0;
		m_iSkipped = 0;

		IEntity nearListener;
		if (!TraceCover(world, from, to, nearListener))
			return 0;

		IEntity nearSpeaker;
		if (!TraceCover(world, to, from, nearSpeaker))
			return 1;

		if (nearSpeaker != nearListener)
			return 2;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether anything between these two points counts as cover, and what it was.
	//!
	//! One trace, with the engine's own filter callback deciding what is worth stopping at, so a path with
	//! any number of props along it still costs a single trace. A lamp post, a bollard, a sign, a bin or a
	//! person standing in the way used to block and read as a whole wall, which made open ground sound like
	//! a building.
	protected bool TraceCover(notnull BaseWorld world, vector from, vector to, out IEntity cover)
	{
		cover = null;
		m_Trace.Start = from;
		m_Trace.End = to;
		m_Trace.TraceEnt = null;
		if (world.TraceMove(m_Trace, FilterCover) >= 1)
			return false;

		cover = m_Trace.TraceEnt;
		if (cover)
		{
			m_CoverEntity = cover;
			m_fCoverWidth = Width(cover);
		}
		else
		{
			// Terrain and other unowned world geometry come back without an entity, and always count
			m_bCoverIsWorld = true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The engine calls this for each entity the trace meets; false ignores that one and the trace carries
	//! on behind it. Measured on the wider of its two horizontal sides, so a fence panel or a wall section
	//! still counts while a post does not.
	protected bool FilterCover(notnull IEntity entity, vector start = "0 0 0", vector dir = "0 0 0")
	{
		// Nobody is cover, whatever their bounding box says
		if (ChimeraCharacter.Cast(entity))
		{
			m_iSkipped++;
			return false;
		}

		if (Width(entity) >= NARROW_M)
			return true;

		m_iSkipped++;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Widest of the two horizontal sides of this entity's bounding box
	protected float Width(notnull IEntity entity)
	{
		vector mins;
		vector maxs;
		entity.GetWorldBounds(mins, maxs);
		return Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! What blocked the last trace and how many props it ignored on the way, for the diagnostic line
	string DescribeCover()
	{
		string text;
		if (m_CoverEntity)
		{
			string prefab = SCR_ResourceNameUtils.GetPrefabName(m_CoverEntity);
			int lastSlash = prefab.LastIndexOf("/");
			if (lastSlash >= 0)
				prefab = prefab.Substring(lastSlash + 1, prefab.Length() - lastSlash - 1);

			if (prefab.IsEmpty())
				prefab = m_CoverEntity.GetName();

			if (prefab.IsEmpty())
				prefab = "unnamed";

			text = prefab + " " + m_fCoverWidth.ToString(-1, 1) + "m";
		}
		else if (m_bCoverIsWorld)
		{
			text = "world";
		}
		else
		{
			text = "clear";
		}

		if (m_iSkipped > 0)
			text += " skip" + m_iSkipped.ToString();

		return text;
	}
}
