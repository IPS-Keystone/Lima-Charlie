
// Dev-only: would a TargetLayers filter give LC_Occlusion the same answers for fewer filter calls?
// APIFunc = "LCBench_Layers". Modes: "find" (densest building cells of the world), "spawn" (a prefab at x z),
// "rays" (random head-height rays within radius of x z), "bodies" (rays through every character and vehicle
// within radius of x z).
class LCBench_LayersRequest : JsonApiStruct
{
	string mode;
	float x;
	float z;
	float radius;
	float rays;
	string prefab;

	void LCBench_LayersRequest()
	{
		RegV("mode");
		RegV("x");
		RegV("z");
		RegV("radius");
		RegV("rays");
		RegV("prefab");
	}
}

class LCBench_LayersResponse : JsonApiStruct
{
	string results;

	void LCBench_LayersResponse()
	{
		RegV("results");
	}
}

//! LC_Occlusion with the layers open to the caller and every filter call counted
class LCBench_LayerOcclusion : LC_Occlusion
{
	int m_iCalls;
	bool m_bRecord;
	ref map<string, int> m_mMet = new map<string, int>();

	protected bool CountingFilter(notnull IEntity entity, vector start = "0 0 0", vector dir = "0 0 0")
	{
		m_iCalls++;
		if (m_bRecord)
		{
			string kind = entity.ClassName();
			if (ChimeraCharacter.Cast(entity))
			{
				kind = "character";
			}
			else
			{
				IEntity parent = entity.GetParent();
				while (parent)
				{
					if (ChimeraCharacter.Cast(parent))
					{
						kind = "worn or carried";
						break;
					}

					parent = parent.GetParent();
				}
			}

			m_mMet.Set(kind, m_mMet.Get(kind) + 1);
		}

		return FilterCover(entity, start, dir);
	}

	bool Blocked(notnull BaseWorld world, vector from, vector to, int targetLayers, array<IEntity> exclude)
	{
		m_Trace.Flags = TraceFlags.DEFAULT | TraceFlags.ANY_CONTACT;
		m_Trace.LayerMask = EPhysicsLayerDefs.Projectile;
		m_Trace.TargetLayers = targetLayers;
		m_Trace.ExcludeArray = exclude;
		m_Trace.Start = from;
		m_Trace.End = to;
		m_Trace.TraceEnt = null;
		return world.TraceMove(m_Trace, CountingFilter) < 1;
	}

	string Cover()
	{
		IEntity entity = m_Trace.TraceEnt;
		if (!entity)
			return "world";

		string prefab = SCR_ResourceNameUtils.GetPrefabName(entity);
		int lastSlash = prefab.LastIndexOf("/");
		if (lastSlash >= 0)
			prefab = prefab.Substring(lastSlash + 1, prefab.Length() - lastSlash - 1);

		if (prefab.IsEmpty())
			prefab = entity.ClassName();

		return prefab;
	}

	string Met()
	{
		string text;
		foreach (string kind, int count : m_mMet)
		{
			text += " " + kind + "=" + count.ToString();
		}

		return text;
	}
}

class LCBench_Layers : NetApiHandler
{
	protected static const int VARIANTS = 5;
	protected static const int CELL_M = 250;

	protected ref map<int, int> m_mCells = new map<int, int>();
	protected ref array<IEntity> m_aBodies = {};
	protected int m_iSeen;

	override JsonApiStruct GetRequest()
	{
		return new LCBench_LayersRequest();
	}

	protected string VariantName(int variant)
	{
		if (variant == 1)
			return "T Static|Dynamic|Vehicles|Terrain";

		if (variant == 2)
			return "T FireGeometry";

		if (variant == 3)
			return "T FireGeometry|Terrain";

		if (variant == 4)
			return "T Static|Vehicles|Terrain";

		return "Projectile (now)";
	}

