//------------------------------------------------------------------------------------------------
//! Sounds for the plugin to play (UI tones, sample beeps), sent as game_state.json self.sounds. The most
//! recent few are always included with increasing seq numbers, so none are lost between writes.
class LC_SoundQueue
{
	//! Must not exceed the plugin's LC_GAME_STATE_MAX_SOUNDS
	protected static const int KEEP = 8;
	//! An event is repeated for this long and then dropped. The plugin reads within a few milliseconds of the
	//! write an event forces, so this is far longer than it needs to be; without it every write from then on
	//! would carry the same played events again.
	protected static const int KEEP_MS = 1000;

	protected ref array<string> m_aEvents = {};
	protected ref array<int> m_aAddedTicks = {};
	protected int m_iSeq;

	//------------------------------------------------------------------------------------------------
	//! soundSet is a sound set folder in the plugin, name a WAV in it without extension
	void Add(string soundSet, string name, int ear = 0, float volume = 1)
	{
		m_iSeq++;
		string json = "{\"seq\":" + m_iSeq.ToString();
		json += ",\"set\":" + LC_Json.String(soundSet);
		json += ",\"name\":" + LC_Json.String(name);
		json += ",\"ear\":" + ear.ToString();
		json += ",\"volume\":" + volume.ToString(-1, 2) + "}";
		m_aEvents.Insert(json);
		m_aAddedTicks.Insert(System.GetTickCount());

		while (m_aEvents.Count() > KEEP)
		{
			m_aEvents.RemoveOrdered(0);
			m_aAddedTicks.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops events the plugin has long since played
	protected void Expire()
	{
		int now = System.GetTickCount();
		while (!m_aEvents.IsEmpty() && now - m_aAddedTicks[0] > KEEP_MS)
		{
			m_aEvents.RemoveOrdered(0);
			m_aAddedTicks.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	int GetSeq()
	{
		return m_iSeq;
	}

	//------------------------------------------------------------------------------------------------
	string BuildJson()
	{
		Expire();

		string json = "[";
		foreach (int i, string sound : m_aEvents)
		{
			if (i > 0)
				json += ",";

			json += sound;
		}

		return json + "]";
	}
}
