[ComponentEditorProps(category: "Lima Charlie", description: "Lima Charlie vehicle radio. Added to the vanilla vehicle base prefabs by delta overrides; a modded vehicle gets one by inheriting them or by adding this component to its own prefab.")]
class LC_VehicleCommsComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A vehicle's Lima Charlie comms. The radio is an LC radio entity the server spawns as a child of the
//! vehicle (its antenna), so vanilla's own radio components on some vehicles stay untouched and LC's radio
//! code sees an ordinary gadget radio. Players reach it from the allowed seats through a VEHICLE radio lease
//! (LC_RadioLeases); LC_VehicleSeat on their client asks for the lease and lists the radio in the VON menu.
class LC_VehicleCommsComponent : ScriptComponent
{
	protected static const ResourceName DEFAULT_RADIO_PREFAB = "{6A5C69DD20000080}Prefabs/LC/Vehicles/LC_VehicleRadio.et";

	[Attribute("1", desc: "Mount a Lima Charlie radio in this vehicle", category: "Radio")]
	protected bool m_bRadio;

	[Attribute(DEFAULT_RADIO_PREFAB, UIWidgets.ResourceNamePicker, "Radio entity spawned as a child of the vehicle: an RplComponent, a BaseRadioComponent with its transceivers and an SCR_RadioComponent, nothing a player could pick up", "et", category: "Radio")]
	protected ResourceName m_sRadioPrefab;

	[Attribute("0 2.5 0", UIWidgets.Coords, "Antenna position relative to the vehicle; transmissions start and reception is measured here", category: "Radio")]
	protected vector m_vAntennaOffset;

	[Attribute("7", UIWidgets.Flags, "Seats that reach the radio", "", ParamEnumArray.FromEnum(LC_ERadioLeaseSeat), category: "Radio")]
	protected int m_iRadioSeats;

	[Attribute("0", desc: "Only mount the radio and fit the intercom when the vehicle's default faction is military (civilian cars and trucks get neither)", category: "Radio")]
	protected bool m_bMilitaryOnly;

	[Attribute("1", desc: "Fit a crew / cargo / misc intercom: direct speech between players on the same channel of this vehicle is heard clear, whatever the distance or walls", category: "Intercom")]
	protected bool m_bIntercom;

	[Attribute("7", UIWidgets.Flags, "Seats that start on the crew channel; every other seat starts on cargo. Any seat may then pick any channel.", "", ParamEnumArray.FromEnum(LC_ERadioLeaseSeat), category: "Intercom")]
	protected int m_iIntercomCrewSeats;

	//! The radio entity, for every machine; invalid while there is none
	[RplProp()]
	protected RplId m_RadioId = RplId.Invalid();

	//! The radio's encryption key. The server sets it (the vehicle faction's key, or a crew member's on key
	//! fill) and every machine applies it to its own copy of the radio, so the key check before transmitting
	//! and the key the plugin is told agree everywhere.
	[RplProp(onRplName: "OnRadioKeyChanged")]
	protected string m_sRadioKey;

	//! Intercom channels players picked that differ from where their seat starts them, as (player id,
	//! LC_EIntercomChannel) pairs. Only the server writes it; every client reads it to tell the plugin who is on
	//! which channel. Whether a player is on this intercom at all every machine works out for itself, from
	//! their seat (or, for the infantry phone, their lease), so a stale pair of someone who got out is ignored.
	[RplProp()]
	protected ref array<int> m_aIntercomChoices = {};

	//! Weak: the vehicle owns it as a child
	protected IEntity m_Radio;

