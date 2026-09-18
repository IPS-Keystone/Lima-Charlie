//------------------------------------------------------------------------------------------------
//! Puts the server's channel names on the vanilla VON display.
//!
//! Vanilla only labels two frequencies, both outgoing only: the faction's platoon net and Conflict task
//! nets. Anything else shows a bare frequency. This fills in the rest from the server's list, on incoming
//! transmissions as well, and the server's naming mode decides what happens to a frequency we have no
//! name for: keep the game's label under HYBRID, hide it under LC_ONLY, or stay out of the way entirely
//! under VANILLA_ONLY.
//!
//! UpdateTransmission runs when a transmission starts or changes frequency, not every frame, so this is
//! only as expensive as the lookup.
modded class SCR_VonDisplay
{
	//------------------------------------------------------------------------------------------------
	override protected bool UpdateTransmission(TransmissionData data, BaseTransceiver radioTransceiver, int frequency, bool IsReceiving)
	{
		bool shown = super.UpdateTransmission(data, radioTransceiver, frequency, IsReceiving);
		if (!shown || !radioTransceiver || !data.m_Widgets)
			return shown;

		RichTextWidget channelText = data.m_Widgets.m_wChannelText;
		Widget channelFrame = data.m_Widgets.m_wChannelFrame;
		if (!channelText || !channelFrame)
			return shown;

		LC_EChannelNaming naming = LC_ChannelLabels.GetNaming();
		if (naming == LC_EChannelNaming.VANILLA_ONLY)
			return shown;

		LC_ChannelLabel label = LC_ChannelLabels.Find(frequency);
		if (label)
		{
			channelText.SetText(label.m_sName);
			channelText.SetColor(LC_ChannelLabels.GetColour(label.m_eColour));
			channelFrame.SetVisible(true);
			return shown;
		}

		// Nothing of ours for this frequency: hide the game's label, or keep it under HYBRID
		if (naming == LC_EChannelNaming.LC_ONLY)
			channelFrame.SetVisible(false);

		return shown;
	}
}
