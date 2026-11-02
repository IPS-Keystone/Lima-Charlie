//! The container attribute must be repeated on the modded class, or prefabs can no longer load m_VONMenu
[BaseContainerProps()]
modded class SCR_VONMenu
{
	//------------------------------------------------------------------------------------------------
	//! Ctrl + scroll changes the hovered channel's Lima Charlie volume; don't let the same scroll also tune its frequency
	override protected void ActionTuneFrequency(float value, EActionTrigger reason)
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager && inputManager.GetActionValue("LC_RadioVolume") != 0)
			return;

		super.ActionTuneFrequency(value, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Scrolling the intercom entry cycles its channel. Vanilla would also pop up the squad and platoon
	//! frequency table, which means nothing for an intercom, so the intercom skips vanilla altogether.
	override protected void OnAdjustEntry(int input, bool isModif)
	{
		if (!m_RadialMenu)
			return;

		LC_VONEntryIntercom intercom = LC_VONEntryIntercom.Cast(m_RadialMenu.GetSelectionEntry());
		if (!intercom)
		{
			super.OnAdjustEntry(input, isModif);
			return;
		}

		if (isModif)
			intercom.AdjustEntryModif(input);
		else
			intercom.AdjustEntry(input);

		intercom.Update();
		m_RadialMenu.UpdateEntries();
	}
}
