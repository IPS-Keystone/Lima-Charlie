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
}
