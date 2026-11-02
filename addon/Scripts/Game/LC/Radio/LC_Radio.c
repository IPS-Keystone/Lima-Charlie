//------------------------------------------------------------------------------------------------
//! Where a radio physically is, and so where its antenna is: "src" in game_state.json radios[], named as
//! LC_Radio.GetSourceName gives it. BORROWED and VEHICLE come from the local player's radio leases
//! (LC_RadioLeases); SPEAKER is named now so the bridge format does not change again when it lands.
enum LC_ERadioSource
{
	//! In the player's own inventory, not worn as the backpack
	HELD,
	//! Worn as the player's backpack (a manpack)
	PACK,
	//! Someone else's manpack, through its handset
	BORROWED,
	//! Mounted in a vehicle
	VEHICLE,
	//! A radio out loud through a speaker
	SPEAKER
}

//------------------------------------------------------------------------------------------------
//! The local player's radios as the plugin sees them: one per radio entry (transceiver), vanilla VON's plus
//! LC_Client's entries for radios held by lease
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
	//! The backpack the character is wearing, which is a manpack radio if one of its radios belongs to it
	static IEntity GetWornBackpack(IEntity character)
	{
		if (!character)
			return null;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return null;

		return loadout.GetClothFromArea(LoadoutBackpackArea);
	}

	//------------------------------------------------------------------------------------------------
	//! Where one of the local player's radios is: worn as the backpack, anywhere else on the body, or (for a
	//! radio off the body that the player holds a lease on) what the lease's kind says
	static LC_ERadioSource GetSource(notnull SCR_VONEntryRadio entry, IEntity character, IEntity wornBackpack, LC_RadioLeases leases)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		if (!transceiver || !transceiver.GetRadio())
			return LC_ERadioSource.HELD;

		IEntity owner = transceiver.GetRadio().GetOwner();
		if (wornBackpack && owner == wornBackpack)
			return LC_ERadioSource.PACK;

		if (leases && character && owner && owner.GetRootParent() != character)
		{
			int kind = leases.GetOwnKind(LC_RadioLeases.GetRadioId(owner));
			if (kind >= 0)
				return LC_RadioLeases.GetSourceOfKind(kind);
		}

		return LC_ERadioSource.HELD;
	}

	//------------------------------------------------------------------------------------------------
	//! Name of a source in the bridge, which the plugin and its RTX announcement share
	static string GetSourceName(LC_ERadioSource source)
	{
		switch (source)
		{
			case LC_ERadioSource.PACK: return "pack";
			case LC_ERadioSource.BORROWED: return "borrowed";
			case LC_ERadioSource.VEHICLE: return "vehicle";
			case LC_ERadioSource.SPEAKER: return "speaker";
		}

		return "held";
	}

	//------------------------------------------------------------------------------------------------
	//! A name the plugin passed back; one this build does not know is held
	static LC_ERadioSource ParseSource(string name)
	{
		if (name == "pack")
			return LC_ERadioSource.PACK;

		if (name == "borrowed")
			return LC_ERadioSource.BORROWED;

		if (name == "vehicle")
			return LC_ERadioSource.VEHICLE;

		if (name == "speaker")
			return LC_ERadioSource.SPEAKER;

		return LC_ERadioSource.HELD;
	}

	//------------------------------------------------------------------------------------------------
	//! Held and worn radios are on the talker, whose head is then as good as the antenna position
	static bool IsOnBody(LC_ERadioSource source)
	{
		return source == LC_ERadioSource.HELD || source == LC_ERadioSource.PACK;
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
	//! \param character the local player's controlled entity, whose worn backpack tells a manpack from the rest
	//! \param bodyPosition antenna of every radio on the body: the listener position the plugin used to announce
	//! for every transmission, so that nothing changes for a held or worn radio. A leased radio's antenna is
	//! the radio entity itself.
	//! \param unlimitedRange announces every radio as having limitless reach, for a Game Master transmitting
	//! from the editor camera. Only what we transmit is affected; what we hear still goes by the real range.
	static string BuildJson(notnull array<SCR_VONEntryRadio> entries, notnull LC_RadioSettings settings, IEntity character, vector bodyPosition, bool unlimitedRange = false)
	{
		IEntity wornBackpack = GetWornBackpack(character);
		LC_RadioLeases leases = LC_RadioLeases.Get();
		string bodyJson = LC_Json.Position(bodyPosition);
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
			json += ",\"halfDuplex\":" + halfDuplex.ToString();
			LC_ERadioSource source = GetSource(entry, character, wornBackpack, leases);
			json += ",\"src\":" + LC_Json.String(GetSourceName(source));
			if (IsOnBody(source))
				json += ",\"pos\":" + bodyJson + "}";
			else
				json += ",\"pos\":" + LC_Json.Position(LC_RadioLeaseRule.GetWorldPosition(radio.GetOwner())) + "}";
		}

		return json + "]";
	}
}
