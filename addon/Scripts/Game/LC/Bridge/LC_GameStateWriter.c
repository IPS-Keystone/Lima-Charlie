//------------------------------------------------------------------------------------------------
//! The last occlusion result for one nearby player, and where both ends were when it was traced
class LC_MuffleSample
{
	float m_fMuffle;
	vector m_vListener;
	vector m_vSpeaker;
	int m_iTracedTick;
	int m_iSeenTick;
	//! Where this player was standing in the engine's room model when last checked
	ref LC_RoomLocation m_Room = new LC_RoomLocation();
	//! Whether the room model answered for this player, rather than a trace, for the diagnostic line
	bool m_bFromRooms;
	//! What the last trace for this player hit, for the diagnostic line
	string m_sCover;
}

//------------------------------------------------------------------------------------------------
//! Writes $profile:LimaCharlie/game_state.json for the TeamSpeak plugin. Format is documented in the
//! plugin's lc_game_state.h; bump PROTOCOL_VERSION on both sides for incompatible changes.
class LC_GameStateWriter
{
	protected static const string DIRECTORY = "$profile:LimaCharlie";
	protected static const string PATH = "$profile:LimaCharlie/game_state.json";
	protected static const int PROTOCOL_VERSION = 10;
	//! 20 Hz while anyone's voice is live; transmit, radio and terrain changes are written immediately
	protected static const int INTERVAL_MS = 50;
	//! 10 Hz when nobody is talking and we are not transmitting: positions still move, but nothing is audible
	protected static const int IDLE_INTERVAL_MS = 100;
	//! 2 Hz with no plugin reading any of it - TeamSpeak not running, or not yet started. Still often enough
	//! for the plugin to find a fresh state within half a second of starting up.
	protected static const int NO_PLUGIN_INTERVAL_MS = 500;
	//! Players beyond this distance cannot matter for direct speech
	protected static const float NEARBY_RANGE_M = 60;
	//! Occlusion traces are the expensive part, so each nearby player is re-traced only as often as it can
	//! matter: often while they talk, less often while silent (so the result is ready when they start), and
	//! rarely when neither end has moved, which is most of a briefing or a building clear
	protected static const int OCCLUSION_TALKING_MS = 50;
	protected static const int OCCLUSION_SILENT_MS = 250;
	protected static const int OCCLUSION_STILL_MS = 1000;
	protected static const float OCCLUSION_MOVE_M = 0.15;
	//! At most this many players are re-traced in one write, so a crowd arriving at once is spread over
	//! several frames instead of landing in one
	protected static const int OCCLUSION_BUDGET = 12;
	//! A player not seen nearby for this long has their cached result dropped
	protected static const int OCCLUSION_FORGET_MS = 5000;
	//! A camera further than this from the body it belongs to is a free camera: Game Master, spectator or photo
	//! mode. Character cameras, including a vehicle's third person boom, stay well inside it.
	protected static const float FREE_CAMERA_RANGE_M = 20;
	//! How often the whole state goes to the log while the server's diagnostic setting is on
	protected static const int DIAGNOSTIC_INTERVAL_MS = 1000;
	//! Nearby players listed in one room diagnostic line
	protected static const int DIAGNOSTIC_MAX_PLAYERS = 6;
	//! Regular diagnostics: one summary line this often
	protected static const int SUMMARY_INTERVAL_MS = 60000;
	//! Regular diagnostics: the radio list and the listener's building are checked this often, and a
	//! change logged; fast enough to place an event, slow enough that scrolling a frequency is one line
	protected static const int CHANGE_CHECK_MS = 3000;

