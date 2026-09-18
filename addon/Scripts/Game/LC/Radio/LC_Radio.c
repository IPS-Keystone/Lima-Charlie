//------------------------------------------------------------------------------------------------
//! The local player's radios as the plugin sees them: one per vanilla VON radio entry (transceiver)
class LC_Radio
{
	//! Range announced for a Game Master's radios: far past any map, so distance and terrain both come out as
	//! nothing once the plugin divides by it. The radio's own range is untouched for everything else.
	protected static const float UNLIMITED_RANGE_M = 1000000;

	//------------------------------------------------------------------------------------------------
	//! Key of the physical radio a transceiver belongs to; per-radio settings are stored under it
	static string GetRadioKey(notnull SCR_VONEntryRadio entry)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		if (!transceiver || !transceiver.GetRadio())
			return "0";

		IEntity owner = transceiver.GetRadio().GetOwner();
		if (!owner)
			return "0";

		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!rpl)
			return "0";

		int id = rpl.Id();
		return id.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Id of one transceiver, unique among the local player's radios
	static string GetId(notnull SCR_VONEntryRadio entry)
	{
		return GetRadioKey(entry) + ":" + entry.GetTransceiverNumber().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Duplex mode from the radio gadget's own prefab, full duplex when the radio has no gadget component
	static LC_ERadioMode GetRadioMode(notnull SCR_VONEntryRadio entry)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		if (!transceiver || !transceiver.GetRadio())
			return LC_ERadioMode.FULL_DUPLEX;

		IEntity owner = transceiver.GetRadio().GetOwner();
		if (!owner)
			return LC_ERadioMode.FULL_DUPLEX;

		SCR_RadioComponent radio = SCR_RadioComponent.Cast(owner.FindComponent(SCR_RadioComponent));
		if (!radio)
			return LC_ERadioMode.FULL_DUPLEX;

		return radio.LC_GetRadioMode();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLongRange(notnull SCR_VONEntryRadio entry)
	{
		return entry.GetGadget() && entry.IsLongRange();
	}

	//------------------------------------------------------------------------------------------------
	//! A switched off or muted transceiver receives nothing
	static bool CanReceive(notnull SCR_VONEntryRadio entry)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		if (!transceiver)
			return false;

		BaseRadioComponent radio = transceiver.GetRadio();
		return radio && radio.IsPowered() && !transceiver.IsMuted() && !entry.GetIsMuted();
	}

	//------------------------------------------------------------------------------------------------
	static string FormatFrequency(int frequencyKHz)
	{
		float megahertz = frequencyKHz / 1000.0;
		return megahertz.ToString(-1, 3) + " MHz";
	}

	//------------------------------------------------------------------------------------------------
	//! "radios" array of game_state.json
	//! \param unlimitedRange announces every radio as having limitless reach, for a Game Master transmitting
	//! from the editor camera. Only what we transmit is affected; what we hear still goes by the real range.
	static string BuildJson(notnull array<SCR_VONEntryRadio> entries, notnull LC_RadioSettings settings, bool unlimitedRange = false)
	{
		string json = "[";
		bool first = true;
		foreach (SCR_VONEntryRadio entry : entries)
		{
			BaseTransceiver transceiver = entry.GetTransceiver();
			if (!transceiver)
				continue;

			BaseRadioComponent radio = transceiver.GetRadio();
			if (!radio)
				continue;

			if (!first)
				json += ",";

			first = false;
			json += "{\"id\":" + LC_Json.String(GetId(entry));
			json += ",\"freq\":" + transceiver.GetFrequency().ToString();
			float range = transceiver.GetRange();
			if (unlimitedRange)
				range = UNLIMITED_RANGE_M;

			json += ",\"range\":" + range.ToString(-1, 0);
			json += ",\"key\":" + LC_Json.String(radio.GetEncryptionKey());
			json += ",\"rx\":" + LC_Json.Bool(CanReceive(entry));
			json += ",\"ear\":" + settings.GetEar(entry).ToString();
			json += ",\"volume\":" + settings.GetVolume(entry).ToString(-1, 2);
			json += ",\"beep\":" + LC_Json.String(settings.GetBeepSet(entry));
			int halfDuplex = GetRadioMode(entry) == LC_ERadioMode.HALF_DUPLEX;
			json += ",\"halfDuplex\":" + halfDuplex.ToString() + "}";
		}

		return json + "]";
	}
}
