//------------------------------------------------------------------------------------------------
//! Local radio preferences, kept for the session: per radio channel its ear, beep set and volume, and which
//! radio channel each Lima Charlie transmit key sends on.
class LC_RadioSettings
{
	static const int EAR_BOTH = 0;
	static const int EAR_LEFT = 1;
	static const int EAR_RIGHT = 2;
	static const int TRANSMIT_KEY_COUNT = 4;

	protected static const int EAR_COUNT = 3;
	protected static const int BEEP_SET_COUNT = 7;
	protected static const int BEEP_SET_SHORT_RANGE = 0;
	protected static const int BEEP_SET_LONG_RANGE = 1;
	protected static const float VOLUME_STEPS = 10;

	protected ref map<string, int> m_mEar = new map<string, int>();
	protected ref map<string, int> m_mBeepSet = new map<string, int>();
	protected ref map<string, float> m_mVolume = new map<string, float>();
	//! Transmit key index -> transceiver id (LC_Radio.GetId) the player assigned to it
	protected ref map<int, string> m_mKeyAssignments = new map<int, string>();
	//! One beep volume for every channel, on top of each channel's own volume
	protected float m_fBeepVolume = 1;

	//------------------------------------------------------------------------------------------------
	int GetEar(notnull SCR_VONEntryRadio entry)
	{
		int ear;
		if (m_mEar.Find(LC_Radio.GetId(entry), ear))
			return ear;

		return EAR_BOTH;
	}

	//------------------------------------------------------------------------------------------------
	//! Both, left, right
	int CycleEar(notnull SCR_VONEntryRadio entry)
	{
		int ear = (GetEar(entry) + 1) % EAR_COUNT;
		m_mEar.Set(LC_Radio.GetId(entry), ear);
		return ear;
	}

