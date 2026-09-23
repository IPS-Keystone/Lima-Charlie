//------------------------------------------------------------------------------------------------
//! Local client side of the TeamSpeak integration: obtains the session from the server, handles the
//! Lima Charlie inputs and keeps the plugin bridge files up to date every frame while the local player controller exists.
class LC_Client
{
	protected static const int SESSION_RETRY_MS = 2000;
	//! LC_RadioPTT1 to LC_RadioPTT4: transmit keys, each sending on the channel assigned in LC_RadioSettings
	protected static const string ACTION_RADIO_PTT = "LC_RadioPTT";
	protected static const string ACTION_RADIO_KEY_ASSIGN = "LC_RadioKeyAssign";
	protected static const string ACTION_RADIO_EAR_CYCLE = "LC_RadioEarCycle";
	protected static const string ACTION_RADIO_BEEP_CYCLE = "LC_RadioBeepCycle";
	//! Radio setting actions below live in VONMenuContext, so they only work with the VON radial menu open
	protected static const string ACTION_RADIO_VOLUME = "LC_RadioVolume";
	protected static const string ACTION_RADIO_VOLUME_CYCLE = "LC_RadioVolumeCycle";
	protected static const string ACTION_RADIO_FREQUENCY = "LC_RadioFrequencyInput";
	//! Key-up spam lockout: more radio key-ups than this inside the window refuses radio transmission for the
	//! lockout, with a deny tone for each attempt. (Quick re-keys are also merged by the plugin.)
	protected static const int KEY_SPAM_MAX_KEY_UPS = 4;
	protected static const int KEY_SPAM_WINDOW_MS = 4000;
	protected static const int KEY_SPAM_LOCKOUT_MS = 2000;
	protected static const string UI_SOUND_SET = "ui";

	protected static ref LC_Client s_Instance;

	//! Weak reference: becomes null when the controller is deleted, which ends the client
	protected SCR_PlayerController m_PlayerController;
	protected ref LC_GameStateWriter m_Writer = new LC_GameStateWriter();
	protected ref LC_PluginStateReader m_Reader = new LC_PluginStateReader();
	protected ref LC_Hud m_Hud = new LC_Hud();
	protected ref LC_VonDisplayFeed m_VonDisplay = new LC_VonDisplayFeed();
	protected ref LC_StatusNotice m_StatusNotice = new LC_StatusNotice();
	protected ref LC_RadioSettings m_RadioSettings = new LC_RadioSettings();
	protected ref LC_RadioLinks m_RadioLinks = new LC_RadioLinks();
	protected ref LC_SoundQueue m_SoundQueue = new LC_SoundQueue();
	protected ref LC_FrequencyInput m_FrequencyInput = new LC_FrequencyInput();
	protected ref array<SCR_VONEntryRadio> m_aRadioEntries = {};

	protected bool m_bHasSession;
	protected string m_sSessionToken;
	protected string m_sTeamSpeakChannel;
	protected string m_sTeamSpeakChannelPassword;
	protected int m_iNextSessionRequestTick;
	protected int m_iStartTick;
	protected int m_iLastTick;

	//! Held Lima Charlie transmit key index, or -1
	protected int m_iRadioPTTKey = -1;

	//! Transmission resolved once per frame, after the spam lockout
	protected EVONTransmitType m_eTransmitType = EVONTransmitType.NONE;
	protected SCR_VONEntryRadio m_TransmitEntry;
	protected bool m_bWantedRadio;
	protected ref array<int> m_aKeyUpTicks = {};
	protected int m_iLockoutUntilTick;

	//! How often the server is told we are speaking, while we are
	protected static const int AI_HEARING_INTERVAL_MS = 1000;
	protected int m_iNextAIHearingTick;
	//! Server setting; the server checks it too, this only saves the RPCs
	protected bool m_bAIHearing = true;
	//! Server setting passed on to the plugin: share of a radio's range heard perfectly before garbling
	protected float m_fCleanFraction = 0.35;
	//! Server setting: multiplier on the terrain a signal has to climb over
	protected float m_fTerrainFactor = 1;
	//! Server setting passed on to the plugin: how far out other people's transmission beeps are heard
	protected float m_fBeepFraction = 0.9;
	//! Server setting: a Game Master transmits without a range or terrain limit while the editor is open
	protected bool m_bGameMasterUnlimitedRange = true;
	//! Server setting: log what we send the plugin and what it reports hearing, for troubleshooting
	protected bool m_bDiagnosticLog;
	//! Server setting: log what the engine's room model says about the people nearby
	protected bool m_bRoomDiagnosticLog;

