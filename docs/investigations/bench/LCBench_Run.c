// Dev-only micro benchmarks for Lima Charlie hot paths. ClaudeDevTools is never published.
// Called via NET API: APIFunc = "LCBench_Run"

class LCBench_RunRequest : JsonApiStruct
{
	int players;
	int iterations;

	void LCBench_RunRequest()
	{
		RegV("players");
		RegV("iterations");
	}
}

class LCBench_RunResponse : JsonApiStruct
{
	string results;

	void LCBench_RunResponse()
	{
		RegV("results");
	}
}

class LCBench_Portal
{
	int a;
	int b;
}

class LCBench_Run : NetApiHandler
{
	protected ref array<float> m_aDistance = {};
	protected ref array<float> m_aCost = {};
	protected ref array<ref LCBench_Portal> m_aPortals = {};
	protected ref array<ref array<int>> m_aAreaPortals = {};
	protected ref array<int> m_aQueue = {};
	protected ref array<bool> m_aQueued = {};
	protected int m_iAreas;

	override JsonApiStruct GetRequest()
	{
		return new LCBench_RunRequest();
	}

	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		LCBench_RunRequest req = LCBench_RunRequest.Cast(request);
		LCBench_RunResponse resp = new LCBench_RunResponse();
		int players = req.players;
		if (players <= 0)
			players = 30;

		int iterations = req.iterations;
		if (iterations <= 0)
			iterations = 200;

