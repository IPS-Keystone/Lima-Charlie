//------------------------------------------------------------------------------------------------
//! Vanilla's radio on/off toggle, with the one change Lima Charlie needs: vanilla casts every VON entry to a
//! radio entry without checking, so the inspection action's on/off crashed with a NULL pointer while any
//! other kind of entry (LC's intercom) was listed. The rest is vanilla's own body, unchanged.
modded class SCR_RadioComponent
{
	//------------------------------------------------------------------------------------------------
	override void RadioToggle()
	{
		m_BaseRadioComp.SetPower(!m_BaseRadioComp.IsPowered());
		m_bIsPowered = m_BaseRadioComp.IsPowered();

		PlayerController pc = GetGame().GetPlayerController();
		if (pc)
		{
			SCR_VONController vonController = SCR_VONController.Cast(pc.FindComponent(SCR_VONController));
			if (vonController)
			{
				array<ref SCR_VONEntry> entries = {};
				vonController.GetVONEntries(entries);

				foreach (SCR_VONEntry entry : entries)
				{
					SCR_VONEntryRadio radioEntry = SCR_VONEntryRadio.Cast(entry);
					if (!radioEntry || !radioEntry.GetTransceiver())
						continue;

					BaseRadioComponent radio = radioEntry.GetTransceiver().GetRadio();
					if (radio == m_BaseRadioComp)
						entry.SetUsable(radio.IsPowered());
				}
			}
		}

		if (m_bIsPowered)
		{
			if (m_iRadioType == ERadioType.ANPRC68 || ERadioType.ANPRC77)
				SetKnobAnim(m_iSignalPower, 1, 0.3, false);
		}
		else
		{
			if (m_iRadioType == ERadioType.ANPRC68 || ERadioType.ANPRC77)
				SetKnobAnim(m_iSignalPower, 0, 0.3, false);
		}
	}
}