	//------------------------------------------------------------------------------------------------
	static LC_Client Get()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	static void Start(notnull SCR_PlayerController playerController)
	{
		if (s_Instance && s_Instance.m_PlayerController)
			return;

		if (s_Instance)
			Stop();

		s_Instance = new LC_Client();
		s_Instance.m_PlayerController = playerController;
		s_Instance.m_iStartTick = System.GetTickCount();
		s_Instance.m_iLastTick = s_Instance.m_iStartTick;
		GetGame().GetCallqueue().CallLater(s_Instance.Tick, 0, true);

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
			s_Instance.SetInputListeners(inputManager, true);

		Print("[LC] Client started", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		if (!s_Instance)
			return;

		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(s_Instance.Tick);

		if (game && game.GetInputManager())
			s_Instance.SetInputListeners(game.GetInputManager(), false);

		s_Instance.m_FrequencyInput.Close();
		s_Instance.m_Hud.Release();
		s_Instance.m_VonDisplay.Release();
		s_Instance.m_Writer.WriteLeftGame();
		s_Instance = null;
		Print("[LC] Client stopped", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Interface tone from the plugin's "ui" sound set (cycle, confirm, deny, direct_on, direct_off)
	static void PlayUiSound(string name)
	{
		if (s_Instance)
			s_Instance.m_SoundQueue.Add(UI_SOUND_SET, name);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetInputListeners(notnull InputManager inputManager, bool add)
	{
		if (add)
		{
			inputManager.AddActionListener(LC_VoiceLevel.ACTION_CYCLE, EActionTrigger.DOWN, OnVoiceLevelCycle);
			inputManager.AddActionListener(ACTION_RADIO_KEY_ASSIGN, EActionTrigger.DOWN, OnRadioKeyAssign);
			inputManager.AddActionListener(ACTION_RADIO_EAR_CYCLE, EActionTrigger.DOWN, OnRadioEarCycle);
			inputManager.AddActionListener(ACTION_RADIO_BEEP_CYCLE, EActionTrigger.DOWN, OnRadioBeepCycle);
			inputManager.AddActionListener(ACTION_RADIO_VOLUME_CYCLE, EActionTrigger.DOWN, OnRadioVolumeCycle);
			inputManager.AddActionListener(ACTION_RADIO_FREQUENCY, EActionTrigger.DOWN, OnRadioFrequencyInput);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "1", EActionTrigger.DOWN, OnRadioPTT1);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "1", EActionTrigger.UP, OnRadioPTT1);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "2", EActionTrigger.DOWN, OnRadioPTT2);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "2", EActionTrigger.UP, OnRadioPTT2);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "3", EActionTrigger.DOWN, OnRadioPTT3);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "3", EActionTrigger.UP, OnRadioPTT3);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "4", EActionTrigger.DOWN, OnRadioPTT4);
			inputManager.AddActionListener(ACTION_RADIO_PTT + "4", EActionTrigger.UP, OnRadioPTT4);
			return;
		}

		inputManager.RemoveActionListener(LC_VoiceLevel.ACTION_CYCLE, EActionTrigger.DOWN, OnVoiceLevelCycle);
		inputManager.RemoveActionListener(ACTION_RADIO_KEY_ASSIGN, EActionTrigger.DOWN, OnRadioKeyAssign);
		inputManager.RemoveActionListener(ACTION_RADIO_EAR_CYCLE, EActionTrigger.DOWN, OnRadioEarCycle);
		inputManager.RemoveActionListener(ACTION_RADIO_BEEP_CYCLE, EActionTrigger.DOWN, OnRadioBeepCycle);
		inputManager.RemoveActionListener(ACTION_RADIO_VOLUME_CYCLE, EActionTrigger.DOWN, OnRadioVolumeCycle);
		inputManager.RemoveActionListener(ACTION_RADIO_FREQUENCY, EActionTrigger.DOWN, OnRadioFrequencyInput);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "1", EActionTrigger.DOWN, OnRadioPTT1);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "1", EActionTrigger.UP, OnRadioPTT1);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "2", EActionTrigger.DOWN, OnRadioPTT2);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "2", EActionTrigger.UP, OnRadioPTT2);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "3", EActionTrigger.DOWN, OnRadioPTT3);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "3", EActionTrigger.UP, OnRadioPTT3);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "4", EActionTrigger.DOWN, OnRadioPTT4);
		inputManager.RemoveActionListener(ACTION_RADIO_PTT + "4", EActionTrigger.UP, OnRadioPTT4);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!m_PlayerController)
		{
			Stop();
			return;
		}

		int now = System.GetTickCount();
		float elapsedMs = now - m_iLastTick;
		m_iLastTick = now;

		if (!m_bHasSession && now >= m_iNextSessionRequestTick)
		{
			m_iNextSessionRequestTick = now + SESSION_RETRY_MS;
			m_PlayerController.LC_RequestSession();
		}

		RefreshRadioEntries();
		ReleaseStuckRadioPTT();
		UpdateTransmit(now);
		UpdateRadioVolumeInput();
		m_FrequencyInput.Update();

		m_Reader.Update(now, m_bDiagnosticLog);
		UpdateAIHearing(now);
		m_RadioLinks.Update(now, m_Reader.GetRadioRequests(), GetRadioPosition(), m_fTerrainFactor);
		m_Writer.Update(now, this);
		m_Hud.Update(elapsedMs / 1000);
		m_VonDisplay.Update(this, now);
		m_StatusNotice.Update(now, this);
	}

	//------------------------------------------------------------------------------------------------
	//! While the plugin reports us talking, tell the server so AI in earshot react to the noise
	protected void UpdateAIHearing(int now)
	{
		if (!m_bAIHearing || now < m_iNextAIHearingTick || !m_Reader.IsSelfTalking())
			return;

		m_iNextAIHearingTick = now + AI_HEARING_INTERVAL_MS;
		m_PlayerController.LC_ReportVoice(LC_VoiceLevel.GetRange());
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshRadioEntries()
	{
		m_aRadioEntries.Clear();
		SCR_VONController vonController = GetVONController();
		if (vonController)
			vonController.LC_GetRadioEntries(m_aRadioEntries);
	}

	//------------------------------------------------------------------------------------------------
	//! A context switch (chat, menus) swallows the release event, as vanilla VON notes, so check the key is still held
	protected void ReleaseStuckRadioPTT()
	{
		if (m_iRadioPTTKey < 0)
			return;

		int number = m_iRadioPTTKey + 1;
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager || !inputManager.IsActionActive(ACTION_RADIO_PTT + number.ToString()))
			m_iRadioPTTKey = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves what the player is transmitting on (a held Lima Charlie transmit key first, then vanilla VON) and applies
	//! the key-up spam lockout to radio transmissions
	protected void UpdateTransmit(int now)
	{
		EVONTransmitType transmitType = EVONTransmitType.NONE;
		SCR_VONEntryRadio entry;
		SCR_VONController vonController = GetVONController();
		if (vonController)
		{
			entry = GetRadioPTTEntry(vonController);
			if (entry)
			{
				transmitType = EVONTransmitType.CHANNEL;
				if (LC_Radio.IsLongRange(entry))
					transmitType = EVONTransmitType.LONG_RANGE;
			}
			else
			{
				transmitType = vonController.LC_GetTransmitType();
				entry = vonController.LC_GetTransmitEntry();
			}
		}

		bool wantsRadio = entry && (transmitType == EVONTransmitType.CHANNEL || transmitType == EVONTransmitType.LONG_RANGE);
		bool locked = now < m_iLockoutUntilTick;
		if (wantsRadio && !m_bWantedRadio && locked)
			PlayUiSound("deny");

		if (!wantsRadio && m_bWantedRadio && !locked)
			CountKeyUp(now);

		m_bWantedRadio = wantsRadio;
		if (wantsRadio && now < m_iLockoutUntilTick)
		{
			m_eTransmitType = EVONTransmitType.NONE;
			m_TransmitEntry = null;
			return;
		}

		m_eTransmitType = transmitType;
		m_TransmitEntry = entry;
	}

	//------------------------------------------------------------------------------------------------
	protected void CountKeyUp(int now)
	{
		m_aKeyUpTicks.Insert(now);
		for (int i = m_aKeyUpTicks.Count() - 1; i >= 0; i--)
		{
			if (now - m_aKeyUpTicks[i] > KEY_SPAM_WINDOW_MS)
				m_aKeyUpTicks.Remove(i);
		}

		if (m_aKeyUpTicks.Count() <= KEY_SPAM_MAX_KEY_UPS)
			return;

		m_aKeyUpTicks.Clear();
		m_iLockoutUntilTick = now + KEY_SPAM_LOCKOUT_MS;
		PlayUiSound("deny");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnVoiceLevelCycle(float value = 0.0, EActionTrigger reason = 0)
	{
		m_Hud.ShowVoiceLevel(LC_VoiceLevel.Cycle());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioPTT1(float value = 0.0, EActionTrigger reason = 0)
	{
		OnRadioPTT(0, reason);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioPTT2(float value = 0.0, EActionTrigger reason = 0)
	{
		OnRadioPTT(1, reason);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioPTT3(float value = 0.0, EActionTrigger reason = 0)
	{
		OnRadioPTT(2, reason);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioPTT4(float value = 0.0, EActionTrigger reason = 0)
	{
		OnRadioPTT(3, reason);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioPTT(int key, EActionTrigger reason)
	{
		if (reason == EActionTrigger.UP)
		{
			if (m_iRadioPTTKey == key)
				m_iRadioPTTKey = -1;

			return;
		}

		m_iRadioPTTKey = key;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the selected channel on the next transmit key
	protected void OnRadioKeyAssign(float value = 0.0, EActionTrigger reason = 0)
	{
		SCR_VONEntryRadio entry = GetHoveredRadioEntry();
		if (!entry)
			return;

		m_RadioSettings.CycleKey(entry, m_aRadioEntries);
		RefreshRadioMenu();
		PlayUiSound("cycle");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioEarCycle(float value = 0.0, EActionTrigger reason = 0)
	{
		SCR_VONEntryRadio entry = GetHoveredRadioEntry();
		if (!entry)
			return;

		m_RadioSettings.CycleEar(entry);
		RefreshRadioMenu();
		PlaySampleBeep(entry);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioBeepCycle(float value = 0.0, EActionTrigger reason = 0)
	{
		SCR_VONEntryRadio entry = GetHoveredRadioEntry();
		if (!entry)
			return;

		m_RadioSettings.CycleBeepSet(entry);
		RefreshRadioMenu();
		PlaySampleBeep(entry);
	}

	//------------------------------------------------------------------------------------------------
	//! Steps the hovered channel down 10%, wrapping from silent back to full
	protected void OnRadioVolumeCycle(float value = 0.0, EActionTrigger reason = 0)
	{
		SCR_VONEntryRadio entry = GetHoveredRadioEntry();
		if (!entry)
			return;

		m_RadioSettings.CycleVolume(entry);
		RefreshRadioMenu();
	}

	//------------------------------------------------------------------------------------------------
	//! Ctrl + scroll is an analogue action, read every frame as Enhanced Radio does (scroll up is louder)
	protected void UpdateRadioVolumeInput()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		float value = inputManager.GetActionValue(ACTION_RADIO_VOLUME);
		if (value > 0)
			AdjustRadioVolume(1);
		else if (value < 0)
			AdjustRadioVolume(-1);
	}

	//------------------------------------------------------------------------------------------------
	protected void AdjustRadioVolume(int direction)
	{
		SCR_VONEntryRadio entry = GetHoveredRadioEntry();
		if (!entry)
			return;

		m_RadioSettings.AdjustVolume(entry, direction);
		RefreshRadioMenu();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioFrequencyInput(float value = 0.0, EActionTrigger reason = 0)
	{
		SCR_VONEntryRadio entry = GetHoveredRadioEntry();
		if (!entry)
			return;

		int number = m_aRadioEntries.Find(entry) + 1;
		if (!m_FrequencyInput.Open(entry, "Radio " + number.ToString() + " frequency (MHz)"))
			PlayUiSound("deny");
	}

	//------------------------------------------------------------------------------------------------
	//! Called by LC_FrequencyInput when the player confirms a typed frequency
	void OnFrequencyTyped(notnull SCR_VONEntryRadio entry, string text)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		if (!transceiver)
			return;

		int frequency = LC_FrequencyInput.ParseFrequency(text, transceiver);
		if (frequency <= 0)
		{
			PlayUiSound("deny");
			return;
		}

		entry.LC_SetFrequency(frequency);
		RefreshRadioEntries();
		RefreshRadioMenu();
		PlayUiSound("confirm");
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaySampleBeep(notnull SCR_VONEntryRadio entry)
	{
		m_SoundQueue.Add(m_RadioSettings.GetBeepSet(entry), "local_start", m_RadioSettings.GetEar(entry), m_RadioSettings.GetVolume(entry));
	}

	//------------------------------------------------------------------------------------------------
	//! A setting changed on a channel: the radio menu entry shows the new value, so it only needs redrawing
	protected void RefreshRadioMenu()
	{
		SCR_VONController vonController = GetVONController();
		if (vonController)
			vonController.LC_RefreshMenu();
	}

	//------------------------------------------------------------------------------------------------
	//! Channel the radio setting inputs act on: the one hovered in the open VON radial menu, or null
	protected SCR_VONEntryRadio GetHoveredRadioEntry()
	{
		RefreshRadioEntries();
		SCR_VONController vonController = GetVONController();
		if (!vonController)
			return null;

		SCR_VONEntryRadio entry = vonController.LC_GetHoveredRadioEntry();
		if (!entry || !m_aRadioEntries.Contains(entry))
			return null;

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! Gameplay settings the server owns, from LC_Settings
	void OnSettingsReceived(float cleanFraction, float beepFraction, float terrainFactor, bool aiHearing, bool gameMasterUnlimitedRange, int diagnosticFlags, string channelLabels, int channelNaming)
	{
		LC_ChannelLabels.Unpack(channelLabels);
		LC_ChannelLabels.SetNaming(channelNaming);
		m_fCleanFraction = cleanFraction;
		m_fBeepFraction = beepFraction;
		m_fTerrainFactor = terrainFactor;
		m_bAIHearing = aiHearing;
		m_bGameMasterUnlimitedRange = gameMasterUnlimitedRange;
		m_bDiagnosticLog = (diagnosticFlags & 1) != 0;
		m_bRoomDiagnosticLog = (diagnosticFlags & 2) != 0;

		int cleanPercent = Math.Round(cleanFraction * 100);
		int beepPercent = Math.Round(beepFraction * 100);
		int terrainPercent = Math.Round(terrainFactor * 100);
		Print("[LC] Clean radio range " + cleanPercent.ToString() + " percent, beep range " + beepPercent.ToString() + " percent, terrain effect " + terrainPercent.ToString() + " percent, AI hearing " + aiHearing.ToString() + ", Game Master unlimited range " + gameMasterUnlimitedRange.ToString() + ", diagnostic log " + m_bDiagnosticLog.ToString() + ", room diagnostic log " + m_bRoomDiagnosticLog.ToString(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Where garbling starts, as a share of each radio's range. The range itself is never changed.
	float GetCleanFraction()
	{
		return m_fCleanFraction;
	}

	//------------------------------------------------------------------------------------------------
	//! How far out other people's transmission beeps are still heard, as a share of range
	float GetBeepFraction()
	{
		return m_fBeepFraction;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server lets a Game Master transmit past the range and terrain rules
	bool GetGameMasterUnlimitedRange()
	{
		return m_bGameMasterUnlimitedRange;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server asked every client to log what the engine's room model says
	bool GetRoomDiagnostics()
	{
		return m_bRoomDiagnosticLog;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server asked every client to log its side of the bridge
	bool GetDiagnosticLog()
	{
		return m_bDiagnosticLog;
	}

	//------------------------------------------------------------------------------------------------
	void OnSessionReceived(string token, string teamSpeakChannel, string teamSpeakChannelPassword)
	{
		m_bHasSession = true;
		m_sSessionToken = token;
		m_sTeamSpeakChannel = teamSpeakChannel;
		m_sTeamSpeakChannelPassword = teamSpeakChannelPassword;
		Print("[LC] Session received, TeamSpeak channel: '" + teamSpeakChannel + "'", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	bool IsFor(IEntity playerController)
	{
		return m_PlayerController && m_PlayerController == playerController;
	}

	//------------------------------------------------------------------------------------------------
	int GetPlayerId()
	{
		return m_PlayerController.GetPlayerId();
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetControlledEntity()
	{
		return m_PlayerController.GetControlledEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Where the local player hears from: the active camera, falling back to the controlled entity
	void GetListenerTransform(out vector position, out vector direction)
	{
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager)
		{
			CameraBase camera = cameraManager.CurrentCamera();
			if (camera)
			{
				vector transform[4];
				camera.GetWorldTransform(transform);
				position = transform[3];
				direction = transform[2];
				return;
			}
		}

		IEntity entity = GetControlledEntity();
		if (entity)
		{
			position = entity.GetOrigin();
			direction = entity.GetTransformAxis(2);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Where the local player's radios are for terrain checks: the character's head, not a third person camera
	vector GetRadioPosition()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetControlledEntity());
		if (character)
			return character.EyePosition();

		vector position;
		vector direction;
		GetListenerTransform(position, direction);
		return position;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_VONController GetVONController()
	{
		return SCR_VONController.Cast(m_PlayerController.FindComponent(SCR_VONController));
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_VONEntryRadio GetRadioPTTEntry(notnull SCR_VONController vonController)
	{
		if (m_iRadioPTTKey < 0)
			return null;

		SCR_VONEntryRadio entry = m_RadioSettings.GetKeyEntry(m_iRadioPTTKey, m_aRadioEntries);
		if (!entry || !vonController.LC_CanTransmitOn(entry))
			return null;

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! What the local player is transmitting on this frame
	EVONTransmitType GetTransmitType()
	{
		return m_eTransmitType;
	}

	//------------------------------------------------------------------------------------------------
	//! Radio entry being transmitted on this frame, or null for none or direct speech
	SCR_VONEntryRadio GetTransmitRadioEntry()
	{
		return m_TransmitEntry;
	}

	//------------------------------------------------------------------------------------------------
	array<SCR_VONEntryRadio> GetRadioEntries()
	{
		return m_aRadioEntries;
	}

	//------------------------------------------------------------------------------------------------
	LC_RadioSettings GetRadioSettings()
	{
		return m_RadioSettings;
	}

	//------------------------------------------------------------------------------------------------
	LC_RadioLinks GetRadioLinks()
	{
		return m_RadioLinks;
	}

	//------------------------------------------------------------------------------------------------
	LC_SoundQueue GetSoundQueue()
	{
		return m_SoundQueue;
	}

	//------------------------------------------------------------------------------------------------
	string GetSessionToken()
	{
		return m_sSessionToken;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the server has answered with this session's token and settings yet
	bool HasSession()
	{
		return m_bHasSession;
	}

	//------------------------------------------------------------------------------------------------
	string GetTeamSpeakChannel()
	{
		return m_sTeamSpeakChannel;
	}

	//------------------------------------------------------------------------------------------------
	string GetTeamSpeakChannelPassword()
	{
		return m_sTeamSpeakChannelPassword;
	}

	//------------------------------------------------------------------------------------------------
	LC_PluginStateReader GetPluginState()
	{
		return m_Reader;
	}
}
