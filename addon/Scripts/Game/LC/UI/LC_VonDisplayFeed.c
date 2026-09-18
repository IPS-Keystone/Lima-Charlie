//------------------------------------------------------------------------------------------------
//! Drives the vanilla VON display (SCR_VonDisplay, top left) from Lima Charlie.
//!
//! The display already knows how to draw a transmission: frequency, channel name, speaker and icon, with
//! its own fade. It is normally fed by SCR_VONComponent's OnCapture and OnReceive events, and neither
//! fires for us: Lima Charlie transmits without taking vanilla's capture, and incoming voice arrives through
//! TeamSpeak rather than the engine's radio. So we raise the same two events ourselves, every frame while
//! a transmission is live, and let the display handle the rest. Mods that build on it, like Enhanced
//! Radio's frequency colouring, keep working because nothing about the display itself is replaced.
class LC_VonDisplayFeed
{
	protected SCR_VonDisplay m_Display;
	protected ref array<SCR_VONEntryRadio> m_aEntries = {};

	//------------------------------------------------------------------------------------------------
	void Update(notnull LC_Client client)
	{
		SCR_VonDisplay display = GetDisplay();
		if (!display)
			return;

		ShowTransmitting(display, client);
		ShowReceiving(display, client);
	}

	//------------------------------------------------------------------------------------------------
	void Release()
	{
		m_Display = null;
	}

	//------------------------------------------------------------------------------------------------
	//! What we are transmitting on. Direct speech is left to vanilla, which still owns that path.
	protected void ShowTransmitting(notnull SCR_VonDisplay display, notnull LC_Client client)
	{
		EVONTransmitType transmitType = client.GetTransmitType();
		if (transmitType != EVONTransmitType.CHANNEL && transmitType != EVONTransmitType.LONG_RANGE)
			return;

		SCR_VONEntryRadio entry = client.GetTransmitRadioEntry();
		if (!entry)
			return;

		BaseTransceiver transceiver = entry.GetTransceiver();
		if (transceiver)
			display.OnCapture(transceiver);
	}

	//------------------------------------------------------------------------------------------------
	//! Everyone the plugin is playing to us over a radio, on the radio it reaches us on
	protected void ShowReceiving(notnull SCR_VonDisplay display, notnull LC_Client client)
	{
		map<int, ref LC_HeardTransmission> heard = client.GetPluginState().GetHeardTransmissions();
		if (heard.IsEmpty())
			return;

		m_aEntries.Clear();
		m_aEntries.Copy(client.GetRadioEntries());

		foreach (int playerId, LC_HeardTransmission transmission : heard)
		{
			BaseTransceiver transceiver = FindTransceiver(transmission.m_sRadioId);
			if (!transceiver)
				continue;

			display.OnReceive(playerId, false, transceiver, transmission.m_iFrequency, transmission.m_fQuality);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The local transceiver the plugin named, or null if that radio has since gone
	protected BaseTransceiver FindTransceiver(string radioId)
	{
		foreach (SCR_VONEntryRadio entry : m_aEntries)
		{
			if (LC_Radio.GetId(entry) == radioId)
				return entry.GetTransceiver();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Looked up lazily: the HUD is not built yet when the client starts
	protected SCR_VonDisplay GetDisplay()
	{
		if (m_Display)
			return m_Display;

		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager)
			return null;

		m_Display = SCR_VonDisplay.Cast(hudManager.FindInfoDisplay(SCR_VonDisplay));
		return m_Display;
	}
}
