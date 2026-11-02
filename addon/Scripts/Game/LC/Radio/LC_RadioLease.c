//------------------------------------------------------------------------------------------------
//! How a player reaches a radio that is not in their own inventory. The kind picks the rules the server
//! holds the lease under (LC_RadioLeases.CollectRules) and the "src" the radio is listed with in
//! game_state.json (LC_RadioLeases.GetSourceOfKind).
enum LC_ERadioLeaseKind
{
	//! Someone else's (or a dropped) manpack, through its handset cable
	BORROWED,
	//! A radio mounted in a vehicle, from one of its seats
	VEHICLE,
	//! A vehicle's infantry phone (LC_InfantryPhoneComponent): the leased entity is the vehicle itself, and
	//! the lease puts the holder on its intercom. Never listed as a radio.
	PHONE
}

//------------------------------------------------------------------------------------------------
//! Why the server turned a lease request down; NONE is a grant
enum LC_ERadioLeaseRefusal
{
	NONE,
	//! No such radio, or it has no radio component
	NO_RADIO,
	//! The requester controls no character
	NO_PLAYER,
	DEAD,
	//! The radio is in the requester's own inventory, which needs no lease
	OWN_RADIO,
	BAD_KIND,
	//! Nothing says how this kind of lease is held on this radio, so it is not granted at all
	NO_RULES,
	OUT_OF_RANGE,
	//! Not in a seat that reaches the radio, or (infantry phone) in a seat at all
	WRONG_SEAT
}

//------------------------------------------------------------------------------------------------
//! Vehicle seats as a lease's seat rule tells them apart. Commander and gunner are both TURRET
//! compartments; vanilla marks the commander's by its unique name (LC_RadioLeaseSeatRule.COMMANDER_NAME).
enum LC_ERadioLeaseSeat
{
	PILOT = 1,
	COMMANDER = 2,
	GUNNER = 4,
	CARGO = 8
}

//------------------------------------------------------------------------------------------------
//! One condition a lease is granted and kept under. The server checks every rule of a lease on the request,
//! a few times a second while it is held, and on a seat change if the rule says it cares about seats.
class LC_RadioLeaseRule
{
	//------------------------------------------------------------------------------------------------
	//! \param granting true for the request itself, false for the checks while held (which may be a little
	//! more forgiving, so a lease does not flicker at the edge of its limit)
	bool Allows(notnull IEntity holder, notnull IEntity radio, bool granting)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What the requester is told when this rule turns a request down
	LC_ERadioLeaseRefusal GetRefusal()
	{
		return LC_ERadioLeaseRefusal.OUT_OF_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the holder's seat changes are worth an immediate check
	bool WatchesSeat()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static vector GetWorldPosition(notnull IEntity entity)
	{
		vector transform[4];
		entity.GetWorldTransform(transform);
		return transform[3];
	}
}

//------------------------------------------------------------------------------------------------
//! The holder stays within a cable's (or a link's) length of the radio
class LC_RadioLeaseDistanceRule : LC_RadioLeaseRule
{
	//! Extra length allowed while held, so standing at the limit does not drop and retake the lease
	protected static const float HOLD_SLACK_M = 0.5;

	protected float m_fMaxDistance;

	//------------------------------------------------------------------------------------------------
	void LC_RadioLeaseDistanceRule(float maxDistance)
	{
		m_fMaxDistance = maxDistance;
	}

	//------------------------------------------------------------------------------------------------
	override bool Allows(notnull IEntity holder, notnull IEntity radio, bool granting)
	{
		float limit = m_fMaxDistance;
		if (!granting)
			limit += HOLD_SLACK_M;

		return vector.DistanceSq(GetWorldPosition(holder), GetAnchor(radio)) <= limit * limit;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the cable is measured from: the radio itself, unless a subclass knows better
	protected vector GetAnchor(notnull IEntity radio)
	{
		return GetWorldPosition(radio);
	}

	//------------------------------------------------------------------------------------------------
	float GetMaxDistance()
	{
		return m_fMaxDistance;
	}
}

//------------------------------------------------------------------------------------------------
//! The holder sits in one of the allowed seats of one vehicle. IsSeatAllowed is the override point for
//! vehicles whose seats do not follow vanilla's naming.
class LC_RadioLeaseSeatRule : LC_RadioLeaseRule
{
	//! CompartmentUniqueName vanilla gives commander turrets (BTR-70, LAV-25, BRDM-2...)
	static const string COMMANDER_NAME = "CommanderTurret";
	//! Driver/pilot, commander and gunner: the seats the 3.0 plan gives a vehicle's radio by default
	static const int CREW_SEATS = LC_ERadioLeaseSeat.PILOT | LC_ERadioLeaseSeat.COMMANDER | LC_ERadioLeaseSeat.GUNNER;

