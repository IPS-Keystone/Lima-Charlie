//------------------------------------------------------------------------------------------------
//! The last occlusion result for one nearby player, and where both ends were when it was traced
class LC_MuffleSample : Managed
{
	float m_fMuffle;
	//! Where both bodies stood at the last trace. Bodies, not heads: turning the head or the camera moves the
	//! eye but must not count as moving.
	vector m_vListener;
	vector m_vSpeaker;
	int m_iTracedTick;
	int m_iSeenTick;
	//! Talking last write, so the start of speech is traced straight away
	bool m_bWasTalking;
	//! The cached muffle must be traced again before it is trusted: speech started, the player is new, or a
	//! shortcut (same room, far outdoors) stood in for a trace
	bool m_bRetrace = true;
	//! The engine's own interior estimate at their position, 0 outside .. 1 inside; -1 until one arrives
	float m_fEngineInterior = -1;
	vector m_vEnginePosition;
	int m_iEngineTick;
	//! The request on its way, weakly: the writer owns every pending request
	LC_InteriorRequest m_InteriorRequest;
	//! Where this player was standing in the engine's room model when last checked
	ref LC_RoomLocation m_Room = new LC_RoomLocation();
	//! Whether the room model answered for this player, rather than a trace, for the diagnostic line
	bool m_bFromRooms;
	//! What the last trace for this player hit, for the diagnostic line
	string m_sCover;
	//! Within direct speech range when last looked at
	bool m_bNearby;
	//! Out of range or not streamed in: not looked at again before this tick
	int m_iNextCheckTick;
	//! This player's entry in the document as last built, when that was, and the muffle it carries. A silent
	//! player's entry is written again as it stands until it is due, instead of being worked out every write.
	string m_sJson;
	int m_iJsonTick;
	float m_fWritten;
}

//------------------------------------------------------------------------------------------------
//! One pending SoundWorld.CalculateInterirorAt for a nearby player: the engine's interior estimate, the same
//! one vanilla's sounds and VON muffle by, worked out off the script thread. The writer keeps every one alive
//! until it answers, never freeing one still pending (vanilla's sound manager does not either), and it holds the
//! sample weakly, so a player forgotten meanwhile just drops the answer.
class LC_InteriorRequest : InteriorRequestCallback
{
	protected LC_MuffleSample m_Sample;
	protected vector m_vPosition;
	bool m_bDone;

	//------------------------------------------------------------------------------------------------
	void LC_InteriorRequest(LC_MuffleSample sample, vector position)
	{
		m_Sample = sample;
		m_vPosition = position;
	}

