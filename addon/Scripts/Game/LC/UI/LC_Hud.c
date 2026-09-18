//------------------------------------------------------------------------------------------------
//! Bottom right HUD, just above where Cupcake's Stance Indicator draws: the TFAR-style voice level icon
//! (TFAR artwork, APL-SA), shown then faded out. Radio setting changes are read off the radio menu itself
//! rather than announced here.
//! The layout is created at runtime in the HUD manager's layer, so no player controller prefab override is
//! needed and it works with or without that mod.
class LC_Hud
{
	protected static const ResourceName LAYOUT = "{6A5C69DD20000010}UI/layouts/HUD/LC_VoiceLevel.layout";
	protected static const ResourceName ICON_WHISPER = "{C57805214E74B591}UI/Textures/VoiceLevel/LC_VoiceLevel_Whisper.edds";
	protected static const ResourceName ICON_NORMAL = "{C000E7C94EE19B30}UI/Textures/VoiceLevel/LC_VoiceLevel_Normal.edds";
	protected static const ResourceName ICON_SHOUT = "{46DDE1C8DD026D27}UI/Textures/VoiceLevel/LC_VoiceLevel_Shout.edds";
	protected static const float FADE_SECONDS = 6;

	protected Widget m_wRoot;
	protected ImageWidget m_wVoiceLevelIcon;
	protected float m_fVoiceLevelRemaining;

	//------------------------------------------------------------------------------------------------
	//! Shows the icon for the level at full opacity and restarts its fade
	void ShowVoiceLevel(int level)
	{
		if (!CreateWidgets() || !m_wVoiceLevelIcon)
			return;

		m_wVoiceLevelIcon.LoadImageTexture(0, GetIcon(level));
		m_wVoiceLevelIcon.SetOpacity(1);
		m_fVoiceLevelRemaining = FADE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	void Update(float timeSlice)
	{
		m_fVoiceLevelRemaining = Fade(m_wVoiceLevelIcon, m_fVoiceLevelRemaining, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the widget references. The HUD manager owns the widgets and deletes them with the player controller.
	void Release()
	{
		m_wRoot = null;
		m_wVoiceLevelIcon = null;
		m_fVoiceLevelRemaining = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Fade(Widget widget, float remaining, float timeSlice)
	{
		if (!widget || remaining <= 0)
			return 0;

		remaining = Math.Max(0, remaining - timeSlice);
		widget.SetOpacity(remaining / FADE_SECONDS);
		return remaining;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CreateWidgets()
	{
		if (m_wRoot)
			return true;

		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager)
			return false;

		m_wRoot = hudManager.CreateLayout(LAYOUT, EHudLayers.LOW);
		if (!m_wRoot)
			return false;

		m_wVoiceLevelIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("VoiceLevelIcon"));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static ResourceName GetIcon(int level)
	{
		switch (level)
		{
			case LC_VoiceLevel.WHISPER:
				return ICON_WHISPER;

			case LC_VoiceLevel.SHOUT:
				return ICON_SHOUT;
		}

		return ICON_NORMAL;
	}
}
