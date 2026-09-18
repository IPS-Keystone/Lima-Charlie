//------------------------------------------------------------------------------------------------
//! Writes $profile:LimaCharlie/game_state.json for the TeamSpeak plugin. Format is documented in the
//! plugin's lc_game_state.h; bump PROTOCOL_VERSION on both sides for incompatible changes.
class LC_GameStateWriter
{
	protected static const string DIRECTORY = "$profile:LimaCharlie";
	protected static const string PATH = "$profile:LimaCharlie/game_state.json";
	protected static const int PROTOCOL_VERSION = 6;
	//! 20 Hz while anyone's voice is live; transmit, radio and terrain changes are written immediately
	protected static const int INTERVAL_MS = 50;
	//! 10 Hz when nobody is talking and we are not transmitting: positions still move, but nothing is audible
	protected static const int IDLE_INTERVAL_MS = 100;
	//! Players beyond this distance cannot matter for direct speech
	protected static const float NEARBY_RANGE_M = 60;
	//! Occlusion traces are the expensive part, so they run at 10 Hz and only within the longest voice range
	protected static const int OCCLUSION_INTERVAL_MS = 100;
	protected static const float OCCLUSION_RANGE_M = 60;
	//! A camera further than this from the body it belongs to is a free camera: Game Master, spectator or photo
	//! mode. Character cameras, including a vehicle's third person boom, stay well inside it.
	protected static const float FREE_CAMERA_RANGE_M = 20;
	//! How often the whole state goes to the log while a Game Master has the editor open
	protected static const int DIAGNOSTIC_INTERVAL_MS = 1000;

	protected int m_iSeq;
	protected int m_iNextWriteTick;
	protected int m_iNextOcclusionTick;
	protected int m_iNextDiagnosticTick;
	protected EVONTransmitType m_eLastTransmitType = EVONTransmitType.NONE;
	protected float m_fLastVoiceRange;
	protected string m_sLastTransmitRadio;
	protected string m_sLastRadios;
	protected int m_iLastLinkRevision;
	protected int m_iLastSoundSeq;
	protected ref array<int> m_aPlayerIds = {};
	protected ref map<int, float> m_mMuffle = new map<int, float>();
	protected ref LC_Occlusion m_Occlusion = new LC_Occlusion();

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
			transmitRadio = LC_Radio.GetId(transmitEntry);
			transmitFrequency = transmitEntry.GetTransceiver().GetFrequency();
		}

		// A Game Master talks to the whole map from the camera, if the server allows it
		bool unlimitedRange = client.GetGameMasterUnlimitedRange() && IsEditorOpen();
		string radios = LC_Radio.BuildJson(client.GetRadioEntries(), client.GetRadioSettings(), unlimitedRange);
		int linkRevision = client.GetRadioLinks().GetRevision();
		int soundSeq = client.GetSoundQueue().GetSeq();

		bool changed = transmitType != m_eLastTransmitType || voiceRange != m_fLastVoiceRange || transmitRadio != m_sLastTransmitRadio
			|| radios != m_sLastRadios || linkRevision != m_iLastLinkRevision || soundSeq != m_iLastSoundSeq;
		if (!changed && now < m_iNextWriteTick)
			return;

		m_eLastTransmitType = transmitType;
		m_fLastVoiceRange = voiceRange;
		m_sLastTransmitRadio = transmitRadio;
		m_sLastRadios = radios;
		m_iLastLinkRevision = linkRevision;
		m_iLastSoundSeq = soundSeq;
		m_iNextWriteTick = now + GetWriteInterval(client, transmitType);

		string json = BuildInGameJson(client, transmitType, voiceRange, transmitRadio, transmitFrequency, radios, now, unlimitedRange);
		WriteFile(json);
		LogDiagnostic(now, json, client.GetDiagnosticLog());
	}

	//------------------------------------------------------------------------------------------------
	//! Voice only needs fresh positions while someone can be heard
	protected int GetWriteInterval(notnull LC_Client client, EVONTransmitType transmitType)
	{
		if (transmitType != EVONTransmitType.NONE)
			return INTERVAL_MS;

		LC_PluginStateReader reader = client.GetPluginState();
		if (reader.IsSelfTalking() || reader.IsAnyPlayerTalking())
			return INTERVAL_MS;

		return IDLE_INTERVAL_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! The state goes to the log once a second as well as to disk, so a transmit key can be held and read back
	//! afterwards. A Game Master always gets this while the editor is open; the server's diagnostic setting
	//! turns it on for everyone, which is the only way to see it on the receiving end of a transmission.
	protected void LogDiagnostic(int now, string json, bool enabled)
	{
		if (!enabled && !IsEditorOpen())
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
		json += ",\"tsServer\":" + LC_Json.String(client.GetTeamSpeakServer());
		json += ",\"tsChannel\":" + LC_Json.String(client.GetTeamSpeakChannel());
		json += ",\"tsChannelPassword\":" + LC_Json.String(client.GetTeamSpeakChannelPassword());
		json += ",\"modVersion\":" + LC_Json.String(LC_Version.VERSION) + "}";

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
		json += ",\"sounds\":" + client.GetSoundQueue().BuildJson() + "}";

		// Occlusion is traced from the listener's head, not the camera, so third person does not hear around walls.
		// A free camera is nowhere near the body it belongs to, and that camera is where the player really listens
		// from, so it is traced from instead and the body is ignored entirely.
		bool refreshOcclusion = now >= m_iNextOcclusionTick;
		if (refreshOcclusion)
		{
			m_iNextOcclusionTick = now + OCCLUSION_INTERVAL_MS;
			m_mMuffle.Clear();
		}

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

		json += ",\"players\":[";
		m_aPlayerIds.Clear();
		playerManager.GetPlayers(m_aPlayerIds);
		float maxDistanceSq = NEARBY_RANGE_M * NEARBY_RANGE_M;
		float occlusionDistanceSq = OCCLUSION_RANGE_M * OCCLUSION_RANGE_M;
		bool first = true;
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

			float muffle;
			if (refreshOcclusion)
			{
				if (distanceSq <= occlusionDistanceSq)
					muffle = m_Occlusion.Compute(occlusionListener, occlusionOrigin, entity, position);

				m_mMuffle.Set(playerId, muffle);
			}
			else
			{
				m_mMuffle.Find(playerId, muffle);
			}

			if (!first)
				json += ",";

			first = false;
			json += "{\"id\":" + playerId.ToString();
			json += ",\"alive\":" + LC_Json.Bool(IsAlive(entity));
			json += ",\"pos\":" + LC_Json.Position(position);
			json += ",\"muffle\":" + muffle.ToString(-1, 2) + "}";
		}

		json += "],\"links\":" + client.GetRadioLinks().BuildJson() + "}";
		return json;
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
		if (IsAlive(localEntity))
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
	protected static bool IsAlive(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (!controller)
			return false;

		return controller.GetLifeState() != ECharacterLifeState.DEAD;
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
