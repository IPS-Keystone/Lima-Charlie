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
	//! Keys 1 to 4 pick transmit keys 1 to 4 (LC_KeyAssign1 to 4), in a context of their own that is only
	//! active while the picker is open, so they never clash with weapon selection
	protected static const string INPUT_CONTEXT = "LC_KeyAssignContext";
	protected static const string ACTION_KEY = "LC_KeyAssign";
	//! The radial menu is reopened a moment after the picker closes, so the click or key that closed the
	//! picker is not also taken by the radial menu's own listeners
	protected static const int REOPEN_DELAY_MS = 100;
	//! Index 0 is None, so a button's index is the transmit key it stands for plus one
	protected static ref array<string> s_aButtons = {"KeyNone", "Key1", "Key2", "Key3", "Key4"};

	//! Handed over by Open, since a menu is created by the menu manager rather than by us
	protected static SCR_VONEntryRadio s_PendingEntry;
	protected static int s_iPendingKey;
	protected static string s_sPendingTitle;

	protected SCR_VONEntryRadio m_Entry;
	//! Set when the player closed the picker (a key chosen, or backed out), so the radio menu comes back.
	//! Not set when something else closed it (another menu, death), where the radio menu should stay shut.
	protected bool m_bReopenRadioMenu;

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
		if (!inputManager)
			return;

		inputManager.AddActionListener(ACTION_BACK, EActionTrigger.DOWN, OnBack);
		inputManager.AddActionListener(ACTION_KEY + "1", EActionTrigger.DOWN, OnKey1);
		inputManager.AddActionListener(ACTION_KEY + "2", EActionTrigger.DOWN, OnKey2);
		inputManager.AddActionListener(ACTION_KEY + "3", EActionTrigger.DOWN, OnKey3);
		inputManager.AddActionListener(ACTION_KEY + "4", EActionTrigger.DOWN, OnKey4);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		// A context only stays active for the frame it was activated in
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			inputManager.ActivateContext(INPUT_CONTEXT);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener(ACTION_BACK, EActionTrigger.DOWN, OnBack);
			inputManager.RemoveActionListener(ACTION_KEY + "1", EActionTrigger.DOWN, OnKey1);
			inputManager.RemoveActionListener(ACTION_KEY + "2", EActionTrigger.DOWN, OnKey2);
			inputManager.RemoveActionListener(ACTION_KEY + "3", EActionTrigger.DOWN, OnKey3);
			inputManager.RemoveActionListener(ACTION_KEY + "4", EActionTrigger.DOWN, OnKey4);
		}

		if (m_bReopenRadioMenu)
			GetGame().GetCallqueue().CallLater(ReopenRadioMenu, REOPEN_DELAY_MS);

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReopenRadioMenu()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		SCR_VONController vonController = SCR_VONController.Cast(playerController.FindComponent(SCR_VONController));
		if (vonController)
			vonController.LC_ReopenMenu();
	}

	//------------------------------------------------------------------------------------------------
	//! \param key 0 to 3, or -1 to take the channel off every transmit key
	void Choose(int key)
	{
		LC_Client client = LC_Client.Get();
		if (client && m_Entry)
			client.OnTransmitKeyChosen(m_Entry, key);

		m_bReopenRadioMenu = true;
		Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Escape is the None button: it takes the channel off every transmit key
	protected void OnBack(float value = 0.0, EActionTrigger reason = 0)
	{
		Choose(-1);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnKey1(float value = 0.0, EActionTrigger reason = 0)
	{
		Choose(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnKey2(float value = 0.0, EActionTrigger reason = 0)
	{
		Choose(1);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnKey3(float value = 0.0, EActionTrigger reason = 0)
	{
		Choose(2);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnKey4(float value = 0.0, EActionTrigger reason = 0)
	{
		Choose(3);
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
