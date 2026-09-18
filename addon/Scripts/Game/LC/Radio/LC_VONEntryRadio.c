modded class SCR_VONEntryRadio
{
	protected static const ResourceName LC_LAYOUT = "{6A5C69DD20000050}UI/layouts/HUD/LC_VONEntry.layout";

	//------------------------------------------------------------------------------------------------
	//! Vanilla's radial menu entry layout plus transmit key and beep set lines
	override void InitEntry()
	{
		super.InitEntry();
		SetCustomLayout(LC_LAYOUT);
	}

	//------------------------------------------------------------------------------------------------
	//! Like Enhanced Radio: "45.0 MHz  L | 70%", then the transmit key and beep set on their own lines
	override void Update()
	{
		super.Update();

		SCR_VONEntryComponent entryComp = SCR_VONEntryComponent.Cast(m_EntryComponent);
		LC_Client client = LC_Client.Get();
		if (!entryComp || !m_RadioTransceiver || !client)
			return;

		LC_RadioSettings settings = client.GetRadioSettings();
		string frequency = m_sText;
		if (!m_sFrequencyTextOverwrite.IsEmpty())
			frequency = m_sFrequencyTextOverwrite;

		int volume = Math.Round(settings.GetVolume(this) * 100);
		entryComp.SetFrequencyText(frequency + "  " + LC_RadioSettings.GetEarShortName(settings.GetEar(this)) + " | " + volume.ToString() + "%");

		string transmitKey = "TX -";
		int key = settings.GetKey(this, client.GetRadioEntries());
		if (key >= 0)
		{
			int keyNumber = key + 1;
			transmitKey = "TX " + keyNumber.ToString();
		}

		entryComp.LC_SetRadioDetails(transmitKey, settings.GetBeepSetShortName(this));
		LC_ApplyChannelText(m_RadioTransceiver.GetFrequency());
	}

	//------------------------------------------------------------------------------------------------
	//! The name shown above the frequency on the radial entry, from whichever source the server picked.
	//! Colours are display only, so the name goes on plain here.
	protected void LC_ApplyChannelText(int frequency)
	{
		if (!m_sChannelTextOverwrite.IsEmpty())
			return;

		LC_EChannelNaming naming = LC_ChannelLabels.GetNaming();
		if (naming == LC_EChannelNaming.VANILLA_ONLY)
		{
			SetChannelText(SCR_VONMenu.GetKnownChannel(frequency));
			return;
		}

		LC_ChannelLabel label = LC_ChannelLabels.Find(frequency);
		if (label)
		{
			SetChannelText(label.m_sName);
			return;
		}

		if (naming == LC_EChannelNaming.HYBRID)
			SetChannelText(SCR_VONMenu.GetKnownChannel(frequency));
		else
			SetChannelText(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Tunes the transceiver and updates this entry's shown frequency straight away, since the transceiver's
	//! own getter lags behind a change (and AdjustEntryModif would write the stale value back).
	void LC_SetFrequency(int frequency)
	{
		if (!m_RadioTransceiver)
			return;

		m_RadioTransceiver.SetFrequency(frequency);
		m_iFrequency = frequency;

		float megahertz = Math.Round(frequency * 0.1) * 0.01;
		m_sText = megahertz.ToString(3, 1) + " " + LABEL_FREQUENCY_UNITS;

		LC_ApplyChannelText(frequency);
	}
}
