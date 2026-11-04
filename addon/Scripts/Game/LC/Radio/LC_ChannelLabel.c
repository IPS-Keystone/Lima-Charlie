//------------------------------------------------------------------------------------------------
//! Colours a channel label can be given: a named palette that reads well on the VON display, or CUSTOM for
//! any colour (the colour picker in the Workbench, "#RRGGBB" in server.json). New values go on the end:
//! the numbers travel to clients and are accepted in server.json.
enum LC_EChannelColour
{
	WHITE,
	RED,
	ORANGE,
	YELLOW,
	GREEN,
	CYAN,
	BLUE,
	PURPLE,
	CUSTOM
}

//------------------------------------------------------------------------------------------------
//! Where channel names on the radio menu and the VON display come from
enum LC_EChannelNaming
{
	//! The server's named frequencies only. A frequency with no entry shows no name at all.
	LC_ONLY,
	//! The server's name where there is one, otherwise the game's own
	HYBRID,
	//! The game's own names only: platoon net, playable group callsigns and task nets
	VANILLA_ONLY
}

//------------------------------------------------------------------------------------------------
//! One named frequency. The server owns the list; every client is sent the same one, so a name means the
//! same thing to everybody on the server.
[BaseContainerProps()]
class LC_ChannelLabel
{
	[Attribute(defvalue: "0", desc: "Frequency in MHz, exactly as it reads on the radio. 45.5 matches a radio tuned to 45.500 MHz.")]
	float m_fFrequencyMHz;

	[Attribute(defvalue: "", desc: "Name shown beside the frequency, for example COMMAND or MEDEVAC. Commas and semicolons are stripped.")]
	string m_sName;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LC_EChannelColour), desc: "Background colour of that name on the VON display; the text is white or black, whichever reads better. CUSTOM uses the colour below.")]
	LC_EChannelColour m_eColour;

	[Attribute(defvalue: "1 1 1 1", uiwidget: UIWidgets.ColorPicker, desc: "Used only when the colour above is CUSTOM. Transparency is ignored: every label is drawn at the game's own label opacity.")]
	ref Color m_CustomColour;

	//! Client side: the colour as sRGB 0-255, resolved when the list arrives
	int m_iRed = 255;
	int m_iGreen = 255;
	int m_iBlue = 255;

	//------------------------------------------------------------------------------------------------
	//! Radios work in kHz, which is what the transmission carries
	int GetFrequencyKHz()
	{
		return Math.Round(m_fFrequencyMHz * 1000);
	}
}