	protected int VariantLayers(int variant)
	{
		int vehicles = EPhysicsLayerDefs.VehicleSimple | EPhysicsLayerDefs.VehicleComplex;
		if (variant == 1)
			return EPhysicsLayerDefs.Static | EPhysicsLayerDefs.Dynamic | vehicles | EPhysicsLayerDefs.Terrain;

		if (variant == 2)
			return EPhysicsLayerDefs.FireGeometry;

		if (variant == 3)
			return EPhysicsLayerDefs.FireGeometry | EPhysicsLayerDefs.Terrain;

		if (variant == 4)
			return EPhysicsLayerDefs.Static | vehicles | EPhysicsLayerDefs.Terrain;

		return 0;
	}

	protected bool CountBuilding(IEntity entity)
	{
		m_iSeen++;
		WorldSubsceneComponent subscene = WorldSubsceneComponent.Cast(entity.FindComponent(WorldSubsceneComponent));
		if (!subscene || subscene.GetAreaCount() < 2)
			return true;

		vector origin = entity.GetOrigin();
		int cellX = origin[0] / CELL_M;
		int cellZ = origin[2] / CELL_M;
		int key = cellX * 1000 + cellZ;
		m_mCells.Set(key, m_mCells.Get(key) + 1);
		return true;
	}

	protected bool CollectBody(IEntity entity)
	{
		if (ChimeraCharacter.Cast(entity) || Vehicle.Cast(entity))
			m_aBodies.Insert(entity);

		return true;
	}

	protected string Find(notnull BaseWorld world)
	{
		vector mins;
		vector maxs;
		world.GetBoundBox(mins, maxs);
		vector centre = (mins + maxs) * 0.5;
		float radius = vector.Distance(mins, maxs) * 0.5;
		m_mCells.Clear();
		m_iSeen = 0;
		world.QueryEntitiesBySphere(centre, radius, CountBuilding, null, EQueryEntitiesFlags.STATIC);
		string text = "world " + mins.ToString() + " to " + maxs.ToString() + ", static entities " + m_iSeen.ToString() + ", cells with buildings " + m_mCells.Count().ToString() + "\n";
		for (int n = 0; n < 6; n++)
		{
			int bestKey = -1;
			int best;
			foreach (int key, int count : m_mCells)
			{
				if (count > best)
				{
					best = count;
					bestKey = key;
				}
			}

			if (bestKey < 0)
				break;

			m_mCells.Remove(bestKey);
			int keyX = bestKey / 1000;
			int keyZ = bestKey - keyX * 1000;
			float cx = keyX * CELL_M + CELL_M * 0.5;
			float cz = keyZ * CELL_M + CELL_M * 0.5;
			text += "cell x " + cx.ToString() + " z " + cz.ToString() + ": " + best.ToString() + " buildings with rooms\n";
		}

		return text;
	}

