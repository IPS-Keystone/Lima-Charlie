
// Dev-only: Lima Charlie engine-call timings against a real building. APIFunc = "LCBench_World"
class LCBench_WorldRequest : JsonApiStruct
{
	float x;
	float z;
	float radius;
	string prefab;

	void LCBench_WorldRequest()
	{
		RegV("x");
		RegV("z");
		RegV("radius");
		RegV("prefab");
	}
}

class LCBench_WorldResponse : JsonApiStruct
{
	string results;

	void LCBench_WorldResponse()
	{
		RegV("results");
	}
}

class LCBench_World : NetApiHandler
{
	protected ref array<IEntity> m_aBuildings = {};
	protected int m_iHits;

	override JsonApiStruct GetRequest()
	{
		return new LCBench_WorldRequest();
	}

	protected bool CollectBuilding(IEntity entity)
	{
		m_iHits++;
		WorldSubsceneComponent subscene = WorldSubsceneComponent.Cast(entity.FindComponent(WorldSubsceneComponent));
		if (subscene && subscene.GetAreaCount() > 1 && !m_aBuildings.Contains(entity))
			m_aBuildings.Insert(entity);

		return true;
	}

	protected string Line(string name, int totalMs, int count)
	{
		float us = totalMs * 1000.0 / count;
		return name + ": " + us.ToString(-1, 3) + " us/op (" + count.ToString() + " ops, " + totalMs.ToString() + " ms)\n";
	}

	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		LCBench_WorldRequest req = LCBench_WorldRequest.Cast(request);
		LCBench_WorldResponse resp = new LCBench_WorldResponse();
		string text;

		BaseWorld world = GetGame().GetWorld();
		string mode = "game world";
		if (!world)
		{
			WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
			if (worldEditor && worldEditor.GetApi())
			{
				world = worldEditor.GetApi().GetWorld();
				mode = "editor world";
			}
		}

		if (!world)
		{
			resp.results = "no world";
			return resp;
		}

		text += mode + "\n";
		float radius = req.radius;
		if (radius <= 0)
			radius = 300;