	protected int m_iSeq;
	protected int m_iNextWriteTick;
	protected int m_iNextDiagnosticTick;
	protected EVONTransmitType m_eLastTransmitType = EVONTransmitType.NONE;
	protected float m_fLastVoiceRange;
	protected string m_sLastTransmitRadio;
	protected int m_iLastLinkRevision;
	protected int m_iLastSoundSeq;
	protected ref array<int> m_aPlayerIds = {};
	protected ref map<int, ref LC_MuffleSample> m_mMuffle = new map<int, ref LC_MuffleSample>();
	protected ref array<int> m_aForget = {};
	protected int m_iTraceBudget;
	protected ref LC_Occlusion m_Occlusion = new LC_Occlusion();
	protected ref LC_Rooms m_Rooms = new LC_Rooms();
	//! Where the listener is in the room model; the room checks are all relative to this
	protected ref LC_RoomLocation m_ListenerRoom = new LC_RoomLocation();
	protected int m_iNextRoomDiagnosticTick;
	protected bool m_bRoomDiagnostics;
	//! This write prints the room line, so the costly parts of the cover description are worth working out
	protected bool m_bRoomLineDue;
	protected int m_iRoomsResolved;
	protected int m_iRoomsTraced;
	protected string m_sRoomDiagnostic;

	//! Regular diagnostics: what was last logged, and the counts the summary line reports
	protected int m_iNextSummaryTick;
	protected int m_iNextChangeCheckTick;
	protected string m_sDiagRadios;
	//! The radios array of the last write, kept for the regular diagnostics' change line
	protected string m_sWrittenRadios;
	protected IEntity m_DiagBuilding;
	protected int m_iDiagArea = -1;
	protected int m_iDiagWrites;
	protected int m_iDiagNearbySum;
	protected int m_iDiagNearbyMax;
	protected int m_iDiagFromRooms;
	protected int m_iDiagFromTraces;
	protected int m_iDiagTraces;
	protected int m_iDiagProbes;

	//------------------------------------------------------------------------------------------------
	void LC_GameStateWriter()
	{
		FileIO.MakeDirectory(DIRECTORY);
	}

	//------------------------------------------------------------------------------------------------
	void Update(int now, notnull LC_Client client)
	{
		EVONTransmitType transmitType = client.GetTransmitType();
		float voiceRange = LC_VoiceLevel.GetRange();

		string transmitRadio;
		int transmitFrequency;
		SCR_VONEntryRadio transmitEntry = client.GetTransmitRadioEntry();
		if (transmitEntry && transmitType != EVONTransmitType.NONE && transmitType != EVONTransmitType.DIRECT)
		{
			// The entry can outlive its transceiver for a frame when the radio is dropped mid-transmission
			BaseTransceiver transceiver = transmitEntry.GetTransceiver();
			if (transceiver)
			{
				transmitRadio = LC_Radio.GetId(transmitEntry);
				transmitFrequency = transceiver.GetFrequency();
			}
		}

		int linkRevision = client.GetRadioLinks().GetRevision();
		int soundSeq = client.GetSoundQueue().GetSeq();

		// Only the cheap fields are compared every frame. Radio state is rebuilt on the write itself, so a
		// change to it is picked up at the next scheduled write rather than costing a string per frame; at
		// 10 Hz idle that is at most 100 ms, and the settings that change it all play a sound, which writes.
		bool changed = transmitType != m_eLastTransmitType || voiceRange != m_fLastVoiceRange || transmitRadio != m_sLastTransmitRadio
			|| linkRevision != m_iLastLinkRevision || soundSeq != m_iLastSoundSeq;
		if (!changed && now < m_iNextWriteTick)
			return;

		// A Game Master talks to the whole map from the camera, if the server allows it
		bool unlimitedRange = client.GetGameMasterUnlimitedRange() && IsEditorOpen();

		m_eLastTransmitType = transmitType;
		m_fLastVoiceRange = voiceRange;
		m_sLastTransmitRadio = transmitRadio;
		m_iLastLinkRevision = linkRevision;
		m_iLastSoundSeq = soundSeq;
		m_iNextWriteTick = now + GetWriteInterval(client, transmitType);

		m_sRoomDiagnostic = string.Empty;
		m_bRoomDiagnostics = client.GetRoomDiagnostics();
		m_bRoomLineDue = m_bRoomDiagnostics && now >= m_iNextRoomDiagnosticTick;
		string json = BuildInGameJson(client, transmitType, voiceRange, transmitRadio, transmitFrequency, now, unlimitedRange);
		WriteFile(json);
		LogDiagnostic(now, json, client.IsComprehensiveDiagnostics());
		LogRoomDiagnostic(now, m_bRoomDiagnostics);
		if (client.IsRegularDiagnostics())
			UpdateRegularDiagnostics(now, client, m_sWrittenRadios);
	}