	//------------------------------------------------------------------------------------------------
	override void OnCompleted(float interior, float roomSize)
	{
		m_bDone = true;
		LC_MuffleSample sample = m_Sample;
		m_Sample = null;
		if (!sample)
			return;

		sample.m_fEngineInterior = interior;
		sample.m_vEnginePosition = m_vPosition;
		sample.m_iEngineTick = System.GetTickCount();
		sample.m_InteriorRequest = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Writes $profile:LimaCharlie/game_state.json for the TeamSpeak plugin. Format is documented in the
//! plugin's lc_game_state.h; bump PROTOCOL_VERSION on both sides for incompatible changes.
class LC_GameStateWriter
{
	protected static const string DIRECTORY = "$profile:LimaCharlie";
	protected static const string PATH = "$profile:LimaCharlie/game_state.json";
	protected static const int PROTOCOL_VERSION = 9;
	//! 20 Hz while anyone's voice is live; transmit, radio and terrain changes are written immediately
	protected static const int INTERVAL_MS = 50;
	//! 10 Hz when nobody is talking and we are not transmitting: positions still move, but nothing is audible
	protected static const int IDLE_INTERVAL_MS = 100;
	//! 2 Hz with no plugin reading any of it - TeamSpeak not running, or not yet started. Still often enough
	//! for the plugin to find a fresh state within half a second of starting up.
	protected static const int NO_PLUGIN_INTERVAL_MS = 500;
	//! Players beyond this distance cannot matter for direct speech
	protected static const float NEARBY_RANGE_M = 60;
	//! Occlusion traces are the expensive part, and only a voice someone can hear needs one. A talking player is
	//! re-traced often while either body moves and once a second while both stand still; the start of speech
	//! is traced straight away.
	protected static const int OCCLUSION_TALKING_MS = 50;
	protected static const int OCCLUSION_STILL_MS = 1000;
	//! Silent players are only kept roughly current, so speech does not start from a result minutes old: one
	//! at a time, only while nobody at all is talking, and no more often than this each
	protected static const int OCCLUSION_SILENT_MS = 2000;
	protected static const int OCCLUSION_SILENT_STILL_MS = 10000;
	//! How far a body (not a head) moves before its result is due again
	protected static const float OCCLUSION_MOVE_M = 0.5;
	//! Or this share of the distance between the two, if that is more: half a metre matters beside someone and
	//! changes nothing at 50 m, where it takes 2.5 m. A still pair is traced again on the still interval as before.
	protected static const float OCCLUSION_MOVE_SHARE = 0.05;
	//! Traces in one write. Writes are at most one a frame, so this is also the most in any frame, and a crowd
	//! arriving at once is spread over several frames instead of landing in one.
	protected static const int OCCLUSION_BUDGET = 3;
	//! No more traces in a write once tracing has taken this long (the clock is whole milliseconds)
	protected static const int OCCLUSION_TIME_MS = 2;
	//! Two people both outdoors and further apart than this are clear without a trace, the way vanilla's VON
	//! treats everyone outdoors: a long trace through woodland is the costliest kind, and a wall at that range
	//! changes little in a voice already that quiet. Only when LC's room model and the engine's interior
	//! estimate both put both ends outdoors and neither is in a vehicle; anything unsure is traced as before.
	protected static const float OUTDOOR_TRACE_RANGE_M = 30;
	//! Engine interior below this counts as outdoors; vanilla's occlusion graph only starts muffling above it
	protected static const float ENGINE_OUTDOORS_BELOW = 0.5;
	protected static const int ENGINE_INTERIOR_REFRESH_MS = 2000;
	protected static const float ENGINE_INTERIOR_MOVE_M = 1;
	//! Vanilla's global audio variables, where the engine keeps the camera's own interior estimate
	protected static const string GLOBAL_AUDIO_VARIABLES = "{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf";
	//! A player gone from the server for this long has their cached state dropped, looked for this often
	protected static const int OCCLUSION_FORGET_MS = 5000;
	protected static const int FORGET_CHECK_MS = 1000;
	//! A silent player nearby is only worked out this often (position, life state, room, entry text); in between
	//! their last entry is written again. Nobody hears them, and the plugin only needs to know roughly where
	//! they are for the moment they speak. Speech starting is written at once with everything fresh.
	protected static const int SILENT_REFRESH_MS = 250;
	//! Someone out of range is not looked at again until they could have closed the gap at this speed (metres
	//! a second), and never later than the cap. Someone not streamed in is looked for this often.
	protected static const float FAR_CLOSING_SPEED = 60;
	protected static const int FAR_RECHECK_MAX_MS = 2000;
	protected static const int ABSENT_RECHECK_MS = 500;
	//! The listener moving this far between two writes (respawn, teleport, a fast free camera) makes every
	//! player due at once, since the waits above assume the listener moves like a person
	protected static const float LISTENER_JUMP_M = 10;
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
	protected int m_iTraceStartTick;
	//! Players are visited from a different one each write, so a budget spent early cannot starve the rest
	protected int m_iPlayerRotation;
	//! Where the listener's body is, for the move test, and whether the engine places the listener outdoors
	protected vector m_vListenerAnchor;
	protected bool m_bListenerOutdoors;
	//! Someone nearby was talking on the write before this one
	protected bool m_bAnyoneTalking;
	//! Someone within direct speech range was talking on the last write: the only voices positions matter for
	protected bool m_bNearbyTalking;
	protected int m_iLastTalkingRevision;
	protected vector m_vLastListenerPosition;
	protected int m_iNextForgetTick;
	//! Every interior request still on its way, kept alive here until the engine answers
	protected static ref array<ref LC_InteriorRequest> s_aInteriorRequests = {};
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
	protected IEntity m_DiagBuilding;
	protected int m_iDiagArea = -1;
	protected int m_iDiagWrites;
	protected int m_iDiagNearbySum;
	protected int m_iDiagNearbyMax;
	protected int m_iDiagFromRooms;
	protected int m_iDiagFromTraces;
	protected int m_iDiagTraces;
	protected int m_iDiagFarOutdoors;
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

		// Someone nearby starting to speak is written at once as well: their entry may be a quarter second old
		// and their muffle older, and the plugin is already playing their voice with it
		LC_PluginStateReader talkReader = client.GetPluginState();
		int talkingRevision = talkReader.GetTalkingRevision();
		if (talkingRevision != m_iLastTalkingRevision)
		{
			m_iLastTalkingRevision = talkingRevision;
			if (NearbyTalkStarted(talkReader))
				changed = true;
		}

		if (!changed && now < m_iNextWriteTick)
			return;

		// A Game Master talks to the whole map from the camera, if the server allows it
		bool unlimitedRange = client.GetGameMasterUnlimitedRange() && IsEditorOpen();
		string radios = LC_Radio.BuildJson(client.GetRadioEntries(), client.GetRadioSettings(), unlimitedRange);

		m_eLastTransmitType = transmitType;
		m_fLastVoiceRange = voiceRange;
		m_sLastTransmitRadio = transmitRadio;
		m_iLastLinkRevision = linkRevision;
		m_iLastSoundSeq = soundSeq;
		m_sRoomDiagnostic = string.Empty;
		m_bRoomDiagnostics = client.GetRoomDiagnostics();
		m_bRoomLineDue = m_bRoomDiagnostics && now >= m_iNextRoomDiagnosticTick;
		string json = BuildInGameJson(client, transmitType, voiceRange, transmitRadio, transmitFrequency, radios, now, unlimitedRange);
		// After the build, which is what finds out whether anyone nearby is talking
		m_iNextWriteTick = now + GetWriteInterval(client, transmitType);
		WriteFile(json);
		LogDiagnostic(now, json, client.IsComprehensiveDiagnostics());
		LogRoomDiagnostic(now, m_bRoomDiagnostics);
		if (client.IsRegularDiagnostics())
			UpdateRegularDiagnostics(now, client, radios);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the plugin reports someone talking who is nearby and was silent on the last write
	protected bool NearbyTalkStarted(notnull LC_PluginStateReader reader)
	{
		foreach (int playerId : reader.GetTalkingPlayers())
		{
			LC_MuffleSample sample = m_mMuffle.Get(playerId);
			if (sample && sample.m_bNearby && !sample.m_bWasTalking)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Voice only needs fresh positions while someone can be heard directly. A radio voice has no position, and
	//! with a full server somebody is on a radio somewhere nearly all the time, which used to hold every client
	//! at the fast rate for the whole mission.
	protected int GetWriteInterval(notnull LC_Client client, EVONTransmitType transmitType)
	{
		LC_PluginStateReader reader = client.GetPluginState();

		// Nobody is reading this. Somebody playing without TeamSpeak running should not pay for the bridge.
		if (!reader.IsPluginRunning(System.GetTickCount()))
			return NO_PLUGIN_INTERVAL_MS;

		if (transmitType != EVONTransmitType.NONE)
			return INTERVAL_MS;

		if (reader.IsSelfTalking() || m_bNearbyTalking)
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
	protected string BuildInGameJson(notnull LC_Client client, EVONTransmitType transmitType, float voiceRange, string transmitRadio, int transmitFrequency, string radios, int now, bool unlimitedRange)
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

		// The listener's body for the move test, so turning the head or the camera does not count as moving. A
		// free camera has no body: it is its own anchor, and turning it does not move it.
		m_vListenerAnchor = occlusionOrigin;
		m_bListenerOutdoors = AudioSystem.GetVariableValue("GInterior", GLOBAL_AUDIO_VARIABLES) < ENGINE_OUTDOORS_BELOW;
		if (occlusionListener)
		{
			m_vListenerAnchor = occlusionListener.GetOrigin();
			if (CompartmentAccessComponent.GetVehicleIn(occlusionListener))
				m_bListenerOutdoors = false;
		}

		// Which room the listener is in, and a slice of the work of mapping that building's type
		m_Rooms.BeginWrite();
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
		json += ",\"radios\":" + radios;
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
		json += ",\"sounds\":" + client.GetSoundQueue().BuildJson() + "}";

		json += ",\"players\":[";
		m_aPlayerIds.Clear();
		playerManager.GetPlayers(m_aPlayerIds);
		LC_PluginStateReader reader = client.GetPluginState();
		float maxDistanceSq = NEARBY_RANGE_M * NEARBY_RANGE_M;
		m_iTraceBudget = OCCLUSION_BUDGET;
		// As of the last write: a voice only competes with the silent players' upkeep when it is one we can hear.
		// Any talker anywhere used to count, and on a full server that left silent players never traced at all.
		m_bAnyoneTalking = m_bNearbyTalking;
		bool first = true;
		m_iRoomsResolved = 0;
		m_iRoomsTraced = 0;
		int playerCount = m_aPlayerIds.Count();
		m_iPlayerRotation++;
		if (m_iPlayerRotation >= playerCount)
			m_iPlayerRotation = 0;

		// Everyone is due after the listener jumps, since the waits below assume a listener moving like a person
		float jumpSq = LISTENER_JUMP_M * LISTENER_JUMP_M;
		bool listenerJumped = vector.DistanceSq(listenerPosition, m_vLastListenerPosition) > jumpSq;
		m_vLastListenerPosition = listenerPosition;
		m_bNearbyTalking = false;

		for (int n = 0; n < playerCount; n++)
		{
			int playerId = m_aPlayerIds[(n + m_iPlayerRotation) % playerCount];
			if (playerId == localPlayerId)
				continue;

			LC_MuffleSample sample = m_mMuffle.Get(playerId);
			if (!sample)
			{
				sample = new LC_MuffleSample();
				// Spread over the refresh interval, so a crowd met at once does not come due together ever after
				int spread = (playerId * 37) % SILENT_REFRESH_MS;
				sample.m_iJsonTick = now - spread;
				m_mMuffle.Set(playerId, sample);
			}

			sample.m_iSeenTick = now;
			bool talking = reader.IsPlayerTalking(playerId);
			if (!listenerJumped)
			{
				// Out of range a moment ago and not yet able to be back in it: nothing to look up at all
				if (!sample.m_bNearby && now < sample.m_iNextCheckTick)
					continue;

				// Silent, nearby and worked out recently: the same entry again, without touching the entity
				if (sample.m_bNearby && !talking && !sample.m_bWasTalking && now - sample.m_iJsonTick < SILENT_REFRESH_MS)
				{
					CountNearby(sample);
					if (m_bRoomLineDue)
						AppendRoomDiagnostic(playerId, sample, sample.m_fWritten);

					if (first)
						json += sample.m_sJson;
					else
						json += "," + sample.m_sJson;

					first = false;
					continue;
				}
			}

			// Only players whose character is streamed in on this client
			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			if (!entity)
			{
				SetFar(sample, now + ABSENT_RECHECK_MS);
				continue;
			}

			vector position = GetSpeakerPosition(entity);
			float distanceSq = vector.DistanceSq(position, listenerPosition);
			if (distanceSq > maxDistanceSq)
			{
				float waitMs = (Math.Sqrt(distanceSq) - NEARBY_RANGE_M) / FAR_CLOSING_SPEED * 1000;
				int wait = Math.Min(waitMs, FAR_RECHECK_MAX_MS);
				SetFar(sample, now + wait);
				continue;
			}

			sample.m_bNearby = true;
			if (talking)
				m_bNearbyTalking = true;

			float muffle = GetMuffle(sample, entity, position, occlusionListener, occlusionOrigin, distanceSq, talking, now);
			CountNearby(sample);
			// Built only for the write that prints it: the line is once a second, writes are up to 20
			if (m_bRoomLineDue)
				AppendRoomDiagnostic(playerId, sample, muffle);

			// Built on its own and appended once: every append copies the whole document so far, which with 60
			// players nearby made this loop 30% slower
			string player = "{\"id\":" + playerId.ToString();
			player += ",\"alive\":" + LC_Json.Bool(LC_Life.CanSpeak(entity));
			player += ",\"pos\":" + LC_Json.Position(position);
			player += ",\"muffle\":" + muffle.ToString(-1, 2);
			player += ",\"room\":" + GetRoomShare(sample, muffle).ToString(-1, 2) + "}";
			sample.m_sJson = player;
			sample.m_iJsonTick = now;
			sample.m_fWritten = muffle;
			if (first)
				json += player;
			else
				json += "," + player;

			first = false;
		}

		json += "],\"links\":" + client.GetRadioLinks().BuildJson() + "}";
		ForgetMuffles(now);
		DropAnsweredRequests();
		return json;
	}

	//------------------------------------------------------------------------------------------------
	//! Out of range, or not on this client at all: nothing about them is current any more
	protected void SetFar(notnull LC_MuffleSample sample, int nextCheckTick)
	{
		sample.m_bNearby = false;
		sample.m_iNextCheckTick = nextCheckTick;
		// Back in range and already talking counts as speech starting, and nothing cached is trusted
		sample.m_bWasTalking = false;
		sample.m_bRetrace = true;
		sample.m_Room.m_iCheckedTick = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! One more nearby player on this write, by what decided their muffle
	protected void CountNearby(notnull LC_MuffleSample sample)
	{
		if (sample.m_bFromRooms)
			m_iRoomsResolved++;
		else
			m_iRoomsTraced++;
	}

	//------------------------------------------------------------------------------------------------
	//! How muffled a nearby player is. Both the engine's room model and a trace get a say, and the clearer
	//! of the two wins: a path through the rooms cannot tell whether the two can see each other, and a trace
	//! cannot tell that an open door two rooms away carries a voice. Only the same room needs neither, being
	//! clear already. One indoors and one outdoors is always traced, so a doorway they can see each other
	//! through stays clear.
	protected float GetMuffle(notnull LC_MuffleSample sample, notnull IEntity speaker, vector speakerPosition, IEntity listener, vector listenerPosition, float distanceSq, bool talking, int now)
	{
		// Whatever was cached while they were silent is traced again the moment they speak
		if (talking && !sample.m_bWasTalking)
			sample.m_bRetrace = true;

		bool wasTalking = sample.m_bWasTalking;
		sample.m_bWasTalking = talking;

		// A room found while they were silent may be seconds old, so the start of speech looks again at once
		if (talking && !wasTalking)
			sample.m_Room.m_iCheckedTick = 0;

		m_Rooms.Locate(sample.m_Room, speakerPosition, now, talking);
		float roomMuffle;
		if (m_Rooms.GetMuffle(m_ListenerRoom, sample.m_Room, distanceSq, now, roomMuffle))
		{
			sample.m_bFromRooms = true;

			// Same room, or one open doorway apart: nothing a trace finds can improve on clear, and a wall
			// beside the doorway, a pillar or a crate between them must not make it worse
			if (roomMuffle <= 0)
			{
				// Any trace result is now stale: a later fallback has to trace again rather than reuse it
				sample.m_bRetrace = true;
				return 0;
			}

			// Otherwise the path is only an upper bound: a trace may find they can see each other
			return Math.Min(roomMuffle, TraceMuffle(sample, speaker, speakerPosition, listener, listenerPosition, talking, now));
		}

		sample.m_bFromRooms = false;
		if (IsFarOutdoors(sample, speaker, speakerPosition, distanceSq, talking, now))
		{
			sample.m_bRetrace = true;
			sample.m_fMuffle = 0;
			m_iDiagFarOutdoors++;
			if (m_bRoomDiagnostics)
				sample.m_sCover = "far outdoors";

			return 0;
		}

		return TraceMuffle(sample, speaker, speakerPosition, listener, listenerPosition, talking, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Both outdoors, far enough apart to skip the trace, and the engine agrees neither is inside anything.
	//! Asks the engine for a fresh interior estimate at the speaker as a side effect; until one has come back,
	//! the answer is no and the pair is traced.
	protected bool IsFarOutdoors(notnull LC_MuffleSample sample, notnull IEntity speaker, vector speakerPosition, float distanceSq, bool talking, int now)
	{
		if (!talking || !m_bListenerOutdoors || distanceSq <= OUTDOOR_TRACE_RANGE_M * OUTDOOR_TRACE_RANGE_M)
			return false;

		if (m_ListenerRoom.IsIndoors() || sample.m_Room.IsIndoors())
			return false;

		// A hull is not outdoors, whatever the ground around it is
		if (CompartmentAccessComponent.GetVehicleIn(speaker))
			return false;

		RequestEngineInterior(sample, speakerPosition, now);
		if (sample.m_fEngineInterior < 0 || sample.m_fEngineInterior >= ENGINE_OUTDOORS_BELOW)
			return false;

		// An estimate from somewhere else is no answer, even while a fresh one is on its way
		float nearM = 2 * ENGINE_INTERIOR_MOVE_M;
		return vector.DistanceSq(sample.m_vEnginePosition, speakerPosition) <= nearM * nearM;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the engine where this player stands, unless an answer is pending or still fresh for that spot
	protected void RequestEngineInterior(notnull LC_MuffleSample sample, vector position, int now)
	{
		if (sample.m_InteriorRequest)
			return;

		bool known = sample.m_fEngineInterior >= 0;
		bool moved = vector.DistanceSq(sample.m_vEnginePosition, position) > ENGINE_INTERIOR_MOVE_M * ENGINE_INTERIOR_MOVE_M;
		if (known && !moved && now - sample.m_iEngineTick < ENGINE_INTERIOR_REFRESH_MS)
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return;

		SoundWorld soundWorld = world.GetSoundWorld();
		if (!soundWorld)
			return;

		LC_InteriorRequest request = new LC_InteriorRequest(sample, position);
		s_aInteriorRequests.Insert(request);
		sample.m_InteriorRequest = request;
		soundWorld.CalculateInterirorAt(position, request);
	}

	//------------------------------------------------------------------------------------------------
	//! Lets go of the interior requests the engine has answered
	protected static void DropAnsweredRequests()
	{
		for (int i = s_aInteriorRequests.Count() - 1; i >= 0; i--)
		{
			if (s_aInteriorRequests[i].m_bDone)
				s_aInteriorRequests.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The traced muffle for this player, from the cache unless it is due and there is budget left in this
	//! write. A talking player is due when their interval has passed and either body has moved, when it has
	//! gone stale, or straight away when they start. A silent one is only refreshed while nobody is talking.
	protected float TraceMuffle(notnull LC_MuffleSample sample, notnull IEntity speaker, vector speakerPosition, IEntity listener, vector listenerPosition, bool talking, int now)
	{
		if (m_iTraceBudget <= 0)
			return sample.m_fMuffle;

		int interval = OCCLUSION_TALKING_MS;
		int stillInterval = OCCLUSION_STILL_MS;
		if (!talking)
		{
			// Nobody hears a silent player, so any live voice goes first, and only one silent player a write
			if (m_bAnyoneTalking || m_iTraceBudget < OCCLUSION_BUDGET)
				return sample.m_fMuffle;

			interval = OCCLUSION_SILENT_MS;
			stillInterval = OCCLUSION_SILENT_STILL_MS;
		}

		vector speakerAnchor = speaker.GetOrigin();
		int age = now - sample.m_iTracedTick;
		bool startOfSpeech = talking && sample.m_bRetrace;
		if (!startOfSpeech)
		{
			if (age < interval)
				return sample.m_fMuffle;

			// The further apart, the further either has to move before the line between them is a different one
			float moveSq = Math.Max(OCCLUSION_MOVE_M * OCCLUSION_MOVE_M, vector.DistanceSq(listenerPosition, speakerPosition) * OCCLUSION_MOVE_SHARE * OCCLUSION_MOVE_SHARE);
			bool moved = vector.DistanceSq(sample.m_vListener, m_vListenerAnchor) > moveSq || vector.DistanceSq(sample.m_vSpeaker, speakerAnchor) > moveSq;
			if (!moved && !sample.m_bRetrace && age < stillInterval)
				return sample.m_fMuffle;
		}

		// A write whose traces have already taken their time leaves the rest for the next frame
		int tick = System.GetTickCount();
		if (m_iTraceBudget == OCCLUSION_BUDGET)
			m_iTraceStartTick = tick;
		else if (tick - m_iTraceStartTick >= OCCLUSION_TIME_MS)
			return sample.m_fMuffle;

		m_iTraceBudget--;
		m_iDiagTraces++;
		sample.m_fMuffle = m_Occlusion.Compute(listener, listenerPosition, speaker, speakerPosition);
		if (m_bRoomDiagnostics)
			sample.m_sCover = m_Occlusion.DescribeCover(m_bRoomLineDue);
		sample.m_vListener = m_vListenerAnchor;
		sample.m_vSpeaker = speakerAnchor;
		sample.m_iTracedTick = now;
		sample.m_bRetrace = false;
		return sample.m_fMuffle;
	}

	//------------------------------------------------------------------------------------------------
	//! How much of our room this player's voice fills, for the plugin's reverb send
	protected float GetRoomShare(notnull LC_MuffleSample sample, float muffle)
	{
		return m_Rooms.GetRoomShare(m_ListenerRoom, sample.m_Room, muffle, !sample.m_bFromRooms);
	}

	//------------------------------------------------------------------------------------------------
	//! One nearby player's room and where their muffle came from: R for the room model, T for a trace
	protected void AppendRoomDiagnostic(int playerId, notnull LC_MuffleSample sample, float muffle)
	{
		if (m_iRoomsResolved + m_iRoomsTraced > DIAGNOSTIC_MAX_PLAYERS)
			return;

		string source = "T";
		if (sample.m_bFromRooms)
			source = "R";

		if (!m_sRoomDiagnostic.IsEmpty())
			m_sRoomDiagnostic += ", ";

		m_sRoomDiagnostic += playerId.ToString() + " " + m_Rooms.Describe(sample.m_Room) + " " + source + " " + muffle.ToString(-1, 2);
		if (!sample.m_sCover.IsEmpty())
			m_sRoomDiagnostic += " [" + sample.m_sCover + "]";

		// The engine's interior estimate, when one was asked for (both outdoors, talking, beyond the trace range)
		if (sample.m_fEngineInterior >= 0)
			m_sRoomDiagnostic += " e" + sample.m_fEngineInterior.ToString(-1, 2);
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
		line += ", traces " + m_iDiagTraces.ToString() + ", far outdoors " + m_iDiagFarOutdoors.ToString() + ", room probes " + m_iDiagProbes.ToString();
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
		m_iDiagFarOutdoors = 0;
		m_iDiagProbes = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached state of players who have left the server. Everyone still on it is seen every write.
	protected void ForgetMuffles(int now)
	{
		if (now < m_iNextForgetTick)
			return;

		m_iNextForgetTick = now + FORGET_CHECK_MS;
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
