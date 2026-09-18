//------------------------------------------------------------------------------------------------
//! Sounds for the plugin to play (UI tones, sample beeps), sent as game_state.json self.sounds. The most
//! recent few are always included with increasing seq numbers, so none are lost between writes.
class LC_SoundQueue
{
	//! Must not exceed the plugin's LC_GAME_STATE_MAX_SOUNDS
	protected static const int KEEP = 8;

	protected ref array<string> m_aEvents = {};
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

		while (m_aEvents.Count() > KEEP)
		{
			m_aEvents.RemoveOrdered(0);
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