	//! Weak: a deleted vehicle refuses every seat
	protected IEntity m_Vehicle;
	protected int m_iSeats;

	//------------------------------------------------------------------------------------------------
	//! \param seats LC_ERadioLeaseSeat flags
	void LC_RadioLeaseSeatRule(IEntity vehicle, int seats)
	{
		m_Vehicle = vehicle;
		m_iSeats = seats;
	}

	//------------------------------------------------------------------------------------------------
	override bool Allows(notnull IEntity holder, notnull IEntity radio, bool granting)
	{
		if (!m_Vehicle)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(holder);
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access)
			return false;

		// The slot may sit on a turret or cargo child of the vehicle; GetVehicle walks up to the vehicle itself
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot || slot.GetVehicle() != m_Vehicle)
			return false;

		return IsSeatAllowed(slot);
	}

	//------------------------------------------------------------------------------------------------
	override LC_ERadioLeaseRefusal GetRefusal()
	{
		return LC_ERadioLeaseRefusal.WRONG_SEAT;
	}

	//------------------------------------------------------------------------------------------------
	override bool WatchesSeat()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this seat of the rule's vehicle reaches the radio
	protected bool IsSeatAllowed(notnull BaseCompartmentSlot slot)
	{
		return (GetSeat(slot) & m_iSeats) != 0;
	}

	//------------------------------------------------------------------------------------------------
	static LC_ERadioLeaseSeat GetSeat(notnull BaseCompartmentSlot slot)
	{
		ECompartmentType type = slot.GetType();
		if (type == ECompartmentType.PILOT)
			return LC_ERadioLeaseSeat.PILOT;

		if (type == ECompartmentType.TURRET)
		{
			if (slot.GetCompartmentUniqueName() == COMMANDER_NAME)
				return LC_ERadioLeaseSeat.COMMANDER;

			return LC_ERadioLeaseSeat.GUNNER;
		}

		return LC_ERadioLeaseSeat.CARGO;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle an entity is part of: the topmost of it and its parents with a compartment manager, as
	//! BaseCompartmentSlot.GetVehicle finds it for a seat. Null for anything not on a vehicle.
	static IEntity FindVehicle(IEntity entity)
	{
		IEntity vehicle;
		while (entity)
		{
			if (entity.FindComponent(BaseCompartmentManagerComponent))
				vehicle = entity;

			entity = entity.GetParent();
		}

		return vehicle;
	}
}

//------------------------------------------------------------------------------------------------
//! The holder is on foot: in no vehicle seat at all. The infantry phone is used from outside; getting in
//! (any vehicle) hangs it up.
class LC_RadioLeaseOnFootRule : LC_RadioLeaseRule
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(notnull IEntity holder, notnull IEntity radio, bool granting)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(holder);
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return !access || !access.GetCompartment();
	}

	//------------------------------------------------------------------------------------------------
	override LC_ERadioLeaseRefusal GetRefusal()
	{
		return LC_ERadioLeaseRefusal.WRONG_SEAT;
	}

	//------------------------------------------------------------------------------------------------
	override bool WatchesSeat()
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A lease as the server keeps it. Clients only see the replicated (radio, player, kind) triple.
class LC_RadioLease
{
	RplId m_RadioId;
	int m_iPlayerId;
	LC_ERadioLeaseKind m_eKind;
	//! Weak references: a deleted radio, or a holder whose character is gone, ends the lease
	IEntity m_Radio;
	IEntity m_Holder;
	ref array<ref LC_RadioLeaseRule> m_aRules = {};
	//! The holder's seat events this lease listens to, if any of its rules watch seats
	SCR_CompartmentAccessComponent m_SeatWatch;

	//------------------------------------------------------------------------------------------------
	bool WatchesSeat()
	{
		foreach (LC_RadioLeaseRule rule : m_aRules)
		{
			if (rule.WatchesSeat())
				return true;
		}

		return false;
	}
}
