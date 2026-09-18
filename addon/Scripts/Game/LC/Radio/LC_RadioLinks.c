//------------------------------------------------------------------------------------------------
class LC_RadioLink
{
	vector m_vFrom;
	vector m_vTo;
	float m_fClearance;
	int m_iNextTick;
	int m_iLastRequestedTick;
}

//------------------------------------------------------------------------------------------------
//! Terrain clearance between the local player and the radio transmitters the plugin asks about
//! (plugin_state.json "radioRx"), recomputed when either end moves. Sent back as game_state.json "links".
//!
//! The server's terrain setting is applied here rather than in the plugin: the plugin's model is
//! effective distance = d + h*k + h*k*(d/2000), so scaling the clearance h scales the whole terrain
//! penalty exactly as scaling the coefficient k would, and the bridge protocol stays as it is.
class LC_RadioLinks
{
	protected static const int REFRESH_MS = 1000;
	protected static const float MOVE_REFRESH_M = 10;
	//! A link the plugin stopped asking about this long ago is dropped from the file
	protected static const int STALE_MS = 1000;

	protected ref map<int, ref LC_RadioLink> m_mLinks = new map<int, ref LC_RadioLink>();
	protected ref array<int> m_aStale = {};
	protected int m_iRevision;

	//------------------------------------------------------------------------------------------------
	void Update(int now, notnull map<int, vector> requests, vector listenerPosition, float terrainFactor = 1)
	{
		m_aStale.Clear();
		foreach (int playerId, LC_RadioLink link : m_mLinks)
		{
			if (!requests.Contains(playerId) && now - link.m_iLastRequestedTick > STALE_MS)
				m_aStale.Insert(playerId);
		}

		foreach (int playerId : m_aStale)
		{
			m_mLinks.Remove(playerId);
		}

		if (!m_aStale.IsEmpty())
			m_iRevision++;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		float moveSq = MOVE_REFRESH_M * MOVE_REFRESH_M;
		foreach (int playerId, vector announcedPosition : requests)
		{
			// A streamed-in transmitter's head is more current than the position in its last announcement
			vector from = announcedPosition;
			ChimeraCharacter character = ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (character)
				from = character.EyePosition();

			LC_RadioLink link = m_mLinks.Get(playerId);
			if (link)
				link.m_iLastRequestedTick = now;

			if (link && now < link.m_iNextTick && vector.DistanceSq(link.m_vFrom, from) < moveSq && vector.DistanceSq(link.m_vTo, listenerPosition) < moveSq)
				continue;

			float clearance = LC_Terrain.Clearance(from, listenerPosition) * terrainFactor;
			if (!link)
			{
				link = new LC_RadioLink();
				link.m_iLastRequestedTick = now;
				m_mLinks.Set(playerId, link);
				m_iRevision++;
			}
			else if (Math.AbsFloat(clearance - link.m_fClearance) >= 1)
			{
				m_iRevision++;
			}

			link.m_fClearance = clearance;
			link.m_vFrom = from;
			link.m_vTo = listenerPosition;
			link.m_iNextTick = now + REFRESH_MS;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Changes whenever the links worth telling the plugin about change
	int GetRevision()
	{
		return m_iRevision;
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		if (m_mLinks.IsEmpty())
			return;

		m_mLinks.Clear();
		m_iRevision++;
	}

	//------------------------------------------------------------------------------------------------
	string BuildJson()
	{
		string json = "[";
		bool first = true;
		foreach (int playerId, LC_RadioLink link : m_mLinks)
		{
			if (!first)
				json += ",";

			first = false;
			json += "{\"id\":" + playerId.ToString() + ",\"clearance\":" + link.m_fClearance.ToString(-1, 0) + "}";
		}

		return json + "]";
	}
}