	//! Runs every variant over the same rays: agreement with what the mod does now, filter calls, time
	protected string Compare(notnull BaseWorld world, notnull array<vector> starts, notnull array<vector> ends, array<IEntity> exclude)
	{
		string text;
		int count = starts.Count();
		array<bool> now = {};
		for (int variant = 0; variant < VARIANTS; variant++)
		{
			LCBench_LayerOcclusion occlusion = new LCBench_LayerOcclusion();
			occlusion.m_bRecord = true;
			int layers = VariantLayers(variant);
			int blockedCount;
			int lost;
			int gained;
			string lostExamples;
			string gainedExamples;
			for (int i = 0; i < count; i++)
			{
				bool blocked = occlusion.Blocked(world, starts[i], ends[i], layers, exclude);
				if (blocked)
					blockedCount++;

				if (variant == 0)
				{
					now.Insert(blocked);
					continue;
				}

				if (now[i] && !blocked)
				{
					lost++;
					if (lost <= 6)
					{
						// What blocks it today
						LCBench_LayerOcclusion current = new LCBench_LayerOcclusion();
						current.Blocked(world, starts[i], ends[i], 0, exclude);
						lostExamples += " " + current.Cover();
					}
				}
				else if (!now[i] && blocked)
				{
					gained++;
					if (gained <= 6)
						gainedExamples += " " + occlusion.Cover();
				}
			}

			int calls = occlusion.m_iCalls;
			string met = occlusion.Met();
			occlusion.m_bRecord = false;
			int reps = 25;
			int start = System.GetTickCount();
			for (int r = 0; r < reps; r++)
			{
				for (int i = 0; i < count; i++)
				{
					occlusion.Blocked(world, starts[i], ends[i], layers, exclude);
				}
			}

			int ms = System.GetTickCount() - start;
			float us = ms * 1000.0 / (reps * count);
			float perTrace = calls;
			perTrace = perTrace / count;
			text += VariantName(variant) + ": blocked " + blockedCount.ToString() + "/" + count.ToString() + ", filter calls " + calls.ToString() + " (" + perTrace.ToString(-1, 2) + " per trace), " + us.ToString(-1, 2) + " us/trace\n";
			if (variant > 0)
			{
				text += "   blocked now but not with this: " + lost.ToString() + lostExamples + "\n";
				text += "   clear now but blocked with this: " + gained.ToString() + gainedExamples + "\n";
			}

			text += "   met:" + met + "\n";
		}

		return text;
	}

	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		LCBench_LayersRequest req = LCBench_LayersRequest.Cast(request);
		LCBench_LayersResponse resp = new LCBench_LayersResponse();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			resp.results = "no game world: play mode only";
			return resp;
		}

		if (req.mode == "find")
		{
			resp.results = Find(world);
			return resp;
		}

		vector centre = Vector(req.x, world.GetSurfaceY(req.x, req.z), req.z);
		if (req.mode == "spawn")
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = centre;
			IEntity spawned = GetGame().SpawnEntityPrefab(Resource.Load(req.prefab), world, params);
			bool ok = spawned != null;
			resp.results = "spawned " + req.prefab + " " + ok.ToString() + " at " + centre.ToString();
			return resp;
		}

		float radius = req.radius;
		if (radius <= 0)
			radius = 150;

		array<vector> starts = {};
		array<vector> ends = {};
		array<IEntity> exclude = {};
		if (req.mode == "bodies")
		{
			m_aBodies.Clear();
			world.QueryEntitiesBySphere(centre, radius, CollectBody, null, EQueryEntitiesFlags.ALL);
			string text = "bodies " + m_aBodies.Count().ToString() + "\n";
			foreach (IEntity body : m_aBodies)
			{
				starts.Clear();
				ends.Clear();
				vector origin = body.GetOrigin();
				// Eight directions through the body at three heights, 4 m either side
				for (int h = 0; h < 3; h++)
				{
					float height = 0.9 + h * 0.3;
					for (int d = 0; d < 8; d++)
					{
						float angle = d * Math.PI * 0.125;
						vector offset = Vector(Math.Cos(angle) * 4, 0, Math.Sin(angle) * 4);
						vector middle = origin;
						middle[1] = middle[1] + height;
						starts.Insert(middle - offset);
						ends.Insert(middle + offset);
					}
				}

				text += "--- through " + SCR_ResourceNameUtils.GetPrefabName(body) + " at " + origin.ToString() + "\n";
				text += Compare(world, starts, ends, exclude);
			}

			resp.results = text;
			return resp;
		}

		// Random pairs of heads within the radius, up to 60 m apart
		int rays = req.rays;
		if (rays <= 0)
			rays = 400;

		RandomGenerator random = new RandomGenerator();
		random.SetSeed(20261110);
		for (int i = 0; i < rays; i++)
		{
			float angle = random.RandFloatXY(0, Math.PI2);
			float reach = radius * Math.Sqrt(random.RandFloatXY(0, 1));
			float fx = centre[0] + Math.Cos(angle) * reach;
			float fz = centre[2] + Math.Sin(angle) * reach;
			float heading = random.RandFloatXY(0, Math.PI2);
			float length = random.RandFloatXY(4, 60);
			float tx = fx + Math.Cos(heading) * length;
			float tz = fz + Math.Sin(heading) * length;
			starts.Insert(Vector(fx, world.GetSurfaceY(fx, fz) + 1.7, fz));
			ends.Insert(Vector(tx, world.GetSurfaceY(tx, tz) + 1.7, tz));
		}

		resp.results = "rays " + rays.ToString() + " within " + radius.ToString() + " m of " + centre.ToString() + "\n" + Compare(world, starts, ends, exclude);
		return resp;
	}
}
