//------------------------------------------------------------------------------------------------
//! Client: keeps the local player's vehicle comms in step with where they sit. One reconciler instead of
//! seat events: every frame it works out which vehicle radio the seat reaches (a pointer walk, nothing
//! more), asks for its VEHICLE lease when that changes, gives the old one back, and lists the radio's
//! transceivers in the VON menu once the lease is held. That also covers death, respawn and possession,
//! and vanilla clearing the VON list, without hooking any of them.
class LC_VehicleSeat
{
	//! How often an unanswered (or refused) lease request is repeated while the seat still wants the radio
	protected static const int REQUEST_RETRY_MS = 3000;

	protected RplId m_RadioId = RplId.Invalid();
	protected int m_iNextRequestTick;
	//! Entries of the leased vehicle radio, one per transceiver, while listed in the VON menu
	protected ref array<ref LC_VONEntryVehicleRadio> m_aEntries = {};

	//------------------------------------------------------------------------------------------------
	void Update(IEntity character, SCR_VONController vonController, int now)
	{
		LC_RadioLeases leases = LC_RadioLeases.Get();
		if (!leases)
			return;

		IEntity radio = FindSeatRadio(character);
		RplId radioId = LC_RadioLeases.GetRadioId(radio);
		if (radioId != m_RadioId)
		{
			RemoveEntries(vonController);
			if (m_RadioId.IsValid())
				leases.ReleaseById(m_RadioId);

			m_RadioId = radioId;
			m_iNextRequestTick = now;
		}

		if (!radioId.IsValid())
			return;

		if (leases.GetOwnKind(radioId) != LC_ERadioLeaseKind.VEHICLE)
		{
			// Not granted yet, or the server ended it (a seat check that ran before ours caught up)
			RemoveEntries(vonController);
			if (now >= m_iNextRequestTick)
			{
				m_iNextRequestTick = now + REQUEST_RETRY_MS;
				leases.Request(radio, LC_ERadioLeaseKind.VEHICLE);
			}

			return;
		}

		ListEntries(radio, vonController);
	}

	//------------------------------------------------------------------------------------------------
	//! The client is going away: give the lease back and take the entries out of the menu
	void Clear(SCR_VONController vonController)
	{
		RemoveEntries(vonController);
		LC_RadioLeases leases = LC_RadioLeases.Get();
		if (leases && m_RadioId.IsValid())
			leases.ReleaseById(m_RadioId);

		m_RadioId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! The radio of the vehicle the character sits in, if their seat reaches it and they can hear at all
	static IEntity FindSeatRadio(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !LC_Life.CanHear(chimera))
			return null;

		CompartmentAccessComponent access = chimera.GetCompartmentAccessComponent();
		if (!access)
			return null;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return null;

		LC_VehicleCommsComponent comms = LC_VehicleCommsComponent.Find(slot.GetVehicle());
		if (!comms || !comms.IsRadioSeat(slot))
			return null;

		return comms.GetRadio();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the entries once per radio and puts back any the VON controller has lost
	protected void ListEntries(notnull IEntity radio, SCR_VONController vonController)
	{
		if (!vonController)
			return;

		if (m_aEntries.IsEmpty() || !IsFor(m_aEntries[0], radio))
		{
			RemoveEntries(vonController);
			BuildEntries(radio);
		}

		foreach (LC_VONEntryVehicleRadio entry : m_aEntries)
		{
			if (!vonController.LC_HasEntry(entry))
				vonController.AddEntry(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildEntries(notnull IEntity radio)
	{
		// A radio entry needs its gadget, as when vanilla lists a carried radio
		SCR_RadioComponent gadget = SCR_RadioComponent.Cast(radio.FindComponent(SCR_RadioComponent));
		if (!gadget)
			return;

		BaseRadioComponent radioComponent = gadget.GetRadioComponent();
		if (!radioComponent)
			return;

		int count = radioComponent.TransceiversCount();
		for (int i = 0; i < count; i++)
		{
			BaseTransceiver transceiver = radioComponent.GetTransceiver(i);
			if (!transceiver)
				continue;

			LC_VONEntryVehicleRadio entry = new LC_VONEntryVehicleRadio();
			entry.SetRadioEntry(transceiver, i + 1, gadget);
			m_aEntries.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveEntries(SCR_VONController vonController)
	{
		if (vonController)
		{
			foreach (LC_VONEntryVehicleRadio entry : m_aEntries)
			{
				if (vonController.LC_HasEntry(entry))
					vonController.RemoveEntry(entry);
			}
		}

		m_aEntries.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsFor(notnull LC_VONEntryVehicleRadio entry, notnull IEntity radio)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		return transceiver && transceiver.GetRadio() && transceiver.GetRadio().GetOwner() == radio;
	}
}