		vector centre = Vector(req.x, world.GetSurfaceY(req.x, req.z), req.z);
		if (!req.prefab.IsEmpty())
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = centre;
			IEntity spawned = GetGame().SpawnEntityPrefab(Resource.Load(req.prefab), world, params);
			bool ok = spawned != null;
			resp.results = "spawned " + req.prefab + " " + ok.ToString() + "\n";
			return resp;
		}

		m_aBuildings.Clear();
		m_iHits = 0;
		world.QueryEntitiesBySphere(centre, radius, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
		int staticHits = m_iHits;
		m_iHits = 0;
		world.QueryEntitiesBySphere(centre, radius, CollectBuilding, null, EQueryEntitiesFlags.ALL);
		text += "static entities " + staticHits.ToString() + ", all entities " + m_iHits.ToString() + ", buildings with rooms " + m_aBuildings.Count().ToString() + "\n";
		if (m_aBuildings.IsEmpty())
		{
			resp.results = text;
			return resp;
		}

		// The building with the most rooms
		IEntity building;
		WorldSubsceneComponent subscene;
		int bestAreas;
		foreach (IEntity candidate : m_aBuildings)
		{
			WorldSubsceneComponent candidateSubscene = WorldSubsceneComponent.Cast(candidate.FindComponent(WorldSubsceneComponent));
			if (candidateSubscene.GetAreaCount() > bestAreas)
			{
				bestAreas = candidateSubscene.GetAreaCount();
				building = candidate;
				subscene = candidateSubscene;
			}
		}

		string prefab = SCR_ResourceNameUtils.GetPrefabName(building);
		vector mins;
		vector maxs;
		building.GetBounds(mins, maxs);
		vector size = maxs - mins;
		int areaCount = subscene.GetAreaCount();
		int portalCount = subscene.GetPortalCount();
		vector origin = building.GetOrigin();
		text += "building " + prefab + " areas " + areaCount.ToString() + " portals " + portalCount.ToString() + " at " + origin.ToString() + " size " + size.ToString() + "\n";

		// Two indoor points in different rooms, and how many indoor cells there are
		vector inA;
		vector inB;
		int areaA = -1;
		int areaB = -1;
		int indoorCells;
		for (float lx = mins[0] + 0.5; lx < maxs[0]; lx += 1.5)
		{
			for (float ly = mins[1] + 1.6; ly < maxs[1]; ly += 3)
			{
				for (float lz = mins[2] + 0.5; lz < maxs[2]; lz += 1.5)
				{
					vector local = Vector(lx, ly, lz);
					int area = subscene.FindArea(local);
					if (area <= 0 || area == 0xFFFF)
						continue;

					indoorCells++;
					if (areaA < 0)
					{
						areaA = area;
						inA = building.CoordToParent(local);
					}
					else if (areaB < 0 && area != areaA)
					{
						areaB = area;
						inB = building.CoordToParent(local);
					}
				}
			}
		}

		text += "indoor sample cells " + indoorCells.ToString() + ", rooms " + areaA.ToString() + " and " + areaB.ToString() + "\n";

		// Native room calls
		int count = 20000;
		vector centreWorld = building.CoordToParent((mins + maxs) * 0.5);
		int start = System.GetTickCount();
		float sink;
		for (int i = 0; i < count; i++)
		{
			sink += subscene.FindArea(building.CoordToLocal(centreWorld));
		}

		text += Line("FindArea(CoordToLocal) - Locate fast path", System.GetTickCount() - start, count);

		vector transform[4];
		building.GetWorldTransform(transform);
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			int cellX = i % 40;
			int cellY = (i / 40) % 20;
			int cellZ = (i / 800) % 40;
			float cx = mins[0] + cellX * 0.75;
			float cy = mins[1] + cellY * 0.75;
			float cz = mins[2] + cellZ * 0.75;
			sink += subscene.FindPortalByOBB(Vector(cx, cy, cz), Vector(cx + 0.75, cy + 0.75, cz + 0.75), transform);
		}

		text += Line("FindPortalByOBB 0.75 m cell - mapping probe", System.GetTickCount() - start, count);

		int portals = subscene.GetPortalCount();
		count = 200;
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			for (int p = 0; p < portals; p++)
			{
				if (subscene.IsPortalEnabled(p))
					sink++;

				if (subscene.IsPortalBlockingSound(p))
					sink++;

				if (subscene.IsPortalPassingSound(p))
					sink++;

				sink += subscene.GetPortalOpening(p);
			}
		}

		text += Line("4 portal state reads, per portal", System.GetTickCount() - start, count * portals);

		// Mapping the whole building type the way LC does it
		LC_RoomLayout layout = new LC_RoomLayout();
		layout.Begin(prefab, building, subscene);
		int batches;
		start = System.GetTickCount();
		while (!layout.m_bComplete && batches < 2000)
		{
			layout.Build(building, subscene, 128);
			batches++;
		}

		text += Line("LC_RoomLayout.Build batch of 128 (" + layout.Describe() + ", " + batches.ToString() + " batches)", System.GetTickCount() - start, batches);

		// Locate: outdoors next to the building is a sphere query every time; indoors is the fast path
		LC_Rooms rooms = new LC_Rooms();
		LC_RoomLocation location = new LC_RoomLocation();
		float ring = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5 + 2;
		int now = 1000000;
		count = 5000;
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			float angle = i * 0.05;
			vector outside = Vector(centreWorld[0] + Math.Cos(angle) * ring, centreWorld[1] - (maxs[1] - mins[1]) * 0.5 + 1.7, centreWorld[2] + Math.Sin(angle) * ring);
			now += 1000;
			rooms.Locate(location, outside, now);
		}

		text += Line("LC_Rooms.Locate outdoors 2 m from the walls (sphere query)", System.GetTickCount() - start, count);

		if (areaA > 0)
		{
			start = System.GetTickCount();
			for (int i = 0; i < count; i++)
			{
				now += 1000;
				rooms.Locate(location, inA, now);
			}

			text += Line("LC_Rooms.Locate indoors, same building (fast path)", System.GetTickCount() - start, count);
		}

		// A mapped layout in LC_Rooms, then the muffle between two rooms and from outdoors
		if (areaA > 0 && areaB > 0)
		{
			LC_RoomLocation listener = new LC_RoomLocation();
			LC_RoomLocation speaker = new LC_RoomLocation();
			LC_RoomLocation outdoor = new LC_RoomLocation();
			now += 1000;
			rooms.Locate(listener, inA, now);
			rooms.Locate(speaker, inB, now);
			int updates;
			start = System.GetTickCount();
			while (updates < 2000 && !rooms.DescribeLayout(listener).Contains("done"))
			{
				rooms.Update(listener);
				updates++;
			}

			int layoutMs = System.GetTickCount() - start;
			text += "rooms layout via Update: " + rooms.DescribeLayout(listener) + " in " + updates.ToString() + " writes, " + layoutMs.ToString() + " ms\n";

			float muffle;
			bool answered = rooms.GetMuffle(listener, speaker, 100, now, muffle);
			text += "room to room answered " + answered.ToString() + " muffle " + muffle.ToString(-1, 2) + "\n";

			count = 20000;
			start = System.GetTickCount();
			for (int i = 0; i < count; i++)
			{
				rooms.GetMuffle(listener, speaker, 100, now, muffle);
			}

			text += Line("GetMuffle room to room, paths cached", System.GetTickCount() - start, count);

			count = 1000;
			start = System.GetTickCount();
			for (int i = 0; i < count; i++)
			{
				now += 300;
				rooms.GetMuffle(listener, speaker, 100, now, muffle);
			}

			text += Line("GetMuffle room to room, portal costs re-read (no change)", System.GetTickCount() - start, count);

			now += 1000;
			vector door = Vector(centreWorld[0] + ring, centreWorld[1] - (maxs[1] - mins[1]) * 0.5 + 1.7, centreWorld[2]);
			rooms.Locate(outdoor, door, now);
			answered = rooms.GetMuffle(outdoor, speaker, 400, now, muffle);
			text += "outdoor to room answered " + answered.ToString() + " muffle " + muffle.ToString(-1, 2) + "\n";
		}

		// Occlusion traces straight through the building, and through open air above it
		LC_Occlusion occlusion = new LC_Occlusion();
		array<IEntity> exclude = {};
		float half = ring + 3;
		float headY = centreWorld[1] - (maxs[1] - mins[1]) * 0.5 + 1.7;
		vector from = Vector(centreWorld[0] - half, headY, centreWorld[2]);
		vector to = Vector(centreWorld[0] + half, headY, centreWorld[2]);
		int obstacles = occlusion.CountObstacles(from, to, exclude);
		float across = half * 2;
		count = 2000;
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			obstacles = occlusion.CountObstacles(from, to, exclude);
		}

		text += Line("CountObstacles through the building (" + obstacles.ToString() + " obstacles, " + occlusion.DescribeCover() + ", " + across.ToString() + " m)", System.GetTickCount() - start, count);

		from[1] = from[1] + 60;
		to[1] = to[1] + 60;
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			obstacles = occlusion.CountObstacles(from, to, exclude);
		}

		text += Line("CountObstacles through open air (" + obstacles.ToString() + " obstacles)", System.GetTickCount() - start, count);

		// A 60 m street-level trace towards the search centre, as between two people in a town
		vector street = Vector(centreWorld[0] - 60, world.GetSurfaceY(centreWorld[0] - 60, centreWorld[2] + 15) + 1.7, centreWorld[2] + 15);
		vector streetEnd = Vector(centreWorld[0], world.GetSurfaceY(centreWorld[0], centreWorld[2] + 15) + 1.7, centreWorld[2] + 15);
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			obstacles = occlusion.CountObstacles(street, streetEnd, exclude);
		}

		text += Line("CountObstacles 60 m along a street (" + obstacles.ToString() + " obstacles, " + occlusion.DescribeCover() + ")", System.GetTickCount() - start, count);

		resp.results = text;
		return resp;
	}
}
