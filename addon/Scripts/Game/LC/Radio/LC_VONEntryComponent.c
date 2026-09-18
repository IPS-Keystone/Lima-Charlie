//------------------------------------------------------------------------------------------------
//! Extra line on radio entries in the VON radial menu (LC_VONEntry.layout): transmit key | beep set
modded class SCR_VONEntryComponent
{
	protected TextWidget m_wLCRadioDetailsText;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		m_wLCRadioDetailsText = TextWidget.Cast(m_wRoot.FindAnyWidget("RadioDetailsText"));
	}

	//------------------------------------------------------------------------------------------------
	void LC_SetRadioDetails(string transmitKey, string beepSet)
	{
		if (m_wLCRadioDetailsText)
			m_wLCRadioDetailsText.SetText(transmitKey + " | " + beepSet);
	}
}
