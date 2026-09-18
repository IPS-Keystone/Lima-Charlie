//------------------------------------------------------------------------------------------------
//! The named frequencies this server uses, packed on the server and unpacked on each client.
//!
//! Held statically because the VON display reads it and has no route back to LC_Client. It is client
//! state only: the server builds the string once per session and sends it with the other settings.
class LC_ChannelLabels
{
	//! "frequencyKHz,colour,name;frequencyKHz,colour,name;"
	protected static const string ENTRY_SEPARATOR = ";";
	protected static const string FIELD_SEPARATOR = ",";

	protected static ref map<int, ref LC_ChannelLabel> s_mByFrequency = new map<int, ref LC_ChannelLabel>();
	protected static LC_EChannelNaming s_eNaming = LC_EChannelNaming.HYBRID;
	protected static ref array<string> s_aEntries = {};
	protected static ref array<string> s_aFields = {};

	//------------------------------------------------------------------------------------------------
	//! Server side: flattens the configured list into one string for the session RPC
	static string Pack(array<ref LC_ChannelLabel> labels)
	{
		if (!labels)
			return string.Empty;

		string packed;
		foreach (LC_ChannelLabel label : labels)
		{
			int frequency = label.GetFrequencyKHz();
			string name = Sanitise(label.m_sName);
			if (frequency <= 0 || name.IsEmpty())
				continue;

			int colour = label.m_eColour;
			packed += frequency.ToString() + FIELD_SEPARATOR + colour.ToString() + FIELD_SEPARATOR + name + ENTRY_SEPARATOR;
		}

		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! Client side: replaces whatever we had with what the server just sent
	static void Unpack(string packed)
	{
		s_mByFrequency.Clear();

		s_aEntries.Clear();
		packed.Split(ENTRY_SEPARATOR, s_aEntries, true);
		foreach (string entry : s_aEntries)
		{
			s_aFields.Clear();
			entry.Split(FIELD_SEPARATOR, s_aFields, false);
			if (s_aFields.Count() != 3)
				continue;

			int frequency = s_aFields[0].ToInt();
			if (frequency <= 0)
				continue;

			LC_ChannelLabel label = new LC_ChannelLabel();
			label.m_fFrequencyMHz = frequency / 1000.0;
			label.m_eColour = s_aFields[1].ToInt();
			label.m_sName = s_aFields[2];
			s_mByFrequency.Set(frequency, label);
		}

		if (!s_mByFrequency.IsEmpty())
			Print("[LC] " + s_mByFrequency.Count().ToString() + " named channels", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The label for a frequency in kHz, or null when that frequency has no name
	static LC_ChannelLabel Find(int frequencyKHz)
	{
		LC_ChannelLabel label;
		s_mByFrequency.Find(frequencyKHz, label);
		return label;
	}

	//------------------------------------------------------------------------------------------------
	static void SetNaming(LC_EChannelNaming naming)
	{
		s_eNaming = naming;
	}

	//------------------------------------------------------------------------------------------------
	static LC_EChannelNaming GetNaming()
	{
		return s_eNaming;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEmpty()
	{
		return s_mByFrequency.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	static Color GetColour(LC_EChannelColour colour)
	{
		switch (colour)
		{
			case LC_EChannelColour.RED:
				return Color.FromSRGBA(232, 74, 74, 255);

			case LC_EChannelColour.ORANGE:
				return Color.FromSRGBA(226, 143, 41, 255);

			case LC_EChannelColour.YELLOW:
				return Color.FromSRGBA(226, 201, 41, 255);

			case LC_EChannelColour.GREEN:
				return Color.FromSRGBA(106, 190, 92, 255);

			case LC_EChannelColour.CYAN:
				return Color.FromSRGBA(88, 198, 214, 255);

			case LC_EChannelColour.BLUE:
				return Color.FromSRGBA(92, 137, 214, 255);

			case LC_EChannelColour.PURPLE:
				return Color.FromSRGBA(163, 110, 214, 255);
		}

		return Color.FromSRGBA(255, 255, 255, 255);
	}

	//------------------------------------------------------------------------------------------------
	//! The separators have to survive the round trip, so they cannot appear in a name
	protected static string Sanitise(string name)
	{
		name.Replace(ENTRY_SEPARATOR, " ");
		name.Replace(FIELD_SEPARATOR, " ");
		return name;
	}
}
