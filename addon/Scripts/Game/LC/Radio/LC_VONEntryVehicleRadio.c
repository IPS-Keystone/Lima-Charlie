//------------------------------------------------------------------------------------------------
//! VON menu entry for one transceiver of a vehicle's radio, listed by LC_VehicleSeat while the player holds
//! the radio's VEHICLE lease. Everything else about it is an ordinary radio entry. The server owns the radio,
//! so frequency and power changes are also sent to it (LC_PlayerController), which checks the lease and
//! makes the change for the whole crew; the local change vanilla makes is only a preview until then.
class LC_VONEntryVehicleRadio : SCR_VONEntryRadio
{
	//------------------------------------------------------------------------------------------------
	override void AdjustEntryModif(int modifier)
	{
		super.AdjustEntryModif(modifier);
		if (modifier != 0)
			LC_SendFrequency();
	}

	//------------------------------------------------------------------------------------------------
	override void AdjustEntry(int modifier)
	{
		int before = m_iFrequency;
		super.AdjustEntry(modifier);
		if (m_iFrequency != before)
			LC_SendFrequency();
	}

	//------------------------------------------------------------------------------------------------
	override void ToggleEntry()
	{
		super.ToggleEntry();

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		RplId radioId = LC_GetRadioId();
		if (playerController && radioId.IsValid())
			playerController.LC_PowerLeasedRadio(radioId, IsUsable());
	}

	//------------------------------------------------------------------------------------------------
	override void LC_SetFrequency(int frequency)
	{
		super.LC_SetFrequency(frequency);
		LC_SendFrequency();
	}

	//------------------------------------------------------------------------------------------------
	//! The radio entity's id, which its lease is held under
	RplId LC_GetRadioId()
	{
		if (!m_RadioTransceiver || !m_RadioTransceiver.GetRadio())
			return RplId.Invalid();

		return LC_RadioLeases.GetRadioId(m_RadioTransceiver.GetRadio().GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void LC_SendFrequency()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		RplId radioId = LC_GetRadioId();
		if (playerController && radioId.IsValid())
			playerController.LC_TuneLeasedRadio(radioId, GetTransceiverNumber() - 1, m_iFrequency);
	}
}
