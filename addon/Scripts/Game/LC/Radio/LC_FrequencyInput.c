//------------------------------------------------------------------------------------------------
//! Small dialog for typing a radio frequency in MHz. While open it holds the chat input context, like the
//! chat box does, so typing does not also drive the character.
class LC_FrequencyInput
{
	protected static const ResourceName LAYOUT = "{6A5C69DD20000040}UI/layouts/HUD/LC_FrequencyInput.layout";
	protected static const string CONTEXT = "ChatContext";
	protected static const string ACTION_CONFIRM = "ChatSendMessage";
	protected static const string ACTION_CANCEL = "ChatEscape";

	protected Widget m_wRoot;
	protected EditBoxWidget m_wEdit;
	protected SCR_VONEntryRadio m_Entry;

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		return m_wRoot != null;
	}

	//------------------------------------------------------------------------------------------------
	bool Open(notnull SCR_VONEntryRadio entry, string title)
	{
		Close();

		BaseTransceiver transceiver = entry.GetTransceiver();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		InputManager inputManager = GetGame().GetInputManager();
		if (!transceiver || !workspace || !inputManager)
			return false;

		m_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!m_wRoot)
			return false;

		m_wEdit = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("FrequencyEdit"));
		if (!m_wEdit)
		{
			Close();
			return false;
		}

		TextWidget titleText = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		if (titleText)
			titleText.SetText(title);

		TextWidget rangeText = TextWidget.Cast(m_wRoot.FindAnyWidget("RangeText"));
		if (rangeText)
			rangeText.SetText(string.Format("%1 to %2", LC_Radio.FormatFrequency(transceiver.GetMinFrequency()), LC_Radio.FormatFrequency(transceiver.GetMaxFrequency())));

		float megahertz = transceiver.GetFrequency() / 1000.0;
		m_wEdit.SetText(megahertz.ToString(-1, 3));
		m_Entry = entry;

		workspace.SetFocusedWidget(m_wEdit);
		m_wEdit.ActivateWriteMode();
		inputManager.AddActionListener(ACTION_CONFIRM, EActionTrigger.DOWN, OnConfirm);
		inputManager.AddActionListener(ACTION_CANCEL, EActionTrigger.DOWN, OnCancel);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		if (!m_wRoot)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener(ACTION_CONFIRM, EActionTrigger.DOWN, OnConfirm);
			inputManager.RemoveActionListener(ACTION_CANCEL, EActionTrigger.DOWN, OnCancel);
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			workspace.SetFocusedWidget(null);

		m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		m_wEdit = null;
		m_Entry = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame while open: keeps game controls blocked and the text box taking keys
	void Update()
	{
		if (!m_wRoot)
			return;

		// The radio was dropped or the player changed character
		if (!m_Entry || !m_Entry.GetTransceiver())
		{
			Close();
			return;
		}

		GetGame().GetInputManager().ActivateContext(CONTEXT, 1);
		if (m_wEdit && !m_wEdit.IsInWriteMode())
			m_wEdit.ActivateWriteMode();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnConfirm(float value = 0.0, EActionTrigger reason = 0)
	{
		if (!m_wRoot)
			return;

		SCR_VONEntryRadio entry = m_Entry;
		string text = m_wEdit.GetText();
		Close();

		LC_Client client = LC_Client.Get();
		if (client && entry)
			client.OnFrequencyTyped(entry, text);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCancel(float value = 0.0, EActionTrigger reason = 0)
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Frequency in kHz for typed MHz, snapped to the radio's channel spacing and limits, or 0 if not a number
	static int ParseFrequency(string text, notnull BaseTransceiver transceiver)
	{
		text.Replace(",", ".");
		float megahertz = text.ToFloat();
		if (megahertz <= 0)
			return 0;

		int minimum = transceiver.GetMinFrequency();
		int maximum = transceiver.GetMaxFrequency();
		int resolution = Math.Max(1, transceiver.GetFrequencyResolution());

		int frequency = Math.Round(megahertz * 1000);
		frequency = Math.ClampInt(frequency, minimum, maximum);

		float exactSteps = frequency - minimum;
		exactSteps /= resolution;
		int steps = Math.Round(exactSteps);
		frequency = Math.ClampInt(minimum + steps * resolution, minimum, maximum);
		return frequency;
	}
}
