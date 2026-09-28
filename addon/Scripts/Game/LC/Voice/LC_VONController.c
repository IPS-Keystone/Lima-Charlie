modded class SCR_VONController
{
	//------------------------------------------------------------------------------------------------
	//! Vanilla activation without starting voice capture: TeamSpeak carries the voice, while inputs,
	//! entries, the radial menu and transmit state keep working.
	override protected bool ActivateVON(notnull SCR_VONEntry entry, EVONTransmitType transmitType = EVONTransmitType.NONE)
	{
		if (!m_VONComp)
			return false;

		if (transmitType == EVONTransmitType.NONE)
		{
			if (entry.GetVONMethod() == ECommMethod.SQUAD_RADIO)
				transmitType = EVONTransmitType.CHANNEL;
			else
				transmitType = EVONTransmitType.DIRECT;
		}

		if (transmitType != EVONTransmitType.DIRECT && m_eLifeState == ECharacterLifeState.INCAPACITATED)
			return false;

		m_eVONType = transmitType;
		if (transmitType != EVONTransmitType.DIRECT && !GetGame().GetVONCanTransmitCrossFaction() && !SCR_Global.IsAdmin())
		{
			InitEncryptionKey();

			if (m_sLocalEncryptionKey != string.Empty)
			{
				SCR_VONEntryRadio radioEntry = SCR_VONEntryRadio.Cast(entry);
				if (radioEntry && radioEntry.GetTransceiver())
				{
					BaseRadioComponent radio = radioEntry.GetTransceiver().GetRadio();
					if (radio && radio.GetEncryptionKey() != m_sLocalEncryptionKey)
					{
						SetVONProximity(true);
						if (m_VONDisplay)
							m_VONDisplay.ShowSelectedVONDisabledHint(true);

						return false;
					}
				}
			}
		}

		SetActiveTransmit(entry);
		m_VONComp.SetCapture(false);
		m_bIsActive = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The player controller is going away (leaving the server, ending play mode): tell the plugin now
	//! rather than letting it wait for game_state.json to go stale.
	override void OnDelete(IEntity owner)
	{
		LC_Client client = LC_Client.Get();
		if (client && client.IsFor(owner))
			LC_Client.Stop();

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	override protected void ActionVONTransceiverCycle(float value, EActionTrigger reason = EActionTrigger.UP)
	{
		super.ActionVONTransceiverCycle(value, reason);
		LC_Client.PlayUiSound("cycle");
	}

	//------------------------------------------------------------------------------------------------
	//! What vanilla VON is transmitting on right now
	EVONTransmitType LC_GetTransmitType()
	{
		if (!m_bIsActive)
			return EVONTransmitType.NONE;

		return m_eVONType;
	}

	//------------------------------------------------------------------------------------------------
	//! Radio entry vanilla VON is transmitting on, or null
	SCR_VONEntryRadio LC_GetTransmitEntry()
	{
		if (!m_bIsActive || m_eVONType == EVONTransmitType.NONE || m_eVONType == EVONTransmitType.DIRECT)
			return null;

		return SCR_VONEntryRadio.Cast(GetEntryByTransmitType(m_eVONType));
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's radio entries in vanilla order; the Lima Charlie radio push-to-talk slots index into this
	void LC_GetRadioEntries(notnull array<SCR_VONEntryRadio> entries)
	{
		entries.Clear();
		foreach (SCR_VONEntry entry : m_aEntries)
		{
			SCR_VONEntryRadio radioEntry = SCR_VONEntryRadio.Cast(entry);
			if (radioEntry && radioEntry.GetTransceiver())
				entries.Insert(radioEntry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether Lima Charlie push-to-talk may transmit on this entry, by the same rules as vanilla activation
	bool LC_CanTransmitOn(notnull SCR_VONEntryRadio entry)
	{
		if (!m_VONComp || m_bIsDisabled || m_bIsUnconscious || m_eLifeState != ECharacterLifeState.ALIVE || !entry.IsUsable())
			return false;

		BaseTransceiver transceiver = entry.GetTransceiver();
		if (!transceiver)
			return false;

		BaseRadioComponent radio = transceiver.GetRadio();
		if (!radio || !radio.IsPowered())
			return false;

		if (GetGame().GetVONCanTransmitCrossFaction() || SCR_Global.IsAdmin())
			return true;

		InitEncryptionKey();
		return m_sLocalEncryptionKey == string.Empty || radio.GetEncryptionKey() == m_sLocalEncryptionKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Radio channel under the mouse (or controller selection) in the open VON radial menu, or null
	SCR_VONEntryRadio LC_GetHoveredRadioEntry()
	{
		if (!m_VONMenu)
			return null;

		SCR_RadialMenu radialMenu = m_VONMenu.GetRadialMenu();
		if (!radialMenu || !radialMenu.IsOpened())
			return null;

		return SCR_VONEntryRadio.Cast(radialMenu.GetSelectionEntry());
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws the radial menu entries after an Lima Charlie setting changed
	void LC_RefreshMenu()
	{
		if (!m_VONMenu)
			return;

		SCR_RadialMenu radialMenu = m_VONMenu.GetRadialMenu();
		if (radialMenu && radialMenu.IsOpened())
			radialMenu.UpdateEntries();
	}
}
