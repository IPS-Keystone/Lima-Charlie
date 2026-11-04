//------------------------------------------------------------------------------------------------
//! The named frequencies this server uses, packed on the server and unpacked on each client.
//!
//! Held statically because the VON display reads it and has no route back to LC_Client. It is client
//! state only: the server builds the string once per session and sends it with the other settings.
class LC_ChannelLabels
{
	//! "frequencyKHz,colour,name;frequencyKHz,colour,name;" - colour is a palette number or "#RRGGBB"
	protected static const string ENTRY_SEPARATOR = ";";
	protected static const string FIELD_SEPARATOR = ",";
	//! Named entries in LC_EChannelColour, which CUSTOM follows
	protected static const int COLOUR_COUNT = 8;
	protected static const string HEX_PREFIX = "#";
	protected static const string HEX_DIGITS = "0123456789ABCDEF";
	//! Perceived brightness (0-255) under which a label gets white text. Splits the palette as RED, BLUE and
	//! PURPLE (dark) against the rest.
	protected static const float DARK_BRIGHTNESS = 150;

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

			packed += frequency.ToString() + FIELD_SEPARATOR + PackColour(label) + FIELD_SEPARATOR + name + ENTRY_SEPARATOR;
		}

		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! A palette number, or "#RRGGBB" for a custom colour. The colour picker hands over the colour widgets
	//! take (vanilla passes picker colours straight to SetColor), which is linear; hex is sRGB, as people write it.
	protected static string PackColour(notnull LC_ChannelLabel label)
	{
		int colour = label.m_eColour;
		if (label.m_eColour != LC_EChannelColour.CUSTOM)
			return colour.ToString();

		if (!label.m_CustomColour)
			return "0";

		Color srgb = label.m_CustomColour.Copy();
		srgb.LinearToSRGB();
		return ToHex(ToByte(srgb.R()), ToByte(srgb.G()), ToByte(srgb.B()));
	}

	//------------------------------------------------------------------------------------------------
	protected static int ToByte(float channel)
	{
		int value = Math.Round(channel * 255);
		return Math.ClampInt(value, 0, 255);
	}

	//------------------------------------------------------------------------------------------------
	//! "#RRGGBB", upper case
	static string ToHex(int red, int green, int blue)
	{
		return HEX_PREFIX + HexByte(red) + HexByte(green) + HexByte(blue);
	}

	//------------------------------------------------------------------------------------------------
	protected static string HexByte(int value)
	{
		int high = value / 16;
		int low = value - high * 16;
		return HEX_DIGITS.Substring(high, 1) + HEX_DIGITS.Substring(low, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! "#RRGGBB" (either case) to 0-255 channels. False for anything else, leaving the outputs alone.
	static bool ParseHex(string text, out int red, out int green, out int blue)
	{
		if (text.Length() != 7 || !text.StartsWith(HEX_PREFIX))
			return false;

		int parsedRed = HexPair(text, 1);
		int parsedGreen = HexPair(text, 3);
		int parsedBlue = HexPair(text, 5);
		if (parsedRed < 0 || parsedGreen < 0 || parsedBlue < 0)
			return false;

		red = parsedRed;
		green = parsedGreen;
		blue = parsedBlue;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The two hex digits at index and index + 1 as 0-255, or -1
	protected static int HexPair(string text, int index)
	{
		int high = HexDigit(text.ToAscii(index));
		int low = HexDigit(text.ToAscii(index + 1));
		if (high < 0 || low < 0)
			return -1;

		return high * 16 + low;
	}

	//------------------------------------------------------------------------------------------------
	//! One hex digit's value from its character code, or -1
	protected static int HexDigit(int ascii)
	{
		if (ascii >= 48 && ascii <= 57)
			return ascii - 48;

		if (ascii >= 65 && ascii <= 70)
			return ascii - 55;

		if (ascii >= 97 && ascii <= 102)
			return ascii - 87;

		return -1;
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
			string colour = ParseColourText(colourName);

			string name = Sanitise(fields[2]);
			name.TrimInPlace();
			if (frequency <= 0 || name.IsEmpty())
			{
				Print("[LC] server.json channel '" + entry + "' has no frequency or no name; skipped", LogLevel.WARNING);
				continue;
			}

			packed += frequency.ToString() + FIELD_SEPARATOR + colour + FIELD_SEPARATOR + name + ENTRY_SEPARATOR;
		}

		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! A colour as server.json writes it, in packed form: "#RRGGBB" is kept (upper cased), names and numbers
	//! become the palette number
	protected static string ParseColourText(string text)
	{
		if (!text.StartsWith(HEX_PREFIX))
		{
			int colour = ParseColour(text);
			return colour.ToString();
		}

		int red;
		int green;
		int blue;
		if (ParseHex(text, red, green, blue))
			return ToHex(red, green, blue);

		Print("[LC] server.json colour '" + text + "' is not #RRGGBB; using white", LogLevel.WARNING);
		return "0";
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

			string colour = fields[1];
			if (!colour.StartsWith(HEX_PREFIX))
				colour = ColourName(fields[1].ToInt());

			text += megahertz.ToString(-1, 3) + FIELD_SEPARATOR + colour + FIELD_SEPARATOR + fields[2];
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

		Print("[LC] server.json colour '" + name + "' is not a Lima Charlie colour or #RRGGBB; using white", LogLevel.WARNING);
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
			label.m_sName = s_aFields[2];
			ResolveColour(label, s_aFields[1]);
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
	//! Client side: the packed colour field onto the label as sRGB channels
	protected static void ResolveColour(notnull LC_ChannelLabel label, string packed)
	{
		int red = 255;
		int green = 255;
		int blue = 255;
		if (ParseHex(packed, red, green, blue))
		{
			label.m_eColour = LC_EChannelColour.CUSTOM;
		}
		else
		{
			label.m_eColour = packed.ToInt();
			GetPaletteRgb(label.m_eColour, red, green, blue);
		}

		label.m_iRed = red;
		label.m_iGreen = green;
		label.m_iBlue = blue;
	}

	//------------------------------------------------------------------------------------------------
	//! The palette in sRGB 0-255; anything unknown is white
	static void GetPaletteRgb(LC_EChannelColour colour, out int red, out int green, out int blue)
	{
		red = 255;
		green = 255;
		blue = 255;
		switch (colour)
		{
			case LC_EChannelColour.RED:
				red = 232; green = 74; blue = 74;
				break;

			case LC_EChannelColour.ORANGE:
				red = 226; green = 143; blue = 41;
				break;

			case LC_EChannelColour.YELLOW:
				red = 226; green = 201; blue = 41;
				break;

			case LC_EChannelColour.GREEN:
				red = 106; green = 190; blue = 92;
				break;

			case LC_EChannelColour.CYAN:
				red = 88; green = 198; blue = 214;
				break;

			case LC_EChannelColour.BLUE:
				red = 92; green = 137; blue = 214;
				break;

			case LC_EChannelColour.PURPLE:
				red = 163; green = 110; blue = 214;
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a label needs white text rather than black to stay readable on its colour, by perceived
	//! brightness (Rec. 601 weights)
	static bool IsDark(notnull LC_ChannelLabel label)
	{
		float brightness = 0.299 * label.m_iRed + 0.587 * label.m_iGreen + 0.114 * label.m_iBlue;
		return brightness < DARK_BRIGHTNESS;
	}

	//------------------------------------------------------------------------------------------------
	static Color GetColour(notnull LC_ChannelLabel label)
	{
		return Color.FromSRGBA(label.m_iRed, label.m_iGreen, label.m_iBlue, 255);
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
