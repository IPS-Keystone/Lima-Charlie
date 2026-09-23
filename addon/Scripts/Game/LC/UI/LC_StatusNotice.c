//------------------------------------------------------------------------------------------------
//! The "is this thing working" notice, shown once shortly after joining and again whenever the answer
//! changes. Both halves of Lima Charlie have to be running and TeamSpeak has to be connected before a
//! player can hear anything, and none of those failures announce themselves: without this, a player with
//! TeamSpeak closed simply hears silence and assumes the mod is broken.
class LC_StatusNotice
{
	//! How long to give the plugin to report itself before saying it is not there. The plugin writes at
	//! least every 250 ms, so this is only generous for a machine still loading.
	protected static const int PLUGIN_GRACE_MS = 6000;
	//! A change is only worth interrupting someone for this often
	protected static const int RESHOW_MS = 15000;
	protected static const float HINT_SECONDS = 12;

	protected int m_iStartTick;
	protected bool m_bShown;
	protected int m_iLastShowTick;
	protected bool m_bLastPlugin;
	protected bool m_bLastConnected;
	protected bool m_bLastInChannel;

	//------------------------------------------------------------------------------------------------
	void Update(int now, notnull LC_Client client)
	{
		if (m_iStartTick == 0)
			m_iStartTick = now;

		// Waiting for the session keeps the channel name out of the notice until the server has sent it
		if (!client.HasSession())
			return;

		LC_PluginStateReader reader = client.GetPluginState();
		bool plugin = reader.IsPluginRunning(now);
		bool connected = plugin && reader.IsTeamSpeakConnected();
		bool inChannel = plugin && reader.IsInGameChannel();

		if (!m_bShown)
		{
			// Shown as soon as the plugin answers, or once it has had long enough not to
			if (!plugin && now - m_iStartTick < PLUGIN_GRACE_MS)
				return;
		}
		else
		{
			bool changed = plugin != m_bLastPlugin || connected != m_bLastConnected || inChannel != m_bLastInChannel;
			if (!changed || now - m_iLastShowTick < RESHOW_MS)
				return;
		}

		m_bShown = true;
		m_iLastShowTick = now;
		m_bLastPlugin = plugin;
		m_bLastConnected = connected;
		m_bLastInChannel = inChannel;

		Show(client, reader, plugin, connected, inChannel);
	}

	//------------------------------------------------------------------------------------------------
	protected void Show(notnull LC_Client client, notnull LC_PluginStateReader reader, bool plugin, bool connected, bool inChannel)
	{
		bool allWell = plugin && connected;
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
			text += "\n" + DescribeChannel(client, inChannel);
			// Nought is normal for whoever joins first, so it is reported without being treated as a fault
			text += "\nOthers with the plugin here: " + reader.GetPeers().ToString();
		}

		// Silent when everything is in place, so a working session is not announced with a noise every time
		bool shown = SCR_HintManagerComponent.ShowCustomHint(text, "Lima Charlie", HINT_SECONDS, allWell);

		// Hints obey the player's "show hints" interface setting, and the player most in need of this notice
		// is the one hearing nothing and wondering why, so fall back to a popup, which obeys a different one
		if (!shown)
		{
			SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
			if (popup)
				popup.PopupMsg("Lima Charlie " + LC_Version.VERSION, HINT_SECONDS, Summarise(plugin, connected));
		}

		Print("[LC] Status: " + text, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! One line for the popup, which has room for far less than the hint
	protected string Summarise(bool plugin, bool connected)
	{
		if (!plugin)
			return "TeamSpeak plugin not running";

		if (!connected)
			return "TeamSpeak not connected to a server";

		return "Ready";
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
