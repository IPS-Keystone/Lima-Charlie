//------------------------------------------------------------------------------------------------
//! Colours a channel label can be given. Named rather than free hex so the list stays readable when it is
//! edited as text rather than in the Workbench.
enum LC_EChannelColour
{
	WHITE,
	RED,
	ORANGE,
	YELLOW,
	GREEN,
	CYAN,
	BLUE,
	PURPLE
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

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LC_EChannelColour), desc: "Colour of that name on the VON display.")]
	LC_EChannelColour m_eColour;

	//------------------------------------------------------------------------------------------------
	//! Radios work in kHz, which is what the transmission carries
	int GetFrequencyKHz()
	{
		return Math.Round(m_fFrequencyMHz * 1000);
	}
}
