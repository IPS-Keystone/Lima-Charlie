//------------------------------------------------------------------------------------------------
class LC_RadioLink
{
	int m_iPlayerId;
	string m_sRadioId;
	vector m_vFrom;
	vector m_vTo;
	//! The highest hill above the line of sight, and every hill, highest first; both already scaled by the
	//! server's terrain setting
	float m_fClearance;
	ref array<float> m_aHills = {};
	int m_iNextTick;
	int m_iLastRequestedTick;
}

//------------------------------------------------------------------------------------------------
//! Terrain clearance between the local player and the radio transmitters the plugin asks about
//! (plugin_state.json "radioRx"), recomputed when either end moves. Sent back as game_state.json "links".
//! Kept per transmitting radio rather than per player, since two radios of one player need not be in the
//! same place.
//!
//! The server's terrain setting is applied here rather than in the plugin: the plugin takes a share of
//! the radio's range for each hill that grows with its height, so scaling the heights makes every hill
//! count for more or less, and the bridge protocol stays as it is.
class LC_RadioLinks
{
	protected static const int REFRESH_MS = 1000;
	protected static const float MOVE_REFRESH_M = 10;
	//! A link the plugin stopped asking about this long ago is dropped from the file
	protected static const int STALE_MS = 1000;
	//! The plugin's requests change at most this often, and a 10 m move takes far longer than this
	protected static const int UPDATE_MS = 50;

	protected ref map<string, ref LC_RadioLink> m_mLinks = new map<string, ref LC_RadioLink>();
	protected ref array<string> m_aStale = {};
	protected ref array<float> m_aHills = {};
	protected int m_iRevision;
	protected int m_iNextUpdateTick;

	//------------------------------------------------------------------------------------------------
	//! One transmitting radio: its player and its id among that player's radios
	static string GetKey(int playerId, string radioId)
	{
		return playerId.ToString() + "/" + radioId;
	}

	//------------------------------------------------------------------------------------------------
	void Update(int now, notnull map<string, ref LC_RadioRequest> requests, vector listenerPosition, float terrainFactor = 1)
	{
		if (now < m_iNextUpdateTick)
			return;

		m_iNextUpdateTick = now + UPDATE_MS;
		m_aStale.Clear();
		foreach (string key, LC_RadioLink link : m_mLinks)
		{
			if (!requests.Contains(key) && now - link.m_iLastRequestedTick > STALE_MS)
				m_aStale.Insert(key);
		}

		foreach (string key : m_aStale)
		{
			m_mLinks.Remove(key);
		}

		if (!m_aStale.IsEmpty())
			m_iRevision++;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		float moveSq = MOVE_REFRESH_M * MOVE_REFRESH_M;
		foreach (string key, LC_RadioRequest request : requests)
		{
			// A streamed-in transmitter's head is more current than the position in its last announcement, for
			// a radio they are carrying. Anywhere else the announced antenna is where the signal starts.
			vector from = request.m_vPosition;
			if (LC_Radio.IsOnBody(request.m_eSource))
			{
				ChimeraCharacter character = ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(request.m_iPlayerId));
				if (character)
					from = character.EyePosition();
			}

			LC_RadioLink link = m_mLinks.Get(key);
			if (link)
				link.m_iLastRequestedTick = now;

			if (link && now < link.m_iNextTick && vector.DistanceSq(link.m_vFrom, from) < moveSq && vector.DistanceSq(link.m_vTo, listenerPosition) < moveSq)
				continue;

			float clearance = LC_Terrain.Hills(from, listenerPosition, m_aHills) * terrainFactor;
			for (int i = 0; i < m_aHills.Count(); i++)
			{
				m_aHills[i] = m_aHills[i] * terrainFactor;
			}

			if (!link)
			{
				link = new LC_RadioLink();
				link.m_iPlayerId = request.m_iPlayerId;
				link.m_sRadioId = request.m_sRadioId;
				link.m_iLastRequestedTick = now;
				m_mLinks.Set(key, link);
				m_iRevision++;
			}
			else if (HillsChanged(link.m_aHills, m_aHills))
			{
				m_iRevision++;
			}

			link.m_fClearance = clearance;
			link.m_aHills.Copy(m_aHills);
			link.m_vFrom = from;
			link.m_vTo = listenerPosition;
			link.m_iNextTick = now + REFRESH_MS;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A different number of hills, or any of them a metre or more higher or lower
	protected bool HillsChanged(notnull array<float> before, notnull array<float> after)
	{
		if (before.Count() != after.Count())
			return true;

		for (int i = 0; i < before.Count(); i++)
		{
			if (Math.AbsFloat(before[i] - after[i]) >= 1)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Changes whenever the links worth telling the plugin about change
	int GetRevision()
	{
		return m_iRevision;
	}

	//------------------------------------------------------------------------------------------------
	string BuildJson()
	{
		string json = "[";
		bool first = true;
		foreach (string key, LC_RadioLink link : m_mLinks)
		{
			if (!first)
				json += ",";

			first = false;
			json += "{\"id\":" + link.m_iPlayerId.ToString() + ",\"radio\":" + LC_Json.String(link.m_sRadioId) + ",\"clearance\":" + link.m_fClearance.ToString(-1, 0) + ",\"hills\":[";
			for (int i = 0; i < link.m_aHills.Count(); i++)
			{
				if (i > 0)
					json += ",";

				json += link.m_aHills[i].ToString(-1, 0);
			}

			json += "]}";
		}

		return json + "]";
	}
}
