//------------------------------------------------------------------------------------------------
//! Client: keeps the local player's vehicle comms in step with where they sit. One reconciler instead of
//! seat events: every frame it works out which vehicle radio the seat reaches (a pointer walk, nothing
//! more), asks for its VEHICLE lease when that changes, gives the old one back, and lists the radio's
//! transceivers in the VON menu once the lease is held. Likewise it lists the intercom entry, last, while the
//! player is on a vehicle's intercom, and holds the channel they picked there. That also covers death,
//! respawn and possession, and vanilla clearing the VON list, without hooking any of them.
class LC_VehicleSeat
{
	//! How often an unanswered (or refused) lease request is repeated while the seat still wants the radio
	protected static const int REQUEST_RETRY_MS = 3000;

	protected RplId m_RadioId = RplId.Invalid();
	protected int m_iNextRequestTick;
	//! Entries of the leased vehicle radio, one per transceiver, while listed in the VON menu
	protected ref array<ref LC_VONEntryVehicleRadio> m_aEntries = {};

	//! The comms of the vehicle whose intercom the local player is on; null while on none
	protected LC_VehicleCommsComponent m_IntercomComms;
	protected ref LC_VONEntryIntercom m_IntercomEntry;
	//! The channel the local player picked on m_IntercomComms; -1 while on the one their seat starts them on
	protected int m_iIntercomChoice = -1;
	//! Weak: the character the last update was for
	protected IEntity m_Character;

	//------------------------------------------------------------------------------------------------
	void Update(IEntity character, SCR_VONController vonController, int now)
	{
		m_Character = character;
		UpdateRadio(character, vonController, now);
		UpdateIntercom(character, vonController);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateRadio(IEntity character, SCR_VONController vonController, int now)
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
		RemoveIntercomEntry(vonController);
		m_IntercomComms = null;
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
	//! The channel the local player is on: self.ic in game_state.json
	LC_EIntercomChannel GetIntercomChannel()
	{
		if (!m_IntercomComms)
			return LC_EIntercomChannel.NONE;

		if (m_iIntercomChoice >= 0)
			return m_iIntercomChoice;

		int channel = m_IntercomComms.GetDefaultChannel(GetLocalPlayerId(), m_Character);
		if (channel < 0)
			return LC_EIntercomChannel.NONE;

		return channel;
	}

	//------------------------------------------------------------------------------------------------
	//! The channel another player is on, as far as the local player's intercom is concerned: players[].ic.
	//! NONE unless both are on the same vehicle's intercom.
	LC_EIntercomChannel GetPlayerIntercomChannel(int playerId, IEntity character)
	{
		if (!m_IntercomComms || GetIntercomChannel() == LC_EIntercomChannel.NONE)
			return LC_EIntercomChannel.NONE;

		return m_IntercomComms.GetPlayerChannel(playerId, character);
	}

	//------------------------------------------------------------------------------------------------
	//! The local player picks a channel (the intercom entry): heard straight away on this side, and told to the
	//! server so everyone else's plugin hears them there too
	void SetIntercomChannel(LC_EIntercomChannel channel)
	{
		if (!m_IntercomComms)
			return;

		m_iIntercomChoice = channel;
		SendIntercomChoice(channel);
	}

	//------------------------------------------------------------------------------------------------
	//! The intercom the character is on: the one of the vehicle they sit in, else the one whose infantry phone
	//! they hold
	protected LC_VehicleCommsComponent FindIntercom(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !LC_Life.CanHear(chimera))
			return null;

		CompartmentAccessComponent access = chimera.GetCompartmentAccessComponent();
		if (access && access.GetCompartment())
		{
			LC_VehicleCommsComponent comms = LC_VehicleCommsComponent.Find(access.GetCompartment().GetVehicle());
			if (comms && comms.HasIntercom())
				return comms;
		}

		return FindPhoneIntercom(character);
	}

	//------------------------------------------------------------------------------------------------
	//! The intercom of the vehicle whose infantry phone the character holds. Part 4 fills this in.
	protected LC_VehicleCommsComponent FindPhoneIntercom(IEntity character)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateIntercom(IEntity character, SCR_VONController vonController)
	{
		LC_VehicleCommsComponent comms = FindIntercom(character);
		if (comms != m_IntercomComms)
		{
			RemoveIntercomEntry(vonController);
			m_IntercomComms = comms;
			m_iIntercomChoice = -1;

			// A pick left over from an earlier ride in this vehicle: start this one where the seat says
			LC_EIntercomChannel stale;
			if (comms && comms.GetIntercomChoice(GetLocalPlayerId(), stale))
				SendIntercomChoice(-1);
		}

		if (!m_IntercomComms || !vonController)
			return;

		if (!m_IntercomEntry)
			m_IntercomEntry = new LC_VONEntryIntercom(this);

		// Last in the list: radio entries that arrive later (a lease granted, a radio picked up) go after it
		// at first, so it is moved back behind them
		if (vonController.LC_HasEntry(m_IntercomEntry))
		{
			if (vonController.LC_IsLastEntry(m_IntercomEntry))
				return;

			vonController.RemoveEntry(m_IntercomEntry);
		}

		vonController.AddEntry(m_IntercomEntry);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveIntercomEntry(SCR_VONController vonController)
	{
		if (vonController && m_IntercomEntry && vonController.LC_HasEntry(m_IntercomEntry))
			vonController.RemoveEntry(m_IntercomEntry);
	}

	//------------------------------------------------------------------------------------------------
	//! \param channel LC_EIntercomChannel, or -1 to go back to the seat's
	protected void SendIntercomChoice(int channel)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController && m_IntercomComms)
			playerController.LC_SetIntercomChannel(LC_RadioLeases.GetRadioId(m_IntercomComms.GetOwner()), channel);
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetLocalPlayerId()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return 0;

		return playerController.GetPlayerId();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsFor(notnull LC_VONEntryVehicleRadio entry, notnull IEntity radio)
	{
		BaseTransceiver transceiver = entry.GetTransceiver();
		return transceiver && transceiver.GetRadio() && transceiver.GetRadio().GetOwner() == radio;
	}
}
