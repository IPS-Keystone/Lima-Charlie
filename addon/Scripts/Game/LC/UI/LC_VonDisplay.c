//------------------------------------------------------------------------------------------------
//! Puts the server's channel names on the vanilla VON display.
//!
//! Vanilla only labels two frequencies, both outgoing only: the faction's platoon net and Conflict task
//! nets. Anything else shows a bare frequency. This fills in the rest from the server's list, on incoming
//! transmissions as well, and the server's naming mode decides what happens to a frequency we have no
//! name for: keep the game's label under HYBRID, hide it under LC_ONLY, or stay out of the way entirely
//! under VANILLA_ONLY.
//!
//! A named channel is drawn on a background of its colour, with white or black text to suit, the way
//! OTAC Core styled its nets (with OTAC's approval, so OTAC can leave the display to us). The server can
//! also hide incoming radio speakers' roles and the direct speech entries.
//!
//! UpdateTransmission runs when a transmission starts or changes frequency, not every frame, so this is
//! only as expensive as the lookup.
modded class SCR_VonDisplay
{
	//! Server settings, sent with the session
	protected static bool s_bLC_HideRadioRoles = true;
	protected static bool s_bLC_HideDirectSpeech = true;

	//------------------------------------------------------------------------------------------------
	static void LC_SetOptions(bool hideRadioRoles, bool hideDirectSpeech)
	{
		s_bLC_HideRadioRoles = hideRadioRoles;
		s_bLC_HideDirectSpeech = hideDirectSpeech;
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla checks this flag here (a new direct speaker) and in UpdateTransmission (radio turning
	//! direct), and takes it from the server's own setting once at init; ours only ever adds to that
	override event void OnReceive(int playerId, bool isSenderEditor, BaseTransceiver receiver, int frequency, float quality)
	{
		if (s_bLC_HideDirectSpeech)
			m_bIsVONDirectDisabled = true;

		super.OnReceive(playerId, isSenderEditor, receiver, frequency, quality);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool UpdateTransmission(TransmissionData data, BaseTransceiver radioTransceiver, int frequency, bool IsReceiving)
	{
		if (s_bLC_HideDirectSpeech)
			m_bIsVONDirectDisabled = true;

		bool shown = super.UpdateTransmission(data, radioTransceiver, frequency, IsReceiving);
		// The "+N" overflow line has no speaker widgets at all, hence every check
		if (!shown || !radioTransceiver || !data.m_Widgets)
			return shown;

		if (IsReceiving && s_bLC_HideRadioRoles)
		{
			if (data.m_Widgets.m_wRole)
				data.m_Widgets.m_wRole.SetVisible(false);

			if (data.m_Widgets.m_wSquadLeaderIcon)
				data.m_Widgets.m_wSquadLeaderIcon.SetVisible(false);
		}

		RichTextWidget channelText = data.m_Widgets.m_wChannelText;
		Widget channelFrame = data.m_Widgets.m_wChannelFrame;
		ImageWidget channelBackground = data.m_Widgets.m_wChannelBackground;
		if (!channelText || !channelFrame)
			return shown;

		LC_EChannelNaming naming = LC_ChannelLabels.GetNaming();
		if (naming == LC_EChannelNaming.VANILLA_ONLY)
			return shown;

		LC_ChannelLabel label = LC_ChannelLabels.Find(frequency);
		if (label)
		{
			channelText.SetText(label.m_sName);
			if (LC_ChannelLabels.IsDark(label))
				channelText.SetColor(Color.FromSRGBA(255, 255, 255, 255));
			else
				channelText.SetColor(Color.FromSRGBA(0, 0, 0, 255));

			if (channelBackground)
				channelBackground.SetColor(LC_ChannelLabels.GetColour(label));

			channelFrame.SetVisible(true);
			return shown;
		}

		// The game's own label (or none) in a widget we may have coloured for an earlier speaker: back to the
		// layout's own colours (VoNOverlay_Element.layout: white background at 0.3 opacity, black text)
		channelText.SetColor(Color.FromSRGBA(0, 0, 0, 255));
		if (channelBackground)
			channelBackground.SetColor(Color.FromSRGBA(255, 255, 255, 255));

		// Nothing of ours for this frequency: hide the game's label, or keep it under HYBRID
		if (naming == LC_EChannelNaming.LC_ONLY)
			channelFrame.SetVisible(false);

		return shown;
	}

	//------------------------------------------------------------------------------------------------
	//! A speaker vanilla parked on the "+N" overflow line whose entry never came on. That line has none of a
	//! speaker's widgets, so a mod that styles them in UpdateTransmission without checking (OTAC Core) throws
	//! there, and the throw skips vanilla's m_bIsActive. Every later OnReceive for that speaker would then run
	//! UpdateTransmission and throw again, many times a second, until a line frees up.
	bool LC_IsStuckOverflow(int playerId)
	{
		TransmissionData data = m_aTransmissionMap.Get(playerId);
		return data && data.m_bIsAdditional && !data.m_bIsActive;
	}
}
