// Dev-only: times LC_Rooms mapping per write with the portal-scaled batch, and checks saving, resuming and
// loading a layout. APIFunc = "LCBench_Map". Play mode only (the room model exists at runtime).
class LCBench_MapRequest : JsonApiStruct
{
	float x;
	float z;
	float radius;
	string prefab;
	//! Writes to map before walking out, for the resume check
	int partial;

	void LCBench_MapRequest()
	{
		RegV("x");
		RegV("z");
		RegV("radius");
		RegV("prefab");
		RegV("partial");
	}
}

class LCBench_MapResponse : JsonApiStruct
{
	string results;

	void LCBench_MapResponse()
	{
		RegV("results");
	}
}

class LCBench_Map : NetApiHandler
{
	protected ref array<IEntity> m_aBuildings = {};

	override JsonApiStruct GetRequest()
	{
		return new LCBench_MapRequest();
	}

	protected bool CollectBuilding(IEntity entity)
	{
		WorldSubsceneComponent subscene = WorldSubsceneComponent.Cast(entity.FindComponent(WorldSubsceneComponent));
		if (subscene && subscene.GetAreaCount() > 1 && !m_aBuildings.Contains(entity))
			m_aBuildings.Insert(entity);

		return true;
	}

	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		LCBench_MapRequest req = LCBench_MapRequest.Cast(request);
		LCBench_MapResponse resp = new LCBench_MapResponse();
		string text;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			resp.results = "no game world (play mode only)";
			return resp;
		}

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

		float radius = req.radius;
		if (radius <= 0)
			radius = 60;

		m_aBuildings.Clear();
		world.QueryEntitiesBySphere(centre, radius, CollectBuilding, null, EQueryEntitiesFlags.ALL);
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

		if (!building)
		{
			resp.results = "no building with rooms";
			return resp;
		}

		string prefab = SCR_ResourceNameUtils.GetPrefabName(building);
		int portalCount = subscene.GetPortalCount();
		text += "building " + prefab + " areas " + bestAreas.ToString() + " portals " + portalCount.ToString() + "\n";

		// A point inside, and one outside
		vector mins;
		vector maxs;
		building.GetBounds(mins, maxs);
		vector inside;
		bool found;
		for (float ly = mins[1] + 0.8; ly < maxs[1] && !found; ly += 0.8)
		{
			for (float lx = mins[0] + 0.5; lx < maxs[0] && !found; lx += 0.75)
			{
				for (float lz = mins[2] + 0.5; lz < maxs[2] && !found; lz += 0.75)
				{
					vector local = Vector(lx, ly, lz);
					int area = subscene.FindArea(local);
					if (area > 0 && area != 0xFFFF)
					{
						inside = building.CoordToParent(local);
						found = true;
					}
				}
			}
		}

		if (!found)
		{
			resp.results = text + "no indoor point found";
			return resp;
		}

		vector outside = building.CoordToParent(Vector(maxs[0] + 20, mins[1] + 1.6, maxs[2] + 20));

		string path = "$profile:LimaCharlie/rooms/" + prefab.Substring(1, 16) + ".txt";
		FileIO.DeleteFile(path);
		bool fileBefore = FileIO.FileExists(path);
		text += "file " + path + " present before: " + fileBefore.ToString() + "\n";

		// 1. Map from nothing, one Update per write, as the writer does
		LC_Rooms rooms = new LC_Rooms();
		LC_RoomLocation listener = new LC_RoomLocation();
		int now = 1000000;
		rooms.Locate(listener, inside, now);
		int writes;
		int totalMs;
		int worstMs;
		int cells;
		string state = rooms.DescribeLayout(listener);
		while (state.Contains("mapping") && writes < 20000)
		{
			int start = System.GetTickCount();
			rooms.Update(listener);
			int took = System.GetTickCount() - start;
			totalMs += took;
			if (took > worstMs)
				worstMs = took;

			cells += rooms.GetBuildSpent();
			writes++;
			state = rooms.DescribeLayout(listener);
		}

		float perWrite = totalMs * 1.0 / Math.Max(writes, 1);
		float perCell = totalMs * 1000.0 / Math.Max(cells, 1);
		text += "full map: " + writes.ToString() + " writes, " + cells.ToString() + " cells, " + totalMs.ToString() + " ms, " + perWrite.ToString(-1, 3) + " ms/write, worst write " + worstMs.ToString() + " ms, " + perCell.ToString(-1, 2) + " us/cell\n";
		text += "  layout: " + state + "\n";
		string mapped = state;
		bool fileAfter = FileIO.FileExists(path);
		text += "  file present after: " + fileAfter.ToString() + "\n";
		text += HeadOfFile(path, 6);

		// 2. A new session loads it
		LC_Rooms loadedRooms = new LC_Rooms();
		LC_RoomLocation loadedListener = new LC_RoomLocation();
		loadedRooms.Locate(loadedListener, inside, now);
		int loadStart = System.GetTickCount();
		loadedRooms.Update(loadedListener);
		int loadMs = System.GetTickCount() - loadStart;
		string loaded = loadedRooms.DescribeLayout(loadedListener);
		text += "new session, first write " + loadMs.ToString() + " ms, spent " + loadedRooms.GetBuildSpent().ToString() + " cells: " + loaded + "\n";
		string mappedTail = mapped;
		bool same = loaded == mappedTail + ", saved";
		text += "  matches the mapped layout: " + same.ToString() + "\n";

		// 3. Partial: map some writes, walk out, a new session resumes from there
		int partialWrites = req.partial;
		if (partialWrites <= 0)
			partialWrites = 150;

		FileIO.DeleteFile(path);
		LC_Rooms partRooms = new LC_Rooms();
		LC_RoomLocation partListener = new LC_RoomLocation();
		partRooms.Locate(partListener, inside, now);
		for (int i = 0; i < partialWrites; i++)
		{
			partRooms.Update(partListener);
		}

		string beforeLeaving = partRooms.DescribeLayout(partListener);
		now += 5000;
		partRooms.Locate(partListener, outside, now);
		partRooms.Update(partListener);
		bool outdoors = !partListener.IsIndoors();
		text += "partial: after " + partialWrites.ToString() + " writes: " + beforeLeaving + "; walked out (outdoors " + outdoors.ToString() + ")\n";

		LC_Rooms resumeRooms = new LC_Rooms();
		LC_RoomLocation resumeListener = new LC_RoomLocation();
		resumeRooms.Locate(resumeListener, inside, now);
		text += "  new session before mapping: " + resumeRooms.DescribeLayout(resumeListener) + "\n";
		int resumeWrites;
		string resumeState = resumeRooms.DescribeLayout(resumeListener);
		while (resumeState.Contains("mapping") && resumeWrites < 20000)
		{
			resumeRooms.Update(resumeListener);
			resumeWrites++;
			resumeState = resumeRooms.DescribeLayout(resumeListener);
		}

		int sum = partialWrites + resumeWrites;
		text += "  resumed to done in " + resumeWrites.ToString() + " more writes (" + sum.ToString() + " total vs " + writes.ToString() + "): " + resumeState + "\n";

		resp.results = text;
		return resp;
	}

	protected string HeadOfFile(string path, int lines)
	{
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return "  (cannot open)\n";

		string text;
		string line;
		int count;
		int total;
		while (file.ReadLine(line) >= 0)
		{
			if (count < lines)
				text += "  | " + line + "\n";

			count++;
			total += line.Length();
		}

		file.Close();
		return text + "  (" + count.ToString() + " lines, " + total.ToString() + " chars)\n";
	}
}
