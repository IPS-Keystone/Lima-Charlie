//------------------------------------------------------------------------------------------------
//! TFAR-style voice level icon (TFAR artwork, APL-SA), shown bottom right just above where Cupcake's
//! Stance Indicator draws, then faded out. The layout is created at runtime in the HUD manager's layer,
//! so no player controller prefab override is needed and it works with or without that mod.
class LC_VoiceLevelHud
{
	protected static const ResourceName LAYOUT = "{6A5C69DD20000010}UI/layouts/HUD/LC_VoiceLevel.layout";
	protected static const ResourceName ICON_WHISPER = "{C57805214E74B591}UI/Textures/VoiceLevel/LC_VoiceLevel_Whisper.edds";
	protected static const ResourceName ICON_NORMAL = "{C000E7C94EE19B30}UI/Textures/VoiceLevel/LC_VoiceLevel_Normal.edds";
	protected static const ResourceName ICON_SHOUT = "{46DDE1C8DD026D27}UI/Textures/VoiceLevel/LC_VoiceLevel_Shout.edds";
	protected static const float FADE_SECONDS = 6;

	protected Widget m_wRoot;
	protected ImageWidget m_wIcon;
	protected float m_fFadeRemaining;

	//------------------------------------------------------------------------------------------------
	//! Shows the icon for the level at full opacity and restarts the fade
	void Show(int level)
	{
		if (!m_wIcon && !CreateWidgets())
			return;

		m_wIcon.LoadImageTexture(0, GetIcon(level));
		m_wIcon.SetOpacity(1);
		m_fFadeRemaining = FADE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	void Update(float timeSlice)
	{
		if (!m_wIcon || m_fFadeRemaining <= 0)
			return;

		m_fFadeRemaining = Math.Max(0, m_fFadeRemaining - timeSlice);
		m_wIcon.SetOpacity(m_fFadeRemaining / FADE_SECONDS);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the widget references. The HUD manager owns the widgets and deletes them with the player controller.
	void Release()
	{
		m_wRoot = null;
		m_wIcon = null;
		m_fFadeRemaining = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CreateWidgets()
	{
		SCR_HUDManagerComponent hudManager = SCR_HUDManagerComponent.GetHUDManager();
		if (!hudManager)
			return false;

		m_wRoot = hudManager.CreateLayout(LAYOUT, EHudLayers.LOW);
		if (!m_wRoot)
			return false;

		m_wIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("VoiceLevelIcon"));
		return m_wIcon != null;
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
