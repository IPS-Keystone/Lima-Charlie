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
	//! Entries in LC_EChannelColour
	protected static const int COLOUR_COUNT = 8;

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
	//! Server side: the same packed form, from the text a server operator writes in server.json.
	//! "45.5,RED,COMMAND;38,GREEN,MEDEVAC" - megahertz, colour name, channel name. Entries that make no
	//! sense are skipped with a warning rather than taking the rest of the list down with them.
	static string PackFromText(string text)
	{
		string packed;
		array<string> entries = {};
		text.Split(ENTRY_SEPARATOR, entries, true);
		foreach (string entry : entries)
		{
			array<string> fields = {};
			entry.Split(FIELD_SEPARATOR, fields, false);
			if (fields.Count() != 3)
			{
				Print("[LC] server.json channel '" + entry + "' is not 'megahertz,colour,name'; skipped", LogLevel.WARNING);
				continue;
			}

			string megahertz = fields[0];
			megahertz.TrimInPlace();
			int frequency = Math.Round(megahertz.ToFloat() * 1000);

			string colourName = fields[1];
			colourName.TrimInPlace();
			int colour = ParseColour(colourName);

			string name = Sanitise(fields[2]);
			name.TrimInPlace();
			if (frequency <= 0 || name.IsEmpty())
			{
				Print("[LC] server.json channel '" + entry + "' has no frequency or no name; skipped", LogLevel.WARNING);
				continue;
			}

			packed += frequency.ToString() + FIELD_SEPARATOR + colour.ToString() + FIELD_SEPARATOR + name + ENTRY_SEPARATOR;
		}

		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! The packed form back as the text server.json holds, so a generated file is editable
	static string UnpackToText(string packed)
	{
		string text;
		array<string> entries = {};
		packed.Split(ENTRY_SEPARATOR, entries, true);
		foreach (string entry : entries)
		{
			array<string> fields = {};
			entry.Split(FIELD_SEPARATOR, fields, false);
			if (fields.Count() != 3)
				continue;

			float megahertz = fields[0].ToInt() / 1000.0;
			if (!text.IsEmpty())
				text += ENTRY_SEPARATOR;

			text += megahertz.ToString(-1, 3) + FIELD_SEPARATOR + ColourName(fields[1].ToInt()) + FIELD_SEPARATOR + fields[2];
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! A colour by name, or by its number; anything unrecognised is white
	static int ParseColour(string name)
	{
		for (int colour = 0; colour < COLOUR_COUNT; colour++)
		{
			if (name.Compare(ColourName(colour), false) == 0)
				return colour;
		}

		int parsed;
		int number = name.ToInt(0, 0, parsed);
		if (parsed > 0 && number >= 0 && number < COLOUR_COUNT)
			return number;

		Print("[LC] server.json colour '" + name + "' is not a Lima Charlie colour; using white", LogLevel.WARNING);
		return LC_EChannelColour.WHITE;
	}

	//------------------------------------------------------------------------------------------------
	static string ColourName(int colour)
	{
		switch (colour)
		{
			case LC_EChannelColour.RED: return "RED";
			case LC_EChannelColour.ORANGE: return "ORANGE";
			case LC_EChannelColour.YELLOW: return "YELLOW";
			case LC_EChannelColour.GREEN: return "GREEN";
			case LC_EChannelColour.CYAN: return "CYAN";
			case LC_EChannelColour.BLUE: return "BLUE";
			case LC_EChannelColour.PURPLE: return "PURPLE";
		}

		return "WHITE";
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