		string text;
		text += BenchCalls();
		text += BenchJson(players, iterations);
		text += BenchParse();
		text += BenchRooms();
		text += BenchIds();
		resp.results = text;
		return resp;
	}

	//------------------------------------------------------------------------------------------------
	static int Add(int a, int b)
	{
		return a + b;
	}

	protected string Line(string name, int totalMs, int count)
	{
		float us = totalMs * 1000.0 / count;
		return name + ": " + us.ToString(-1, 3) + " us/op (" + count.ToString() + " ops, " + totalMs.ToString() + " ms)\n";
	}

	//------------------------------------------------------------------------------------------------
	protected string BenchCalls()
	{
		int count = 2000000;
		int sum;
		int start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			sum = Add(sum, i);
		}

		int callMs = System.GetTickCount() - start;

		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			sum = sum + i;
		}

		int loopMs = System.GetTickCount() - start;
		return Line("static call + loop", callMs, count) + Line("inline add + loop", loopMs, count);
	}

	//------------------------------------------------------------------------------------------------
	//! The writer's players array as built today: five appends to the growing document per player
	protected string BenchJson(int players, int iterations)
	{
		string head = "{\"v\":9,\"seq\":123,\"inGame\":true,\"session\":{\"token\":\"1789377566-123-456-789\",\"playerId\":1,\"playerName\":\"Duke\",\"tsChannel\":\"LimaCharlie\",\"tsChannelPassword\":\"\",\"modVersion\":\"2.0.6\"}";
		head += ",\"self\":{\"alive\":true,\"pos\":[4821.3,12.4,7210.9],\"dir\":[0.123,0.000,0.992],\"tx\":0,\"txFrequency\":0,\"txRadio\":\"\",\"voiceRange\":20.0,\"cleanFraction\":0.35,\"beepFraction\":0.90,\"unlimitedRx\":false,\"radios\":[{\"id\":\"1234:1\",\"freq\":45000,\"range\":1500,\"key\":\"US\",\"rx\":true,\"ear\":0,\"volume\":0.80,\"beep\":\"tfar_sw\",\"halfDuplex\":0}],\"beepVolume\":1.00,\"roomVolume\":0,\"sounds\":[]}";

		vector position = "4821.31 12.42 7210.93";
		int start = System.GetTickCount();
		int length;
		for (int it = 0; it < iterations; it++)
		{
			string json = head + ",\"players\":[";
			bool first = true;
			for (int p = 0; p < players; p++)
			{
				if (!first)
					json += ",";

				first = false;
				json += "{\"id\":" + p.ToString();
				json += ",\"alive\":" + LC_Json.Bool(true);
				json += ",\"pos\":" + LC_Json.Position(position);
				float muffle = 0.6;
				json += ",\"muffle\":" + muffle.ToString(-1, 2);
				float room = 0.35;
				json += ",\"room\":" + room.ToString(-1, 2) + "}";
			}

			json += "],\"links\":[]}";
			length = json.Length();
		}

		int currentMs = System.GetTickCount() - start;

		start = System.GetTickCount();
		for (int it = 0; it < iterations; it++)
		{
			string json = head + ",\"players\":[";
			for (int p = 0; p < players; p++)
			{
				float muffle = 0.6;
				float room = 0.35;
				string player = "{\"id\":" + p.ToString() + ",\"alive\":" + LC_Json.Bool(true) + ",\"pos\":" + LC_Json.Position(position) + ",\"muffle\":" + muffle.ToString(-1, 2) + ",\"room\":" + room.ToString(-1, 2) + "}";
				if (p > 0)
					json += ",";

				json += player;
			}

			json += "],\"links\":[]}";
			length = json.Length();
		}

		int oneAppendMs = System.GetTickCount() - start;

		// Same pieces collected and joined once
		start = System.GetTickCount();
		array<string> parts = {};
		for (int it = 0; it < iterations; it++)
		{
			parts.Clear();
			for (int p = 0; p < players; p++)
			{
				float muffle = 0.6;
				float room = 0.35;
				parts.Insert("{\"id\":" + p.ToString() + ",\"alive\":" + LC_Json.Bool(true) + ",\"pos\":" + LC_Json.Position(position) + ",\"muffle\":" + muffle.ToString(-1, 2) + ",\"room\":" + room.ToString(-1, 2) + "}");
			}

			string joined = SCR_StringHelper.Join(",", parts);
			string json = head + ",\"players\":[" + joined + "],\"links\":[]}";
			length = json.Length();
		}

		int joinMs = System.GetTickCount() - start;

		string label = players.ToString() + " players, " + length.ToString() + " chars";
		return Line("writer json, append per field (" + label + ")", currentMs, iterations)
			+ Line("writer json, one append per player", oneAppendMs, iterations)
			+ Line("writer json, parts + Join", joinMs, iterations);
	}

	//------------------------------------------------------------------------------------------------
	protected string BenchParse()
	{
		string sample = "{\"v\":9,\"seq\":2379,\"gameSeq\":75,\"pluginVersion\":\"2.0.6\",\"inGame\":true,\"tsConnected\":true,\"tsClientId\":75,\"inGameChannel\":true,\"peers\":24,\"selfTalking\":false,\"micMuted\":false,\"talking\":\"3,7,12\",\"radioRx\":\"3,4821.3,12.4,7210.9;7,4900.1,15.0,7100.2;\",\"radioHeard\":\"3,1234:1,45000,0.92;7,1234:1,45000,0.71;\"}";
		int count = 5000;
		int start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			JsonLoadContext load = new JsonLoadContext();
			load.LoadFromString(sample);
			int seq;
			load.ReadValue("seq", seq);
			string pluginVersion;
			bool b;
			int n;
			string s;
			load.ReadValue("pluginVersion", pluginVersion);
			load.ReadValue("tsConnected", b);
			load.ReadValue("tsClientId", n);
			load.ReadValue("inGameChannel", b);
			load.ReadValue("peers", n);
			load.ReadValue("selfTalking", b);
			load.ReadValue("micMuted", b);
			load.ReadValue("talking", s);
			load.ReadValue("radioRx", s);
			load.ReadValue("radioHeard", s);
		}

		int stringMs = System.GetTickCount() - start;
		string result = Line("plugin_state parse from string + 11 reads", stringMs, count);

		string path = "$profile:LimaCharlie/plugin_state.json";
		if (!FileIO.FileExists(path))
			return result + "plugin_state.json not present\n";

		count = 2000;
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			if (!FileIO.FileExists(path))
				continue;

			JsonLoadContext load = new JsonLoadContext();
			load.LoadFromFile(path);
			int seq;
			load.ReadValue("seq", seq);
		}

		int fileMs = System.GetTickCount() - start;
		return result + Line("plugin_state FileExists + LoadFromFile + seq (unchanged poll)", fileMs, count);
	}

	//------------------------------------------------------------------------------------------------
	//! An apartment-sized graph: 117 areas (0 outside), 5 floors of a stairwell, a corridor and 22 rooms,
	//! each room a door to its corridor and windows to the outside until there are 185 usable portals
	protected void BuildGraph()
	{
		m_aPortals.Clear();
		m_iAreas = 117;
		int area = 1;
		array<int> corridors = {};
		array<int> stairs = {};
		array<int> rooms = {};
		for (int floor = 0; floor < 5; floor++)
		{
			int stair = area++;
			int corridor = area++;
			stairs.Insert(stair);
			corridors.Insert(corridor);
			AddPortal(stair, corridor);
			if (floor > 0)
				AddPortal(stairs[floor - 1], stair);

			for (int r = 0; r < 21 && area < m_iAreas; r++)
			{
				int room = area++;
				rooms.Insert(room);
				AddPortal(room, corridor);
			}
		}

		// Ground floor entrance, then windows out of rooms in turn
		AddPortal(0, stairs[0]);
		int w = 0;
		while (m_aPortals.Count() < 185)
		{
			AddPortal(rooms[w % rooms.Count()], 0);
			w++;
		}

		m_aAreaPortals.Clear();
		for (int i = 0; i < m_iAreas; i++)
		{
			m_aAreaPortals.Insert(new array<int>());
		}

		foreach (int index, LCBench_Portal portal : m_aPortals)
		{
			m_aAreaPortals[portal.a].Insert(index);
			m_aAreaPortals[portal.b].Insert(index);
		}

		m_aCost.Clear();
		foreach (int index, LCBench_Portal portal : m_aPortals)
		{
			// Doors mostly closed, windows intact, a few open
			float cost = 0.6;
			if (index % 7 == 0)
				cost = 0.2;

			m_aCost.Insert(cost);
		}
	}

	protected void AddPortal(int a, int b)
	{
		LCBench_Portal portal = new LCBench_Portal();
		portal.a = a;
		portal.b = b;
		m_aPortals.Insert(portal);
	}

	//------------------------------------------------------------------------------------------------
	//! 2.0.5: every portal relaxed both ways per pass until nothing changes
	protected int SolveBellmanFord(int from)
	{
		m_aDistance.Resize(m_iAreas);
		for (int i = 0; i < m_iAreas; i++)
		{
			m_aDistance[i] = 99;
		}

		m_aDistance[from] = 0;
		int passes;
		for (int pass = 0; pass < m_iAreas; pass++)
		{
			passes++;
			bool changed = false;
			foreach (int index, LCBench_Portal portal : m_aPortals)
			{
				if (portal.a >= m_iAreas || portal.b >= m_iAreas)
					continue;

				if (Relax(portal.a, portal.b, m_aCost[index]))
					changed = true;

				if (Relax(portal.b, portal.a, m_aCost[index]))
					changed = true;
			}

			if (!changed)
				break;
		}

		return passes;
	}

	protected bool Relax(int from, int to, float cost)
	{
		float candidate = m_aDistance[from] + cost;
		if (candidate >= m_aDistance[to])
			return false;

		m_aDistance[to] = candidate;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 2.0.6: queue search over each area's own portals
	protected int SolveQueue(int from)
	{
		m_aDistance.Resize(m_iAreas);
		m_aQueued.Resize(m_iAreas);
		for (int i = 0; i < m_iAreas; i++)
		{
			m_aDistance[i] = 99;
			m_aQueued[i] = false;
		}

		m_aQueue.Clear();
		m_aQueue.Insert(from);
		m_aQueued[from] = true;
		m_aDistance[from] = 0;
		int visits;
		int head;
		while (head < m_aQueue.Count())
		{
			int area = m_aQueue[head];
			head++;
			m_aQueued[area] = false;
			array<int> links = m_aAreaPortals[area];
			foreach (int index : links)
			{
				visits++;
				LCBench_Portal portal = m_aPortals[index];
				int next = portal.a;
				if (next == area)
					next = portal.b;

				float reached = m_aDistance[area] + m_aCost[index];
				if (reached >= m_aDistance[next])
					continue;

				m_aDistance[next] = reached;
				if (!m_aQueued[next])
				{
					m_aQueue.Insert(next);
					m_aQueued[next] = true;
				}
			}
		}

		return visits;
	}

	protected string BenchRooms()
	{
		BuildGraph();
		int count = 300;
		int passes;
		int start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			passes = SolveBellmanFord(i % 2 * 60);
		}

		int bfMs = System.GetTickCount() - start;
		float d1 = m_aDistance[100];
		string check = d1.ToString(-1, 2);

		count = 3000;
		int visits;
		start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			visits = SolveQueue(i % 2 * 60);
		}

		int queueMs = System.GetTickCount() - start;
		float d2 = m_aDistance[100];
		string check2 = d2.ToString(-1, 2);
		string label = m_iAreas.ToString() + " areas, " + m_aPortals.Count().ToString() + " portals";
		return Line("rooms 2.0.5 Bellman-Ford solve (" + label + ", " + passes.ToString() + " passes, d[100]=" + check + ")", bfMs, 300)
			+ Line("rooms 2.0.6 queue solve (" + visits.ToString() + " portal ends, d[100]=" + check2 + ")", queueMs, count);
	}

	//------------------------------------------------------------------------------------------------
	//! The string side of LC_Radio.GetId and a settings lookup keyed by it
	protected string BenchIds()
	{
		map<string, int> ears = new map<string, int>();
		for (int i = 0; i < 12; i++)
		{
			int radio = 1000 + i;
			ears.Set(radio.ToString() + ":1", i % 3);
		}

		int count = 200000;
		int found;
		int start = System.GetTickCount();
		for (int i = 0; i < count; i++)
		{
			int id = 1000 + i % 12;
			int transceiver = 1;
			string key = id.ToString() + ":" + transceiver.ToString();
			int ear;
			if (ears.Find(key, ear))
				found++;
		}

		int idMs = System.GetTickCount() - start;
		return Line("radio id string + map<string> lookup", idMs, count);
	}
}
