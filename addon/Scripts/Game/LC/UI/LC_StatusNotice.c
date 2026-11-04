//------------------------------------------------------------------------------------------------
//! The "is this thing working" notice, shown once per connection, shortly after the player first spawns
//! in (or opens the Game Master editor). Both halves of Lima Charlie have to be running and TeamSpeak has
//! to be connected before a player can hear anything, and none of those failures announce themselves:
//! without this, a player with TeamSpeak closed simply hears silence and assumes the mod is broken.
//! It is not re-shown when something changes later (a microphone toggle, a channel move): the HUD and
//! TeamSpeak itself say that, and a notice for each was noise.
class LC_StatusNotice
{
	//! After spawning in, how long to give the plugin to report itself before saying it is not there.
	//! The plugin writes at least every 250 ms, so this is only generous for a machine still loading.
	protected static const int PLUGIN_GRACE_MS = 6000;
	//! After spawning in, how long to wait for the server's session before reporting that it never came
	protected static const int SESSION_GRACE_MS = 15000;
	//! The deploy menu and spawn fade are still closing when the character arrives; a hint shown then is missed
	protected static const int SPAWN_SETTLE_MS = 1500;
	protected static const int HINT_SECONDS = 12;

	//! Tick the player was first seen in the world (character or editor), 0 before then
	protected int m_iInWorldTick;
	protected bool m_bShown;
	//! The hint we showed, and when to take it down ourselves: the Game Master editor makes every hint
	//! endless (SCR_HintEditorComponent sets a duration override of -1), so its own timer cannot be trusted
	protected SCR_HintUIInfo m_Hint;
	protected int m_iHideTick;

	//------------------------------------------------------------------------------------------------
	void Update(int now, notnull LC_Client client)
	{
		if (m_bShown)
		{
			UpdateHide(now);
			return;
		}

		if (m_iInWorldTick == 0)
		{
			if (!IsInWorld(client))
				return;

			m_iInWorldTick = now;
		}

		int sinceSpawn = now - m_iInWorldTick;
		if (sinceSpawn < SPAWN_SETTLE_MS)
			return;

		// Normally we wait for the session, so the channel name is known before saying anything. A session
		// that never arrives is itself worth reporting: the mod is loaded here but the server is not
		// answering, which looks exactly like the mod being broken.
		if (!client.HasSession())
		{
			if (sinceSpawn < SESSION_GRACE_MS)
				return;

			m_bShown = true;
			ShowNoSession(now);
			return;
		}

		LC_PluginStateReader reader = client.GetPluginState();
		bool plugin = reader.IsPluginRunning(now);

		// Shown as soon as the plugin answers, or once it has had long enough not to
		if (!plugin && sinceSpawn < PLUGIN_GRACE_MS)
			return;

		m_bShown = true;
		bool connected = plugin && reader.IsTeamSpeakConnected();
		bool inChannel = plugin && reader.IsInGameChannel();
		bool micMuted = plugin && reader.IsMicMuted();
		Show(now, client, reader, plugin, connected, inChannel, micMuted);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawned in as a character, or looking through the Game Master camera without one
	protected bool IsInWorld(notnull LC_Client client)
	{
		if (client.GetControlledEntity())
			return true;

		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (!editorManager)
			return false;

		return editorManager.IsOpened();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHide(int now)
	{
		if (m_iHideTick == 0 || now < m_iHideTick)
			return;

		m_iHideTick = 0;
		// Hide only hides the hint passed in if it is still the latest one, so another mod's hint is safe
		if (m_Hint)
			SCR_HintManagerComponent.HideHint(m_Hint);

		m_Hint = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Show(int now, notnull LC_Client client, notnull LC_PluginStateReader reader, bool plugin, bool connected, bool inChannel, bool micMuted)
	{
		bool allWell = plugin && connected && !micMuted;
		string text = "Mod loaded: " + LC_Version.VERSION;

		if (!plugin)
			text += "\nTeamSpeak plugin: not running. Start TeamSpeak with the Lima Charlie plugin enabled.";
		else
			text += "\nTeamSpeak plugin: " + reader.GetPluginVersion();

		if (!plugin)
		{
			// Nothing downstream of the plugin can be known without it
			text += "\nTeamSpeak: unknown";
		}
		else if (!connected)
		{
			text += "\nTeamSpeak: not connected to a server. Connect to your group's server.";
		}
		else
		{
			text += "\nTeamSpeak: connected";
			// Being muted in TeamSpeak looks exactly like the mod being broken
			if (micMuted)
				text += "\nMicrophone: muted in TeamSpeak. Nobody can hear you until you unmute it.";
			else
				text += "\nMicrophone: on";
			text += "\n" + DescribeChannel(client, inChannel);
			// Nought is normal for whoever joins first, so it is reported without being treated as a fault
			text += "\nOthers with the plugin here: " + reader.GetPeers().ToString();
		}

		// Silent when everything is in place, so a working session is not announced with a noise every time
		if (!ShowHint(now, text, allWell))
		{
			// Hints obey the player's "show hints" interface setting, and the player most in need of this
			// notice is the one hearing nothing and wondering why, so fall back to a popup, which obeys a different one
			SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
			if (popup)
				popup.PopupMsg("Lima Charlie " + LC_Version.VERSION, HINT_SECONDS, Summarise(plugin, connected, micMuted));
		}

		Print("[LC] Status: " + text, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! The mod is running here, but the server has not answered with a session, so nothing can be heard
	protected void ShowNoSession(int now)
	{
		string text = "Mod loaded: " + LC_Version.VERSION;
		text += "\nServer: no Lima Charlie session. This server may not be running the mod, or the scenario has no game mode.";

		if (!ShowHint(now, text, false))
		{
			SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
			if (popup)
				popup.PopupMsg("Lima Charlie " + LC_Version.VERSION, HINT_SECONDS, "No session from the server");
		}

		Print("[LC] Status: " + text, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when hints are switched off (or another hint outranks ours), so the caller can fall back
	protected bool ShowHint(int now, string text, bool silent)
	{
		if (!SCR_HintManagerComponent.ShowCustomHint(text, "Lima Charlie", HINT_SECONDS, silent))
			return false;

		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (hintManager)
			m_Hint = hintManager.GetLatestHint();

		m_iHideTick = now + HINT_SECONDS * 1000;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One line for the popup, which has room for far less than the hint
	protected string Summarise(bool plugin, bool connected, bool micMuted)
	{
		if (!plugin)
			return "TeamSpeak plugin not running";

		if (!connected)
			return "TeamSpeak not connected to a server";

		if (micMuted)
			return "Ready, but microphone muted in TeamSpeak";

		return "Ready, microphone on";
	}

	//------------------------------------------------------------------------------------------------
	protected string DescribeChannel(notnull LC_Client client, bool inChannel)
	{
		string channel = client.GetTeamSpeakChannel();
		if (channel.IsEmpty())
			return "Channel: this server does not move anyone, stay where your group is";

		if (inChannel)
			return "Channel: " + channel;

		return "Channel: not in '" + channel + "' yet";
	}
}
