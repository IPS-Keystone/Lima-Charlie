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
	//! Vanilla fades an entry out a second after its last refresh, so this keeps one lit with room to spare
	protected static const int RECEIVE_KEEPALIVE_MS = 250;

	protected SCR_VonDisplay m_Display;
	protected ref array<SCR_VONEntryRadio> m_aEntries = {};
	protected int m_iLastPluginSeq = -1;
	protected int m_iNextReceiveTick;

	//------------------------------------------------------------------------------------------------
	void Update(notnull LC_Client client, int now)
	{
		SCR_VonDisplay display = GetDisplay();
		if (!display)
			return;

		// One call that returns at once unless something changed, so the transmit indicator follows the key
		ShowTransmitting(display, client);

		// What we hear only changes when the plugin writes, at most 20 times a second
		int pluginSeq = client.GetPluginState().GetSeq();
		if (pluginSeq == m_iLastPluginSeq && now < m_iNextReceiveTick)
			return;

		m_iLastPluginSeq = pluginSeq;
		m_iNextReceiveTick = now + RECEIVE_KEEPALIVE_MS;
		ShowReceiving(display, client);
	}

	//------------------------------------------------------------------------------------------------
	void Release()
	{
		m_Display = null;
	}

	//------------------------------------------------------------------------------------------------
	//! What we are transmitting on, radio or direct. Neither reaches the display on its own any more:
	//! vanilla only raises direct speech while its own key is held, and that key is no longer what opens
	//! the microphone.
	protected void ShowTransmitting(notnull SCR_VonDisplay display, notnull LC_Client client)
	{
		EVONTransmitType transmitType = client.GetTransmitType();
		if (transmitType != EVONTransmitType.CHANNEL && transmitType != EVONTransmitType.LONG_RANGE)
		{
			// Direct speech follows TeamSpeak now, not a game key, so vanilla never raises it for us. A null
			// transceiver is how the display is told a transmission is direct.
			if (client.GetPluginState().IsSelfTalking() && LC_Life.CanSpeak(client.GetControlledEntity()))
				display.OnCapture(null);

			return;
		}

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