	//------------------------------------------------------------------------------------------------
	//! 0 silent to 1 full
	float GetVolume(notnull SCR_VONEntryRadio entry)
	{
		float volume;
		if (m_mVolume.Find(LC_Radio.GetId(entry), volume))
			return volume;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! One 10% step up (direction 1) or down (-1); returns the new volume
	float AdjustVolume(notnull SCR_VONEntryRadio entry, int direction)
	{
		float volume = Math.Round(GetVolume(entry) * VOLUME_STEPS + direction) / VOLUME_STEPS;
		volume = Math.Clamp(volume, 0, 1);
		m_mVolume.Set(LC_Radio.GetId(entry), volume);
		return volume;
	}

	//------------------------------------------------------------------------------------------------
	//! One 10% step down, wrapping from silent back to full; returns the new volume
	float CycleVolume(notnull SCR_VONEntryRadio entry)
	{
		float volume = Math.Round(GetVolume(entry) * VOLUME_STEPS - 1) / VOLUME_STEPS;
		if (volume < 0)
			volume = 1;

		m_mVolume.Set(LC_Radio.GetId(entry), volume);
		return volume;
	}

	//------------------------------------------------------------------------------------------------
	//! 0 silent to 1 full, applied to every channel's beeps on top of that channel's own volume
	float GetBeepVolume()
	{
		return m_fBeepVolume;
	}

	//------------------------------------------------------------------------------------------------
	//! One 10% step down, wrapping from silent back to full; returns the new volume
	float CycleBeepVolume()
	{
		float volume = Math.Round(m_fBeepVolume * VOLUME_STEPS - 1) / VOLUME_STEPS;
		if (volume < 0)
			volume = 1;

		m_fBeepVolume = volume;
		return volume;
	}

	//------------------------------------------------------------------------------------------------
	//! What one of this channel's beeps is played at: the channel's volume scaled by the global beep volume
	float GetBeepGain(notnull SCR_VONEntryRadio entry)
	{
		return GetVolume(entry) * m_fBeepVolume;
	}

	//------------------------------------------------------------------------------------------------
	//! Sound set folder name in the plugin
	string GetBeepSet(notnull SCR_VONEntryRadio entry)
	{
		return GetBeepSetName(GetBeepSetIndex(entry));
	}

	//------------------------------------------------------------------------------------------------
	int CycleBeepSet(notnull SCR_VONEntryRadio entry)
	{
		int index = (GetBeepSetIndex(entry) + 1) % BEEP_SET_COUNT;
		m_mBeepSet.Set(LC_Radio.GetId(entry), index);
		return index;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetBeepSetIndex(notnull SCR_VONEntryRadio entry)
	{
		int index;
		if (m_mBeepSet.Find(LC_Radio.GetId(entry), index))
			return index;

		if (LC_Radio.IsLongRange(entry))
			return BEEP_SET_LONG_RANGE;

		return BEEP_SET_SHORT_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Channel a transmit key sends on: the channel assigned to it; with none assigned, the channel at the key's
	//! position in the radio list, unless the player has assigned that channel to another key.
	SCR_VONEntryRadio GetKeyEntry(int key, notnull array<SCR_VONEntryRadio> entries)
	{
		string assigned;
		if (m_mKeyAssignments.Find(key, assigned))
		{
			foreach (SCR_VONEntryRadio entry : entries)
			{
				if (entry && LC_Radio.GetId(entry) == assigned)
					return entry;
			}

			// Assigned radio is not carried right now
			return null;
		}

		if (!entries.IsIndexValid(key))
			return null;

		SCR_VONEntryRadio defaultEntry = entries[key];
		if (!defaultEntry || GetAssignedKey(defaultEntry) >= 0)
			return null;

		return defaultEntry;
	}

	//------------------------------------------------------------------------------------------------
	//! Transmit key a channel is on (assigned or by default), or -1
	int GetKey(notnull SCR_VONEntryRadio entry, notnull array<SCR_VONEntryRadio> entries)
	{
		for (int key = 0; key < TRANSMIT_KEY_COUNT; key++)
		{
			if (GetKeyEntry(key, entries) == entry)
				return key;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a channel to the next transmit key (1, 2, 3, 4, 1...), taking that key from whatever it sent on.
	//! Returns the new key index.
	int CycleKey(notnull SCR_VONEntryRadio entry, notnull array<SCR_VONEntryRadio> entries)
	{
		int key = (GetKey(entry, entries) + 1) % TRANSMIT_KEY_COUNT;

		int previous = GetAssignedKey(entry);
		if (previous >= 0)
			m_mKeyAssignments.Remove(previous);

		m_mKeyAssignments.Set(key, LC_Radio.GetId(entry));
		return key;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetAssignedKey(notnull SCR_VONEntryRadio entry)
	{
		string id = LC_Radio.GetId(entry);
		foreach (int key, string assigned : m_mKeyAssignments)
		{
			if (assigned == id)
				return key;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static string GetBeepSetName(int index)
	{
		switch (index)
		{
			case 0: return "tfar_sw";
			case 1: return "tfar_lr";
			case 2: return "tfar_ab";
			case 3: return "tfar_classic";
			case 4: return "acre";
			case 5: return "vanilla";
		}

		return "none";
	}

	//------------------------------------------------------------------------------------------------
	//! Which sound in a set to play as a sample when a channel's settings change. The game's own radio
	//! beeps only at the end of a transmission, so that set has no start beep to demonstrate.
	string GetSampleName(notnull SCR_VONEntryRadio entry)
	{
		if (GetBeepSetIndex(entry) == 5)
			return "local_end";

		return "local_start";
	}

	//------------------------------------------------------------------------------------------------
	//! Compact beep set label for the radial menu
	string GetBeepSetShortName(notnull SCR_VONEntryRadio entry)
	{
		string name = "No beeps";
		switch (GetBeepSetIndex(entry))
		{
			case 0: name = "TFAR SW"; break;
			case 1: name = "TFAR LR"; break;
			case 2: name = "TFAR AB"; break;
			case 3: name = "TFAR Classic"; break;
			case 4: name = "ACRE"; break;
			case 5: name = "Vanilla"; break;
		}

		// The beep volume is one setting for every channel, so it rides along on each channel's beep label
		// rather than having a line of its own. At full volume there is nothing worth saying.
		if (name == "No beeps" || m_fBeepVolume >= 1)
			return name;

		int percent = Math.Round(m_fBeepVolume * 100);
		return name + " " + percent.ToString() + "%";
	}

	//------------------------------------------------------------------------------------------------
	//! L, R or C (centre) as in Enhanced Radio
	static string GetEarShortName(int ear)
	{
		switch (ear)
		{
			case EAR_LEFT: return "L";
			case EAR_RIGHT: return "R";
		}

		return "C";
	}
}