	//------------------------------------------------------------------------------------------------
	//! Voice only needs fresh positions while someone can be heard
	protected int GetWriteInterval(notnull LC_Client client, EVONTransmitType transmitType)
	{
		LC_PluginStateReader reader = client.GetPluginState();

		// Nobody is reading this. Somebody playing without TeamSpeak running should not pay for the bridge.
		if (!reader.IsPluginRunning(System.GetTickCount()))
			return NO_PLUGIN_INTERVAL_MS;

		if (transmitType != EVONTransmitType.NONE)
			return INTERVAL_MS;

		if (reader.IsSelfTalking() || reader.IsAnyPlayerTalking())
			return INTERVAL_MS;

		return IDLE_INTERVAL_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! The state goes to the log once a second as well as to disk, so a transmit key can be held and read back
	//! afterwards. Only while the server's diagnostic setting is on: with a full server this is several
	//! kilobytes a second of log, and a Game Master can sit in the editor for an entire mission.
	protected void LogDiagnostic(int now, string json, bool enabled)
	{
		if (!enabled)
		{
			m_iNextDiagnosticTick = 0;
			return;
		}

		if (now < m_iNextDiagnosticTick)
			return;

		m_iNextDiagnosticTick = now + DIAGNOSTIC_INTERVAL_MS;
		Print("[LC] game_state " + json, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the plugin to release the microphone and return to the previous channel
	void WriteLeftGame()
	{
		m_iSeq++;
		WriteFile("{\"v\":" + PROTOCOL_VERSION.ToString() + ",\"seq\":" + m_iSeq.ToString() + ",\"inGame\":false}");
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildInGameJson(notnull LC_Client client, EVONTransmitType transmitType, float voiceRange, string transmitRadio, int transmitFrequency, int now, bool unlimitedRange)
	{
		m_iSeq++;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		int localPlayerId = client.GetPlayerId();
		IEntity localEntity = client.GetControlledEntity();

		vector listenerPosition;
		vector listenerDirection;
		client.GetListenerTransform(listenerPosition, listenerDirection);

		string json = "{\"v\":" + PROTOCOL_VERSION.ToString() + ",\"seq\":" + m_iSeq.ToString() + ",\"inGame\":true";

		json += ",\"session\":{\"token\":" + LC_Json.String(client.GetSessionToken());
		json += ",\"playerId\":" + localPlayerId.ToString();
		json += ",\"playerName\":" + LC_Json.String(playerManager.GetPlayerName(localPlayerId));
		json += ",\"tsChannel\":" + LC_Json.String(client.GetTeamSpeakChannel());
		json += ",\"tsChannelPassword\":" + LC_Json.String(client.GetTeamSpeakChannelPassword());
		json += ",\"modVersion\":" + LC_Json.String(LC_Version.VERSION) + "}";

		// Occlusion is traced from the listener's head, not the camera, so third person does not hear around walls.
		// A free camera is nowhere near the body it belongs to, and that camera is where the player really listens
		// from, so it is traced from instead and the body is ignored entirely.
		IEntity occlusionListener;
		vector occlusionOrigin = listenerPosition;
		if (localEntity)
		{
			vector headPosition = GetSpeakerPosition(localEntity);
			if (vector.DistanceSq(headPosition, listenerPosition) <= FREE_CAMERA_RANGE_M * FREE_CAMERA_RANGE_M)
			{
				occlusionListener = localEntity;
				occlusionOrigin = headPosition;
			}
		}

		// Which room the listener is in, and a slice of the work of mapping that building's type
		m_Rooms.Locate(m_ListenerRoom, occlusionOrigin, now);
		m_Rooms.Update(m_ListenerRoom);

		int transmit = transmitType;
		json += ",\"self\":{\"alive\":" + LC_Json.Bool(IsListening(localEntity));
		json += ",\"pos\":" + LC_Json.Position(listenerPosition);
		json += ",\"dir\":" + LC_Json.Direction(listenerDirection);
		json += ",\"tx\":" + transmit.ToString();
		json += ",\"txFrequency\":" + transmitFrequency.ToString();
		json += ",\"txRadio\":" + LC_Json.String(transmitRadio);
		json += ",\"voiceRange\":" + voiceRange.ToString(-1, 1);
		json += ",\"cleanFraction\":" + client.GetCleanFraction().ToString(-1, 2);
		json += ",\"beepFraction\":" + client.GetBeepFraction().ToString(-1, 2);
		json += ",\"unlimitedRx\":" + LC_Json.Bool(unlimitedRange);
		// Radios on the body transmit from where we listen, which is what every transmission came from before
		// radios had an antenna position of their own
		m_sWrittenRadios = LC_Radio.BuildJson(client.GetRadioEntries(), client.GetRadioSettings(), localEntity, listenerPosition, unlimitedRange);
		json += ",\"radios\":" + m_sWrittenRadios;
		// One beep volume for every channel, scaling each channel's own volume where the plugin plays beeps
		json += ",\"beepVolume\":" + client.GetRadioSettings().GetBeepVolume().ToString(-1, 2);
		// Volume of the room the listener is in, 0 outdoors: the plugin sizes its reverb from it
		json += ",\"roomVolume\":" + m_ListenerRoom.m_fVolume.ToString(-1, 0);
		// The listener's own hearing, while another mod reports one (an ear protection headset)
		LC_HearingProfile hearing = LC_Hearing.GetCombined(now);
		if (hearing)
			json += ",\"hearing\":" + LC_Hearing.BuildJson(hearing);
		else
			LC_Hearing.LogUnchanged();
		json += ",\"sounds\":" + client.GetSoundQueue().BuildJson();
		// The vehicle intercom channel we are on (LC_EIntercomChannel bits, 0 none)
		LC_VehicleSeat vehicleSeat = client.GetVehicleSeat();
		int intercom = vehicleSeat.GetIntercomChannel();
		json += ",\"ic\":" + intercom.ToString() + "}";

		json += ",\"players\":[";
		m_aPlayerIds.Clear();
		playerManager.GetPlayers(m_aPlayerIds);
		LC_PluginStateReader reader = client.GetPluginState();
		float maxDistanceSq = NEARBY_RANGE_M * NEARBY_RANGE_M;
		m_iTraceBudget = OCCLUSION_BUDGET;
		bool first = true;
		m_iRoomsResolved = 0;
		m_iRoomsTraced = 0;
		foreach (int playerId : m_aPlayerIds)
		{
			if (playerId == localPlayerId)
				continue;

			// Only players whose character is streamed in on this client
			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			if (!entity)
				continue;

			vector position = GetSpeakerPosition(entity);
			float distanceSq = vector.DistanceSq(position, listenerPosition);
			if (distanceSq > maxDistanceSq)
				continue;

			float muffle = GetMuffle(playerId, entity, position, occlusionListener, occlusionOrigin, distanceSq, reader.IsPlayerTalking(playerId), now);
			// Built only for the write that prints it: the line is once a second, writes are up to 20
			if (m_bRoomLineDue)
				AppendRoomDiagnostic(playerId, muffle);

			// Built on its own and appended once: every append copies the whole document so far, which with 60
			// players nearby made this loop 30% slower
			string player = "{\"id\":" + playerId.ToString();
			player += ",\"alive\":" + LC_Json.Bool(LC_Life.CanSpeak(entity));
			player += ",\"pos\":" + LC_Json.Position(position);
			player += ",\"muffle\":" + muffle.ToString(-1, 2);
			player += ",\"room\":" + GetRoomShare(playerId, muffle).ToString(-1, 2);
			// Their channel on our vehicle's intercom; 0 when they are not on it, so the plugin only compares bits
			int playerIntercom = vehicleSeat.GetPlayerIntercomChannel(playerId, entity);
			if (playerIntercom != 0)
				player += ",\"ic\":" + playerIntercom.ToString();
			player += "}";
			if (!first)
				player = "," + player;

			first = false;
			json += player;
		}

		json += "],\"links\":" + client.GetRadioLinks().BuildJson() + "}";
		ForgetMuffles(now);
		return json;
	}

	//------------------------------------------------------------------------------------------------
	//! How muffled a nearby player is. Both the engine's room model and a trace get a say, and the clearer
	//! of the two wins: a path through the rooms cannot tell whether the two can see each other, and a trace
	//! cannot tell that an open door two rooms away carries a voice. Only the same room needs neither, being
	//! clear already.
	protected float GetMuffle(int playerId, notnull IEntity speaker, vector speakerPosition, IEntity listener, vector listenerPosition, float distanceSq, bool talking, int now)
	{
		LC_MuffleSample sample = m_mMuffle.Get(playerId);
		if (!sample)
		{
			// Due straight away: the zero positions read as moved
			sample = new LC_MuffleSample();
			sample.m_iTracedTick = now - OCCLUSION_STILL_MS;
			m_mMuffle.Set(playerId, sample);
		}

		sample.m_iSeenTick = now;

		m_Rooms.Locate(sample.m_Room, speakerPosition, now);
		float roomMuffle;
		if (m_Rooms.GetMuffle(m_ListenerRoom, sample.m_Room, distanceSq, now, roomMuffle))
		{
			sample.m_bFromRooms = true;
			m_iRoomsResolved++;

			// Same room, or one open doorway apart: nothing a trace finds can improve on clear, and a wall
			// beside the doorway, a pillar or a crate between them must not make it worse
			if (roomMuffle <= 0)
			{
				// Any trace result is now stale: a later fallback has to trace again rather than reuse it
				sample.m_iTracedTick = now - OCCLUSION_STILL_MS;
				return 0;
			}

			// Otherwise the path is only an upper bound: a trace may find they can see each other
			return Math.Min(roomMuffle, TraceMuffle(sample, speaker, speakerPosition, listener, listenerPosition, talking, now));
		}

		sample.m_bFromRooms = false;
		m_iRoomsTraced++;
		return TraceMuffle(sample, speaker, speakerPosition, listener, listenerPosition, talking, now);
	}

	//------------------------------------------------------------------------------------------------
	//! The traced muffle for this player, from the cache unless it is due and there is budget left in this
	//! write. Due means their interval has passed and either end has moved, or it has gone stale.
	protected float TraceMuffle(notnull LC_MuffleSample sample, notnull IEntity speaker, vector speakerPosition, IEntity listener, vector listenerPosition, bool talking, int now)
	{
		int interval = OCCLUSION_SILENT_MS;
		if (talking)
			interval = OCCLUSION_TALKING_MS;

		int age = now - sample.m_iTracedTick;
		if (age < interval || m_iTraceBudget <= 0)
			return sample.m_fMuffle;

		float moveSq = OCCLUSION_MOVE_M * OCCLUSION_MOVE_M;
		bool moved = vector.DistanceSq(sample.m_vListener, listenerPosition) > moveSq || vector.DistanceSq(sample.m_vSpeaker, speakerPosition) > moveSq;
		if (!moved && age < OCCLUSION_STILL_MS)
			return sample.m_fMuffle;

		m_iTraceBudget--;
		m_iDiagTraces++;
		sample.m_fMuffle = m_Occlusion.Compute(listener, listenerPosition, speaker, speakerPosition);
		if (m_bRoomDiagnostics)
			sample.m_sCover = m_Occlusion.DescribeCover(m_bRoomLineDue);
		sample.m_vListener = listenerPosition;
		sample.m_vSpeaker = speakerPosition;
		sample.m_iTracedTick = now;
		return sample.m_fMuffle;
	}

	//------------------------------------------------------------------------------------------------
	//! How much of our room this player's voice fills, for the plugin's reverb send
	protected float GetRoomShare(int playerId, float muffle)
	{
		LC_MuffleSample sample = m_mMuffle.Get(playerId);
		if (!sample)
			return 0;

		return m_Rooms.GetRoomShare(m_ListenerRoom, sample.m_Room, muffle, !sample.m_bFromRooms);
	}

	//------------------------------------------------------------------------------------------------
	//! One nearby player's room and where their muffle came from: R for the room model, T for a trace
	protected void AppendRoomDiagnostic(int playerId, float muffle)
	{
		LC_MuffleSample sample = m_mMuffle.Get(playerId);
		if (!sample || m_iRoomsResolved + m_iRoomsTraced > DIAGNOSTIC_MAX_PLAYERS)
			return;

		string source = "T";
		if (sample.m_bFromRooms)
			source = "R";

		if (!m_sRoomDiagnostic.IsEmpty())
			m_sRoomDiagnostic += ", ";

		m_sRoomDiagnostic += playerId.ToString() + " " + m_Rooms.Describe(sample.m_Room) + " " + source + " " + muffle.ToString(-1, 2);
		if (!sample.m_sCover.IsEmpty())
			m_sRoomDiagnostic += " [" + sample.m_sCover + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! Where the listener is, what is known about that building, how open its doorways are, and how each
	//! nearby player's muffle was decided. One line a second while the server's room diagnostic is on.
	protected void LogRoomDiagnostic(int now, bool enabled)
	{
		if (!enabled)
		{
			m_iNextRoomDiagnosticTick = 0;
			return;
		}

		if (now < m_iNextRoomDiagnosticTick)
			return;

		m_iNextRoomDiagnosticTick = now + DIAGNOSTIC_INTERVAL_MS;

		string line = "[LC] rooms listener " + m_Rooms.Describe(m_ListenerRoom);
		line += " | layout " + m_Rooms.DescribeLayout(m_ListenerRoom);

		int probes = m_Rooms.GetBuildSpent();
		if (probes > 0)
			line += " | probes " + probes.ToString();

		string portals = m_Rooms.DescribePortals(m_ListenerRoom);
		if (!portals.IsEmpty())
			line += " | portals " + portals;

		line += " | rooms " + m_iRoomsResolved.ToString() + " traced " + m_iRoomsTraced.ToString();
		if (!m_sRoomDiagnostic.IsEmpty())
			line += " | " + m_sRoomDiagnostic;

		// PrintFormat, because Print of a bare variable logs it as "string line = '...'"
		PrintFormat("%1", line);
	}

	//------------------------------------------------------------------------------------------------
	//! Regular diagnostics, after each write: counters for the summary, a line when the radios or the
	//! listener's building change (checked every few seconds), and the summary itself once a minute.
	//! Nothing here traces, queries or walks the players again; it reads what the write already worked out.
	protected void UpdateRegularDiagnostics(int now, notnull LC_Client client, string radios)
	{
		int nearby = m_iRoomsResolved + m_iRoomsTraced;
		m_iDiagWrites++;
		m_iDiagNearbySum += nearby;
		if (nearby > m_iDiagNearbyMax)
			m_iDiagNearbyMax = nearby;

		m_iDiagFromRooms += m_iRoomsResolved;
		m_iDiagFromTraces += m_iRoomsTraced;
		m_iDiagProbes += m_Rooms.GetBuildSpent();

		if (now >= m_iNextChangeCheckTick)
		{
			m_iNextChangeCheckTick = now + CHANGE_CHECK_MS;
			if (radios != m_sDiagRadios)
			{
				m_sDiagRadios = radios;
				PrintFormat("[LC] radios %1", radios);
			}

			int area = -1;
			if (m_ListenerRoom.IsIndoors())
				area = m_ListenerRoom.m_iArea;

			if (m_ListenerRoom.m_Building != m_DiagBuilding || area != m_iDiagArea)
			{
				// A new building gets its layout too; a new room in the same one only says where
				string line = "[LC] rooms listener " + m_Rooms.Describe(m_ListenerRoom);
				if (m_ListenerRoom.m_Building != m_DiagBuilding)
					line += " | layout " + m_Rooms.DescribeLayout(m_ListenerRoom);

				m_DiagBuilding = m_ListenerRoom.m_Building;
				m_iDiagArea = area;
				PrintFormat("%1", line);
			}
		}

		if (m_iNextSummaryTick == 0)
			m_iNextSummaryTick = now + SUMMARY_INTERVAL_MS;

		if (now < m_iNextSummaryTick)
			return;

		m_iNextSummaryTick = now + SUMMARY_INTERVAL_MS;
		LogSummary(client);
	}

	//------------------------------------------------------------------------------------------------
	//! One line a minute: how busy the bridge was, what decided the muffling, and what the plugin did
	protected void LogSummary(notnull LC_Client client)
	{
		int states;
		int receptions;
		client.GetPluginState().TakeStats(states, receptions);

		int transmissions;
		int transmitMs;
		client.TakeTransmitStats(transmissions, transmitMs);

		float nearbyAverage = m_iDiagNearbySum;
		if (m_iDiagWrites > 0)
			nearbyAverage /= m_iDiagWrites;

		int seconds = SUMMARY_INTERVAL_MS / 1000;
		string line = "[LC] summary " + seconds.ToString() + " s: writes " + m_iDiagWrites.ToString();
		line += ", plugin states " + states.ToString();
		line += ", nearby avg " + nearbyAverage.ToString(-1, 1) + " max " + m_iDiagNearbyMax.ToString();
		line += ", muffle by rooms " + m_iDiagFromRooms.ToString() + " by trace " + m_iDiagFromTraces.ToString();
		line += ", traces " + m_iDiagTraces.ToString() + ", room probes " + m_iDiagProbes.ToString();
		line += ", tx " + transmissions.ToString() + " (" + transmitMs.ToString() + " ms), rx " + receptions.ToString();
		line += ", peers " + client.GetPluginState().GetPeers().ToString();
		line += " | listener " + m_Rooms.Describe(m_ListenerRoom) + " | layout " + m_Rooms.DescribeLayout(m_ListenerRoom);
		PrintFormat("%1", line);

		m_iDiagWrites = 0;
		m_iDiagNearbySum = 0;
		m_iDiagNearbyMax = 0;
		m_iDiagFromRooms = 0;
		m_iDiagFromTraces = 0;
		m_iDiagTraces = 0;
		m_iDiagProbes = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached results of players who left, died out of range or were streamed out
	protected void ForgetMuffles(int now)
	{
		m_aForget.Clear();
		foreach (int playerId, LC_MuffleSample sample : m_mMuffle)
		{
			if (now - sample.m_iSeenTick > OCCLUSION_FORGET_MS)
				m_aForget.Insert(playerId);
		}

		foreach (int playerId : m_aForget)
		{
			m_mMuffle.Remove(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static vector GetSpeakerPosition(notnull IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (character)
			return character.EyePosition();

		return entity.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this player hears anything at all. A dead body hears nothing; an unconscious one still does.
	//! A Game Master has no body to be alive or dead, so they listen through the camera instead.
	protected static bool IsListening(IEntity localEntity)
	{
		if (LC_Life.CanHear(localEntity))
			return true;

		// A character that is not alive is dead, whatever the camera is doing
		if (ChimeraCharacter.Cast(localEntity))
			return false;

		return IsEditorOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! True while this player is looking through the Game Master camera rather than out of a character
	protected static bool IsEditorOpen()
	{
		SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
		if (!editorManager)
			return false;

		return editorManager.IsOpened();
	}

	//------------------------------------------------------------------------------------------------
	//! The plugin tolerates reading a half-written file: it re-reads until the JSON parses
	protected void WriteFile(string json)
	{
		FileHandle file = FileIO.OpenFile(PATH, FileMode.WRITE);
		if (!file)
			return;

		file.WriteLine(json);
		file.Close();
	}
}