	//------------------------------------------------------------------------------------------------
	//! The comms component of the vehicle an entity belongs to (the entity itself, or a turret or cargo child of it)
	static LC_VehicleCommsComponent Find(IEntity entity)
	{
		IEntity vehicle = LC_RadioLeaseSeatRule.FindVehicle(entity);
		if (!vehicle)
			return null;

		return LC_VehicleCommsComponent.Cast(vehicle.FindComponent(LC_VehicleCommsComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle's radio entity, or null while it has none or it is not streamed in yet
	IEntity GetRadio()
	{
		if (!m_Radio && m_RadioId.IsValid())
		{
			m_Radio = LC_RadioLeases.FindRadio(m_RadioId);
			if (m_Radio)
				ApplyRadioKey(m_Radio);
		}

		return m_Radio;
	}

	//------------------------------------------------------------------------------------------------
	string GetRadioKey()
	{
		return m_sRadioKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: loads a key into the radio for everyone
	void SetRadioKey(string key)
	{
		if (!Replication.IsServer() || key == m_sRadioKey)
			return;

		m_sRadioKey = key;
		Replication.BumpMe();

		IEntity radio = GetRadio();
		if (radio)
			ApplyRadioKey(radio);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRadioKeyChanged()
	{
		IEntity radio = GetRadio();
		if (radio)
			ApplyRadioKey(radio);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyRadioKey(notnull IEntity radio)
	{
		BaseRadioComponent radioComponent = BaseRadioComponent.Cast(radio.FindComponent(BaseRadioComponent));
		if (radioComponent && radioComponent.GetEncryptionKey() != m_sRadioKey)
			radioComponent.SetEncryptionKey(m_sRadioKey);
	}

	//------------------------------------------------------------------------------------------------
	//! LC_ERadioLeaseSeat flags of the seats that reach the radio
	int GetRadioSeats()
	{
		return m_iRadioSeats;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a seat of this vehicle reaches its radio. Override point for vehicles whose seats do not follow
	//! vanilla's naming (keep it in step with the lease's seat rule, which only knows the flags).
	bool IsRadioSeat(notnull BaseCompartmentSlot slot)
	{
		return (LC_RadioLeaseSeatRule.GetSeat(slot) & m_iRadioSeats) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this vehicle has an intercom. The same on every machine: it comes from prefab data alone.
	bool HasIntercom()
	{
		if (!m_bIntercom)
			return false;

		if (!m_bMilitaryOnly)
			return true;

		SCR_Faction faction = GetDefaultFaction(GetOwner());
		return faction && faction.IsMilitary();
	}

	//------------------------------------------------------------------------------------------------
	//! The channel a seat starts on. Override point, like IsRadioSeat, for vehicles whose seats do not follow
	//! vanilla's naming.
	LC_EIntercomChannel GetSeatChannel(notnull BaseCompartmentSlot slot)
	{
		if (LC_RadioLeaseSeatRule.GetSeat(slot) & m_iIntercomCrewSeats)
			return LC_EIntercomChannel.CREW;

		return LC_EIntercomChannel.CARGO;
	}

	//------------------------------------------------------------------------------------------------
	//! The channel a player picked on this intercom, if they picked one
	bool GetIntercomChoice(int playerId, out LC_EIntercomChannel channel)
	{
		for (int i = 0; i + 1 < m_aIntercomChoices.Count(); i += 2)
		{
			if (m_aIntercomChoices[i] == playerId)
			{
				channel = m_aIntercomChoices[i + 1];
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The channel a player starts on here: their seat's, or the phone's while they hold it. -1 when they are
	//! not on this intercom.
	int GetDefaultChannel(int playerId, IEntity character)
	{
		if (!character || !HasIntercom())
			return -1;

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (chimera && chimera.GetCompartmentAccessComponent())
		{
			BaseCompartmentSlot slot = chimera.GetCompartmentAccessComponent().GetCompartment();
			if (slot && LC_RadioLeaseSeatRule.FindVehicle(slot.GetVehicle()) == GetOwner())
				return GetSeatChannel(slot);
		}

		return GetPhoneChannel(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The channel a player is on here, LC_EIntercomChannel.NONE when they are not on this intercom (or chose
	//! No Intercom): what game_state.json's players[].ic carries
	LC_EIntercomChannel GetPlayerChannel(int playerId, IEntity character)
	{
		int channel = GetDefaultChannel(playerId, character);
		if (channel < 0 || !LC_Life.CanSpeak(character))
			return LC_EIntercomChannel.NONE;

		LC_EIntercomChannel chosen;
		if (GetIntercomChoice(playerId, chosen))
			return chosen;

		return channel;
	}

	//------------------------------------------------------------------------------------------------
	//! The channel a player on the infantry phone is put on; -1 when they do not hold it. Part 4 fills this in.
	protected int GetPhoneChannel(int playerId)
	{
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records a player's pick, or forgets it (channel -1, back to where their seat starts them).
	//! Refused for players not on this intercom; pairs of players who have since left are dropped on the way.
	void ServerSetIntercomChoice(int playerId, int channel)
	{
		if (!Replication.IsServer() || (channel != -1 && !LC_Intercom.IsChannel(channel)))
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity character = playerManager.GetPlayerControlledEntity(playerId);
		if (GetDefaultChannel(playerId, character) < 0)
			return;

		for (int i = m_aIntercomChoices.Count() - 2; i >= 0; i -= 2)
		{
			int other = m_aIntercomChoices[i];
			if (other == playerId || GetDefaultChannel(other, playerManager.GetPlayerControlledEntity(other)) < 0)
			{
				m_aIntercomChoices.RemoveOrdered(i + 1);
				m_aIntercomChoices.RemoveOrdered(i);
			}
		}

		if (channel >= 0)
		{
			m_aIntercomChoices.Insert(playerId);
			m_aIntercomChoices.Insert(channel);
		}

		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Nothing is spawned while a world is being edited, and only the server spawns: the radio replicates
		// to clients with its place in the vehicle's hierarchy
		if (!m_bRadio || !GetGame().InPlayMode() || !Replication.IsServer())
			return;

		// After the vehicle's own initialisation, and outside it: its RplComponent must be in place first
		GetGame().GetCallqueue().CallLater(SpawnRadio, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(SpawnRadio);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnRadio()
	{
		IEntity owner = GetOwner();
		if (!owner || m_Radio || m_sRadioPrefab.IsEmpty())
			return;

		SCR_Faction faction = GetDefaultFaction(owner);
		if (m_bMilitaryOnly && (!faction || !faction.IsMilitary()))
			return;

		Resource resource = Resource.Load(m_sRadioPrefab);
		if (!resource || !resource.IsValid())
		{
			Print("[LC] Vehicle radio prefab not found: " + m_sRadioPrefab, LogLevel.WARNING);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		owner.GetWorldTransform(params.Transform);
		params.Transform[3] = owner.CoordToParent(m_vAntennaOffset);
		IEntity radio = GetGame().SpawnEntityPrefab(resource, owner.GetWorld(), params);
		if (!radio)
			return;

		owner.AddChild(radio, -1, EAddChildFlags.AUTO_TRANSFORM);
		Print(string.Format("[LC] Vehicle radio mounted on %1 (%2)", owner, LC_RadioLeases.GetRadioId(radio)), LogLevel.DEBUG);
		m_Radio = radio;
		m_RadioId = LC_RadioLeases.GetRadioId(radio);
		if (faction)
		{
			m_sRadioKey = faction.GetFactionRadioEncryptionKey();
			TuneToFaction(radio, faction);
		}

		ApplyRadioKey(radio);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! A new vehicle radio starts on its faction's platoon frequency, as a carried radio would
	protected void TuneToFaction(notnull IEntity radio, notnull SCR_Faction faction)
	{
		BaseRadioComponent radioComponent = BaseRadioComponent.Cast(radio.FindComponent(BaseRadioComponent));
		if (!radioComponent)
			return;

		int frequency = faction.GetFactionRadioFrequency();
		if (frequency <= 0)
			return;

		int count = radioComponent.TransceiversCount();
		for (int i = 0; i < count; i++)
		{
			BaseTransceiver transceiver = radioComponent.GetTransceiver(i);
			if (transceiver && frequency >= transceiver.GetMinFrequency() && frequency <= transceiver.GetMaxFrequency())
				transceiver.SetFrequency(frequency);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_Faction GetDefaultFaction(notnull IEntity vehicle)
	{
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return null;

		return SCR_Faction.Cast(affiliation.GetDefaultAffiliatedFaction());
	}
}
