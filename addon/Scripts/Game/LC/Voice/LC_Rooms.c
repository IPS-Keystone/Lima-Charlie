//------------------------------------------------------------------------------------------------
//! Where one person is in the engine's own room model. A building with interior audio carries a
//! WorldSubsceneComponent, in which area 0 is the outside world and areas above it are that building's
//! rooms, separated by doorways and windows the engine calls portals.
class LC_RoomLocation
{
	//! The building whose room model this location is expressed in, or null when not inside one
	IEntity m_Building;
	WorldSubsceneComponent m_Subscene;
	int m_iArea = LC_Rooms.AREA_NONE;
	//! Approximate volume of that room in cubic metres; 0 outdoors
	float m_fVolume;
	int m_iCheckedTick;
	vector m_vCheckedPosition;

	//------------------------------------------------------------------------------------------------
	bool IsIndoors()
	{
		return m_Subscene && m_iArea > LC_Rooms.AREA_OUTSIDE && m_iArea != LC_Rooms.AREA_NONE;
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_Building = null;
		m_Subscene = null;
		m_iArea = LC_Rooms.AREA_NONE;
		m_fVolume = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! One doorway or window, and the two areas it joins
class LC_RoomPortal
{
	int m_iPortal;
	int m_iAreaA = LC_Rooms.AREA_NONE;
	int m_iAreaB = LC_Rooms.AREA_NONE;
	//! Middle of the probe cell it was found in, in the building's local space
	vector m_vLocal;

	//------------------------------------------------------------------------------------------------
	bool JoinsKnownAreas()
	{
		return m_iAreaA != LC_Rooms.AREA_NONE && m_iAreaB != LC_Rooms.AREA_NONE && m_iAreaA != m_iAreaB;
	}
}

//------------------------------------------------------------------------------------------------
//! The room layout of one building type.
//!
//! The engine says which area a point is in and how open each portal is, but not which areas a portal
//! joins or where it is. That is discovered here: the building is probed with small boxes to find each
//! portal, then each portal is asked what lies a short step to either side of it.
//!
//! A layout belongs to the model, so it is discovered once per prefab and reused by every copy of that
//! building on the map. Probing stops as soon as every portal the engine reports has been found.
class LC_RoomLayout
{
	//! Side of one probe box. Smaller finds narrow portals more reliably and costs more probes.
	protected static const float CELL_M = 0.75;
	//! How far to either side of a portal we look to see which areas it joins
	protected static const float PROBE_M = 0.8;
	//! Probing gives up after this many cells, so a building whose portals cannot be found this way stops
	//! costing anything
	protected static const int MAX_CELLS = 40000;
	protected static const int MAX_CELLS_PER_AXIS = 200;

	string m_sPrefab;
	int m_iAreaCount;
	int m_iPortalCount;
	ref array<ref LC_RoomPortal> m_aPortals = {};
	bool m_bComplete;
	//! Portals the engine reports that probing never located. The layout still works, it just cannot see
	//! through those doorways.
	int m_iMissing;
	int m_iCellsDone;

	protected vector m_vMins;
	protected vector m_vMaxs;
	protected int m_iCellX;
	protected int m_iCellY;
	protected int m_iCellZ;
	protected int m_iCellsX;
	protected int m_iCellsY;
	protected int m_iCellsZ;
	protected ref map<int, ref LC_RoomPortal> m_mByPortal = new map<int, ref LC_RoomPortal>();

	//------------------------------------------------------------------------------------------------
	void Begin(string prefab, notnull IEntity building, notnull WorldSubsceneComponent subscene)
	{
		m_sPrefab = prefab;
		m_iAreaCount = subscene.GetAreaCount();
		m_iPortalCount = subscene.GetPortalCount();

		building.GetBounds(m_vMins, m_vMaxs);
		m_iCellsX = Math.ClampInt(Math.Ceil((m_vMaxs[0] - m_vMins[0]) / CELL_M), 1, MAX_CELLS_PER_AXIS);
		m_iCellsY = Math.ClampInt(Math.Ceil((m_vMaxs[1] - m_vMins[1]) / CELL_M), 1, MAX_CELLS_PER_AXIS);
		m_iCellsZ = Math.ClampInt(Math.Ceil((m_vMaxs[2] - m_vMins[2]) / CELL_M), 1, MAX_CELLS_PER_AXIS);

		// A building with a single area has no rooms of its own, so there is nothing to map
		if (m_iPortalCount <= 0 || m_iAreaCount <= 1)
			m_bComplete = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Probes up to cellBudget cells; returns how many were spent
	int Build(notnull IEntity building, notnull WorldSubsceneComponent subscene, int cellBudget)
	{
		if (m_bComplete)
			return 0;

		vector transform[4];
		building.GetWorldTransform(transform);

		float half = CELL_M * 0.5;
		int spent;
		while (spent < cellBudget)
		{
			vector cell = Vector(m_vMins[0] + (m_iCellX + 0.5) * CELL_M, m_vMins[1] + (m_iCellY + 0.5) * CELL_M, m_vMins[2] + (m_iCellZ + 0.5) * CELL_M);
			vector mins = Vector(cell[0] - half, cell[1] - half, cell[2] - half);
			vector maxs = Vector(cell[0] + half, cell[1] + half, cell[2] + half);

			int portal = subscene.FindPortalByOBB(mins, maxs, transform);
			spent++;
			m_iCellsDone++;

			if (portal >= 0 && portal < LC_Rooms.AREA_NONE && !m_mByPortal.Contains(portal))
				Record(subscene, portal, cell);

			if (!Advance())
				break;
		}

		if (m_aPortals.Count() >= m_iPortalCount || m_iCellsDone >= MAX_CELLS)
			Finish();

		return spent;
	}

	//------------------------------------------------------------------------------------------------
	//! Which areas lie either side of a portal, by asking what is a short step away along each axis
	protected void Record(notnull WorldSubsceneComponent subscene, int portal, vector cell)
	{
		LC_RoomPortal entry = new LC_RoomPortal();
		entry.m_iPortal = portal;
		entry.m_vLocal = cell;

		for (int axis = 0; axis < 3; axis++)
		{
			vector step = Vector(PROBE_M, 0, 0);
			if (axis == 1)
				step = Vector(0, PROBE_M, 0);
			else if (axis == 2)
				step = Vector(0, 0, PROBE_M);

			int before = subscene.FindArea(cell - step);
			int after = subscene.FindArea(cell + step);
			if (before == after || before == LC_Rooms.AREA_NONE || after == LC_Rooms.AREA_NONE)
				continue;

			entry.m_iAreaA = before;
			entry.m_iAreaB = after;
			break;
		}

		m_mByPortal.Set(portal, entry);
		m_aPortals.Insert(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Steps the cell cursor; false once every cell has been probed
	protected bool Advance()
	{
		m_iCellX++;
		if (m_iCellX < m_iCellsX)
			return true;

		m_iCellX = 0;
		m_iCellY++;
		if (m_iCellY < m_iCellsY)
			return true;

		m_iCellY = 0;
		m_iCellZ++;
		return m_iCellZ < m_iCellsZ;
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		m_bComplete = true;
		m_iMissing = m_iPortalCount - m_aPortals.Count();
		if (m_iMissing < 0)
			m_iMissing = 0;

		// A portal whose two areas could not be established is no use for pathing
		for (int i = m_aPortals.Count() - 1; i >= 0; i--)
		{
			if (!m_aPortals[i].JoinsKnownAreas())
				m_aPortals.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		string state = "mapping";
		if (m_bComplete)
			state = "done";

		return string.Format("%1 areas, %2 portals, %3 usable, %4 unfound, %5 cells, %6", m_iAreaCount, m_iPortalCount, m_aPortals.Count(), m_iMissing, m_iCellsDone, state);
	}
}

//------------------------------------------------------------------------------------------------
//! Occlusion from the engine's room model instead of from traces.
//!
//! Two people in the same room hear each other clearly, with no tracing at all. Two people in different
//! rooms of one building, or one inside a building and one outside it, are muffled by the cheapest path
//! between their areas: an open door costs little, a closed one costs about as much as a wall, and an
//! intact window is not a path at all. Anything else - both outdoors, two different buildings, or a
//! building with no room model - is left to tracing.
class LC_Rooms
{
	static const int AREA_OUTSIDE = 0;
	//! What FindArea and FindPortalByOBB return when they find nothing
	static const int AREA_NONE = 0xFFFF;

	//! Muffle for a closed door or an intact window, matching LC_Occlusion's single obstacle
	protected static const float CLOSED_MUFFLE = 0.6;
	//! Muffle for a wide open doorway
	protected static const float OPEN_MUFFLE = 0.2;
	//! A location is rechecked after this long, or once its owner has moved this far
	protected static const int RECHECK_MS = 500;
	protected static const float RECHECK_MOVE_M = 0.5;
	//! Probe cells spent per game state write while the local player stands in an unmapped building
	protected static const int BUILD_BUDGET = 128;
	protected static const float SEARCH_RADIUS_M = 0.5;

	protected ref map<string, ref LC_RoomLayout> m_mLayouts = new map<string, ref LC_RoomLayout>();
	protected ref array<float> m_aDistance = {};

	//! Where the sphere query is looking and what it found, since a query callback carries nothing itself
	protected vector m_vQueryPosition;
	protected IEntity m_QueryBuilding;
	protected WorldSubsceneComponent m_QuerySubscene;
	protected int m_iQueryArea;

	protected int m_iBuildSpent;

	//------------------------------------------------------------------------------------------------
	//! Updates where this position is, reusing the last answer while it is still fresh. The building the
	//! location was last in is asked first, which answers it for anyone who has not left that building.
	void Locate(notnull LC_RoomLocation location, vector position, int now)
	{
		bool moved = vector.DistanceSq(location.m_vCheckedPosition, position) > RECHECK_MOVE_M * RECHECK_MOVE_M;
		if (!moved && now - location.m_iCheckedTick < RECHECK_MS)
			return;

		location.m_iCheckedTick = now;
		location.m_vCheckedPosition = position;

		if (location.m_Subscene && location.m_Building)
		{
			int area = location.m_Subscene.FindArea(location.m_Building.CoordToLocal(position));
			if (area > AREA_OUTSIDE && area != AREA_NONE)
			{
				SetArea(location, area);
				return;
			}
		}

		Search(location, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Looks for a building whose room model contains this position
	protected void Search(notnull LC_RoomLocation location, vector position)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_vQueryPosition = position;
		m_QueryBuilding = null;
		m_QuerySubscene = null;
		m_iQueryArea = AREA_NONE;
		world.QueryEntitiesBySphere(position, SEARCH_RADIUS_M, OnQueryEntity, null, EQueryEntitiesFlags.STATIC);

		if (!m_QuerySubscene)
		{
			location.Clear();
			return;
		}

		location.m_Building = m_QueryBuilding;
		location.m_Subscene = m_QuerySubscene;
		SetArea(location, m_iQueryArea);
	}

	//------------------------------------------------------------------------------------------------
	//! A static entity near the position: walk up to whatever carries the room model, as vanilla's window
	//! damage does, and keep it if the position is inside one of its rooms
	protected bool OnQueryEntity(IEntity entity)
	{
		IEntity parent = entity;
		while (parent)
		{
			WorldSubsceneComponent subscene = WorldSubsceneComponent.Cast(parent.FindComponent(WorldSubsceneComponent));
			if (subscene)
			{
				int area = subscene.FindArea(parent.CoordToLocal(m_vQueryPosition));
				if (area > AREA_OUTSIDE && area != AREA_NONE)
				{
					m_QueryBuilding = parent;
					m_QuerySubscene = subscene;
					m_iQueryArea = area;
					return false;
				}

				break;
			}

			parent = parent.GetParent();
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetArea(notnull LC_RoomLocation location, int area)
	{
		location.m_iArea = area;
		location.m_fVolume = 0;
		if (location.m_Subscene && area > AREA_OUTSIDE && area != AREA_NONE)
			location.m_fVolume = location.m_Subscene.GetAreaApproxVolume(area);
	}

	//------------------------------------------------------------------------------------------------
	//! Spends this write's probe budget on the building the local player is in, if its type is not mapped
	//! yet. Called once per game state write.
	void Update(notnull LC_RoomLocation listener)
	{
		m_iBuildSpent = 0;
		if (!listener.IsIndoors())
			return;

		LC_RoomLayout layout = GetLayout(listener.m_Building, listener.m_Subscene);
		if (layout && !layout.m_bComplete)
			m_iBuildSpent = layout.Build(listener.m_Building, listener.m_Subscene, BUILD_BUDGET);
	}

	//------------------------------------------------------------------------------------------------
	//! The layout for this building's type, started the first time we are inside one of them
	LC_RoomLayout GetLayout(IEntity building, WorldSubsceneComponent subscene)
	{
		if (!building || !subscene)
			return null;

		string prefab = SCR_ResourceNameUtils.GetPrefabName(building);
		if (prefab.IsEmpty())
			return null;

		LC_RoomLayout layout = m_mLayouts.Get(prefab);
		if (layout)
			return layout;

		layout = new LC_RoomLayout();
		layout.Begin(prefab, building, subscene);
		m_mLayouts.Set(prefab, layout);
		return layout;
	}

	//------------------------------------------------------------------------------------------------
	//! How muffled the speaker is for this listener, from the room model alone.
	//! \param muffle 0 clear to 1 fully obstructed, only meaningful when this returns true
	//! \return false when the room model cannot answer, so tracing is needed instead
	bool GetMuffle(notnull LC_RoomLocation listener, notnull LC_RoomLocation speaker, out float muffle)
	{
		muffle = 0;

		// Both outdoors tells us nothing: a building can still stand between them
		if (!listener.IsIndoors() && !speaker.IsIndoors())
			return false;

		// Whoever is indoors owns the room model both are placed in. With two buildings involved there is
		// no single model to path through.
		WorldSubsceneComponent subscene = listener.m_Subscene;
		IEntity building = listener.m_Building;
		int listenerArea = listener.m_iArea;
		int speakerArea = AREA_OUTSIDE;
		if (listener.IsIndoors() && speaker.IsIndoors())
		{
			if (listener.m_Building != speaker.m_Building)
				return false;

			speakerArea = speaker.m_iArea;
		}
		else if (!listener.IsIndoors())
		{
			subscene = speaker.m_Subscene;
			building = speaker.m_Building;
			listenerArea = AREA_OUTSIDE;
			speakerArea = speaker.m_iArea;
		}

		if (listenerArea == speakerArea)
			return true;

		LC_RoomLayout layout = GetLayout(building, subscene);
		if (!layout || !layout.m_bComplete || layout.m_aPortals.IsEmpty())
			return false;

		return FindPath(subscene, layout, listenerArea, speakerArea, muffle);
	}

	//------------------------------------------------------------------------------------------------
	//! Cheapest total muffle from one area to another across the portals between them. There are only ever
	//! a handful of areas, so every edge is relaxed repeatedly rather than kept in a sorted queue.
	protected bool FindPath(notnull WorldSubsceneComponent subscene, notnull LC_RoomLayout layout, int from, int to, out float muffle)
	{
		int areaCount = layout.m_iAreaCount;
		if (from < 0 || to < 0 || from >= areaCount || to >= areaCount)
			return false;

		m_aDistance.Clear();
		for (int i = 0; i < areaCount; i++)
		{
			m_aDistance.Insert(1.0);
		}

		m_aDistance[from] = 0;

		for (int pass = 0; pass < areaCount; pass++)
		{
			bool changed = false;
			foreach (LC_RoomPortal portal : layout.m_aPortals)
			{
				if (portal.m_iAreaA >= areaCount || portal.m_iAreaB >= areaCount)
					continue;

				float cost = PortalMuffle(subscene, portal.m_iPortal);
				if (Relax(portal.m_iAreaA, portal.m_iAreaB, cost))
					changed = true;

				if (Relax(portal.m_iAreaB, portal.m_iAreaA, cost))
					changed = true;
			}

			if (!changed)
				break;
		}

		muffle = m_aDistance[to];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Relax(int from, int to, float cost)
	{
		float candidate = m_aDistance[from] + cost;
		if (candidate >= m_aDistance[to])
			return false;

		m_aDistance[to] = candidate;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What passing through one portal costs. A door reports how open it is; a window reports whether it
	//! blocks sound, and stops blocking once it is broken.
	protected float PortalMuffle(notnull WorldSubsceneComponent subscene, int portal)
	{
		if (!subscene.IsPortalEnabled(portal))
			return CLOSED_MUFFLE;

		if (subscene.IsPortalBlockingSound(portal))
			return CLOSED_MUFFLE;

		if (subscene.IsPortalPassingSound(portal))
			return OPEN_MUFFLE;

		float opening = Math.Clamp(subscene.GetPortalOpening(portal), 0, 1);
		return CLOSED_MUFFLE + (OPEN_MUFFLE - CLOSED_MUFFLE) * opening;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe cells spent on the last write, for the diagnostic line
	int GetBuildSpent()
	{
		return m_iBuildSpent;
	}

	//------------------------------------------------------------------------------------------------
	//! "Barn_01 room 3, 84 m3" or "outdoors"
	string Describe(notnull LC_RoomLocation location)
	{
		if (!location.IsIndoors())
			return "outdoors";

		string prefab = SCR_ResourceNameUtils.GetPrefabName(location.m_Building);
		int lastSlash = prefab.LastIndexOf("/");
		string name = prefab;
		if (lastSlash >= 0)
			name = prefab.Substring(lastSlash + 1, prefab.Length() - lastSlash - 1);

		int volume = Math.Round(location.m_fVolume);
		return name + " room " + location.m_iArea.ToString() + ", " + volume.ToString() + " m3";
	}

	//------------------------------------------------------------------------------------------------
	//! The layout of the building this location is in
	string DescribeLayout(notnull LC_RoomLocation location)
	{
		if (!location.IsIndoors())
			return "none";

		LC_RoomLayout layout = GetLayout(location.m_Building, location.m_Subscene);
		if (!layout)
			return "none";

		return layout.Describe();
	}

	//------------------------------------------------------------------------------------------------
	//! How open each of the first few portals of this building is, as "index:percent" with B for blocking
	//! sound, S for passing sound and X for disabled. Opening a door and watching this line is how to
	//! confirm the engine keeps portal state up to date.
	string DescribePortals(notnull LC_RoomLocation location, int max = 8)
	{
		if (!location.IsIndoors())
			return "";

		string text;
		WorldSubsceneComponent subscene = location.m_Subscene;
		int count = subscene.GetPortalCount();
		if (count > max)
			count = max;

		for (int i = 0; i < count; i++)
		{
			if (i > 0)
				text += " ";

			int opening = Math.Round(subscene.GetPortalOpening(i) * 100);
			text += i.ToString() + ":" + opening.ToString();
			if (subscene.IsPortalBlockingSound(i))
				text += "B";

			if (subscene.IsPortalPassingSound(i))
				text += "S";

			if (!subscene.IsPortalEnabled(i))
				text += "X";
		}

		return text;
	}
}
