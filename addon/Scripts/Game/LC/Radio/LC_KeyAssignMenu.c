modded enum ChimeraMenuPreset
{
	LC_KeyAssignMenu
}

//------------------------------------------------------------------------------------------------
//! The transmit key picker: five buttons beside the radio menu, None and 1 to 4, for the channel that was
//! under the cursor when the key was pressed. A real menu rather than widgets over the radial one, so it
//! gets a cursor of its own and holds game input until it is closed - the same as the Coalition squad
//! interface's colour team picker, which this is modelled on.
class LC_KeyAssignMenu : ChimeraMenuBase
{
	protected static const string ACTION_BACK = "MenuBack";
	//! Index 0 is None, so a button's index is the transmit key it stands for plus one
	protected static ref array<string> s_aButtons = {"KeyNone", "Key1", "Key2", "Key3", "Key4"};

	//! Handed over by Open, since a menu is created by the menu manager rather than by us
	protected static SCR_VONEntryRadio s_PendingEntry;
	protected static int s_iPendingKey;
	protected static string s_sPendingTitle;

	protected SCR_VONEntryRadio m_Entry;

	//------------------------------------------------------------------------------------------------
	//! \param currentKey the channel's transmit key now, 0 to 3, or -1 for none
	static void Open(notnull SCR_VONEntryRadio entry, int currentKey, string title)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		s_PendingEntry = entry;
		s_iPendingKey = currentKey;
		s_sPendingTitle = title;
		menuManager.OpenMenu(ChimeraMenuPreset.LC_KeyAssignMenu);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_Entry = s_PendingEntry;
		s_PendingEntry = null;

		Widget root = GetRootWidget();
		TextWidget title = TextWidget.Cast(root.FindAnyWidget("Title"));
		if (title)
			title.SetText(s_sPendingTitle);

		foreach (int index, string name : s_aButtons)
		{
			ButtonWidget button = ButtonWidget.Cast(root.FindAnyWidget(name));
			if (!button)
				continue;

			// The key the channel is on now is lit, so the picker also says what it is set to
			int key = index - 1;
			if (key == s_iPendingKey)
				button.SetColor(Color.FromSRGBA(40, 159, 70, 255));
			else
				button.SetColor(Color.FromSRGBA(46, 46, 46, 255));

			button.AddHandler(new LC_KeyAssignButton(this, key));
		}

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.AddActionListener(ACTION_BACK, EActionTrigger.DOWN, OnBack);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.RemoveActionListener(ACTION_BACK, EActionTrigger.DOWN, OnBack);

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! \param key 0 to 3, or -1 to take the channel off every transmit key
	void Choose(int key)
	{
		LC_Client client = LC_Client.Get();
		if (client && m_Entry)
			client.OnTransmitKeyChosen(m_Entry, key);

		Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBack(float value = 0.0, EActionTrigger reason = 0)
	{
		Close();
	}
}

//------------------------------------------------------------------------------------------------
//! One of the picker's buttons. Holds the menu weakly: the menu owns the widgets this is attached to.
class LC_KeyAssignButton : ScriptedWidgetEventHandler
{
	protected LC_KeyAssignMenu m_Menu;
	protected int m_iKey;

	//------------------------------------------------------------------------------------------------
	void LC_KeyAssignButton(LC_KeyAssignMenu menu, int key)
	{
		m_Menu = menu;
		m_iKey = key;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Menu)
			m_Menu.Choose(m_iKey);

		return true;
	}
}
