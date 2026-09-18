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
	protected static const int BEEP_SET_COUNT = 6;
	protected static const int BEEP_SET_SHORT_RANGE = 0;
	protected static const int BEEP_SET_LONG_RANGE = 1;
	protected static const float VOLUME_STEPS = 10;

	protected ref map<string, int> m_mEar = new map<string, int>();
	protected ref map<string, int> m_mBeepSet = new map<string, int>();
	protected ref map<string, float> m_mVolume = new map<string, float>();
	//! Transmit key index -> transceiver id (LC_Radio.GetId) the player assigned to it
	protected ref map<int, string> m_mKeyAssignments = new map<int, string>();

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
		}

		return "none";
	}

	//------------------------------------------------------------------------------------------------
	static string GetBeepSetDisplayName(int index)
	{
		switch (index)
		{
			case 0: return "TFAR short range beeps";
			case 1: return "TFAR long range beeps";
			case 2: return "TFAR airborne beeps";
			case 3: return "TFAR classic beeps";
			case 4: return "ACRE2 clicks";
		}

		return "No beeps";
	}

	//------------------------------------------------------------------------------------------------
	//! Compact beep set label for the radial menu
	string GetBeepSetShortName(notnull SCR_VONEntryRadio entry)
	{
		switch (GetBeepSetIndex(entry))
		{
			case 0: return "TFAR SW";
			case 1: return "TFAR LR";
			case 2: return "TFAR AB";
			case 3: return "TFAR Classic";
			case 4: return "ACRE";
		}

		return "No beeps";
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

	//------------------------------------------------------------------------------------------------
	static string GetEarDisplayName(int ear)
	{
		switch (ear)
		{
			case EAR_LEFT: return "Left ear";
			case EAR_RIGHT: return "Right ear";
		}

		return "Both ears";
	}
}
