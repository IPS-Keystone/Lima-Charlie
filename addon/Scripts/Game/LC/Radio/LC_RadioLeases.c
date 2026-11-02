//------------------------------------------------------------------------------------------------
//! Server-arbitrated access to radios that are not in the player's own inventory: a borrowed manpack's
//! handset, a vehicle radio from a seat. Only access is arbitrated; what is said stays client-trusted.
//!
//! One instance per game mode (SCR_BaseGameMode.LC_GetRadioLeases), on every machine. The server keeps the
//! leases with their rules and publishes them as (radio RplId, player id, kind) triples in a replicated
//! array on the game mode, so any client can ask who holds a radio and no radio prefab needs a component.
//! Access is not exclusive: any number of players may hold one radio, and its owner never needs a lease.
class LC_RadioLeases
{
	//! Handset cable of a borrowed manpack (3.0 plan, A)
	static const float HANDSET_CABLE_M = 4;
	//! How often the server re-checks held leases, only while there are any
	protected static const int CHECK_INTERVAL_MS = 500;
	//! Entries per lease in the replicated state: radio, player, kind
	protected static const int STATE_STRIDE = 3;

	//! Weak: the game mode owns this object and the replicated state
	protected SCR_BaseGameMode m_GameMode;

	//! Server only
	protected ref array<ref LC_RadioLease> m_aLeases = {};
	protected bool m_bChecking;
	protected bool m_bCheckQueued;

	//! The local player's own leases as last seen in the state, and a count of changes to them
	protected ref array<RplId> m_aOwnRadios = {};
	protected ref array<int> m_aOwnKinds = {};
	protected int m_iOwnRevision;
	protected ref ScriptInvokerVoid m_OnOwnLeasesChanged;

	//------------------------------------------------------------------------------------------------
	void LC_RadioLeases(SCR_BaseGameMode gameMode)
	{
		m_GameMode = gameMode;
	}

	//------------------------------------------------------------------------------------------------
	void ~LC_RadioLeases()
	{
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
		{
			game.GetCallqueue().Remove(ServerCheck);
			game.GetCallqueue().Remove(ServerCheckQueued);
		}

		foreach (LC_RadioLease lease : m_aLeases)
		{
			if (lease.m_SeatWatch)
				RemoveSeatHandlers(lease.m_SeatWatch);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The current game mode's leases, or null before there is a game mode
	static LC_RadioLeases Get()
	{
		ArmaReforgerScripted game = GetGame();
		if (!game)
			return null;

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(game.GetGameMode());
		if (!gameMode)
			return null;

		return gameMode.LC_GetRadioLeases();
	}

	//------------------------------------------------------------------------------------------------
	//! Leases are keyed by the radio entity's RplComponent id, the same number LC_Radio.GetRadioKey gives
	static RplId GetRadioId(IEntity radio)
	{
		if (!radio)
			return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(radio.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! The radio entity behind an id, or null where it does not exist or is not streamed in
	static IEntity FindRadio(RplId radioId)
	{
		if (!radioId.IsValid())
			return null;

		RplComponent rpl = RplComponent.Cast(Replication.FindItem(radioId));
		if (!rpl)
			return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! What a leased radio is listed as in game_state.json (a PHONE lease is on a vehicle, which is never listed)
	static LC_ERadioSource GetSourceOfKind(LC_ERadioLeaseKind kind)
	{
		if (kind == LC_ERadioLeaseKind.VEHICLE)
			return LC_ERadioSource.VEHICLE;

		return LC_ERadioSource.BORROWED;
	}

	//------------------------------------------------------------------------------------------------
	static string GetKindName(LC_ERadioLeaseKind kind)
	{
		return typename.EnumToString(LC_ERadioLeaseKind, kind);
	}

	//------------------------------------------------------------------------------------------------
	static string GetRefusalName(LC_ERadioLeaseRefusal refusal)
	{
		return typename.EnumToString(LC_ERadioLeaseRefusal, refusal);
	}

	//================================================================================================
	// Client side: ask, give back, look up. Works on every machine; on the server the state is the one
	// it publishes.
	//================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Asks the server for access to a radio. A grant shows up in the replicated state (and fires
	//! GetOnOwnLeasesChanged); a refusal is logged. Asking again for a radio already held changes nothing.
	//! \return false if the request could not be sent
	bool Request(IEntity radio, LC_ERadioLeaseKind kind)
	{
		RplId radioId = GetRadioId(radio);
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!radioId.IsValid() || !playerController)
			return false;

		playerController.LC_RequestRadioLease(radioId, kind);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Release(IEntity radio)
	{
		ReleaseById(GetRadioId(radio));
	}

	//------------------------------------------------------------------------------------------------
	void ReleaseById(RplId radioId)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (radioId.IsValid() && playerController)
			playerController.LC_ReleaseRadioLease(radioId);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives back every radio the local player holds by lease
	void ReleaseAll()
	{
		array<RplId> radios = {};
		radios.Copy(m_aOwnRadios);
		foreach (RplId radioId : radios)
		{
			ReleaseById(radioId);
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsHolder(int playerId, IEntity radio)
	{
		return IsHolderById(playerId, GetRadioId(radio));
	}

	//------------------------------------------------------------------------------------------------
	bool IsHolderById(int playerId, RplId radioId)
	{
		array<int> state = GetState();
		if (!state || !radioId.IsValid())
			return false;

		for (int i = 0; i + STATE_STRIDE <= state.Count(); i += STATE_STRIDE)
		{
			if (state[i] == radioId && state[i + 1] == playerId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Kind of a player's lease on a radio, or -1 if they hold none
	int GetHolderKind(int playerId, RplId radioId)
	{
		array<int> state = GetState();
		if (!state || !radioId.IsValid())
			return -1;

		for (int i = 0; i + STATE_STRIDE <= state.Count(); i += STATE_STRIDE)
		{
			if (state[i] == radioId && state[i + 1] == playerId)
				return state[i + 2];
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Everyone holding a radio by lease (not its owner, who needs none)
	void GetHolders(IEntity radio, notnull array<int> playerIds)
	{
		playerIds.Clear();
		RplId radioId = GetRadioId(radio);
		array<int> state = GetState();
		if (!state || !radioId.IsValid())
			return;

		for (int i = 0; i + STATE_STRIDE <= state.Count(); i += STATE_STRIDE)
		{
			if (state[i] == radioId)
				playerIds.Insert(state[i + 1]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How many leases anyone holds, on any machine (with GetLeaseAt, for code that draws every holder)
	int GetLeaseCount()
	{
		array<int> state = GetState();
		if (!state)
			return 0;

		return state.Count() / STATE_STRIDE;
	}

	//------------------------------------------------------------------------------------------------
	//! One lease of GetLeaseCount's, in the order the server keeps them
	void GetLeaseAt(int index, out RplId radioId, out int playerId, out LC_ERadioLeaseKind kind)
	{
		array<int> state = GetState();
		int first = index * STATE_STRIDE;
		if (!state || index < 0 || first + STATE_STRIDE > state.Count())
		{
			radioId = RplId.Invalid();
			playerId = 0;
			kind = -1;
			return;
		}

		radioId = state[first];
		playerId = state[first + 1];
		kind = state[first + 2];
	}

	//------------------------------------------------------------------------------------------------
	//! The local player's leased radios and their kinds, in the order they were granted
	void GetOwnLeases(notnull array<RplId> radios, array<int> kinds = null)
	{
		radios.Copy(m_aOwnRadios);
		if (kinds)
			kinds.Copy(m_aOwnKinds);
	}

	//------------------------------------------------------------------------------------------------
	//! Kind of the local player's lease on a radio, or -1 if they hold none
	int GetOwnKind(RplId radioId)
	{
		int index = m_aOwnRadios.Find(radioId);
		if (index < 0)
			return -1;

		return m_aOwnKinds[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Goes up by one whenever the local player's own leases change
	int GetOwnRevision()
	{
		return m_iOwnRevision;
	}

	//------------------------------------------------------------------------------------------------
	//! Fires on the local player's machine when a lease of theirs is granted or ends
	ScriptInvokerVoid GetOnOwnLeasesChanged()
	{
		if (!m_OnOwnLeasesChanged)
			m_OnOwnLeasesChanged = new ScriptInvokerVoid();

		return m_OnOwnLeasesChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! The replicated state changed: from replication on a client, from the server's own publish on a host
	void OnStateChanged()
	{
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		array<int> state = GetState();
		if (localPlayerId <= 0 || !state)
			return;

		array<RplId> radios = {};
		array<int> kinds = {};
		for (int i = 0; i + STATE_STRIDE <= state.Count(); i += STATE_STRIDE)
		{
			if (state[i + 1] != localPlayerId)
				continue;

			radios.Insert(state[i]);
			kinds.Insert(state[i + 2]);
		}

		bool changed = radios.Count() != m_aOwnRadios.Count();
		for (int i = 0; !changed && i < radios.Count(); i++)
		{
			changed = radios[i] != m_aOwnRadios[i] || kinds[i] != m_aOwnKinds[i];
		}

		if (!changed)
			return;

		m_aOwnRadios.Copy(radios);
		m_aOwnKinds.Copy(kinds);
		m_iOwnRevision++;
		Print(string.Format("[LC] Radio leases held: %1", m_aOwnRadios.Count()), LogLevel.NORMAL);
		if (m_OnOwnLeasesChanged)
			m_OnOwnLeasesChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! The server turned down one of the local player's requests
	void OnRefused(RplId radioId, LC_ERadioLeaseRefusal refusal)
	{
		Print(string.Format("[LC] Radio lease refused: radio %1, %2", radioId, GetRefusalName(refusal)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected array<int> GetState()
	{
		if (!m_GameMode)
			return null;

		return m_GameMode.LC_GetRadioLeaseState();
	}

	//================================================================================================
	// Server side
	//================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Validates and grants a request. Also the entry point for server code that grants leases itself.
	//! \return NONE when the player holds the lease afterwards, including when they already did
	LC_ERadioLeaseRefusal ServerRequest(int playerId, RplId radioId, LC_ERadioLeaseKind kind)
	{
		if (!Replication.IsServer())
			return LC_ERadioLeaseRefusal.NO_PLAYER;

		if (kind != LC_ERadioLeaseKind.BORROWED && kind != LC_ERadioLeaseKind.VEHICLE && kind != LC_ERadioLeaseKind.PHONE)
			return LC_ERadioLeaseRefusal.BAD_KIND;

		IEntity holder = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!holder)
			return LC_ERadioLeaseRefusal.NO_PLAYER;

		if (!LC_Life.CanHear(holder))
			return LC_ERadioLeaseRefusal.DEAD;

		// For the infantry phone the leased entity is the vehicle, which has the phone instead of a radio
		IEntity radio = FindRadio(radioId);
		if (!radio)
			return LC_ERadioLeaseRefusal.NO_RADIO;

		typename required = BaseRadioComponent;
		if (kind == LC_ERadioLeaseKind.PHONE)
			required = LC_InfantryPhoneComponent;

		if (!radio.FindComponent(required))
			return LC_ERadioLeaseRefusal.NO_RADIO;

		if (radio.GetRootParent() == holder)
			return LC_ERadioLeaseRefusal.OWN_RADIO;

		// A repeated request (a double click, a reconciler re-asking) is a grant that changes nothing; the
		// first lease's kind stands
		if (FindLease(playerId, radioId))
			return LC_ERadioLeaseRefusal.NONE;

		LC_RadioLease lease = new LC_RadioLease();
		CollectRules(kind, radio, holder, lease.m_aRules);
		if (lease.m_aRules.IsEmpty())
			return Refuse(playerId, radioId, kind, LC_ERadioLeaseRefusal.NO_RULES);

		foreach (LC_RadioLeaseRule rule : lease.m_aRules)
		{
			if (!rule.Allows(holder, radio, true))
				return Refuse(playerId, radioId, kind, rule.GetRefusal());
		}

		lease.m_RadioId = radioId;
		lease.m_iPlayerId = playerId;
		lease.m_eKind = kind;
		lease.m_Radio = radio;
		lease.m_Holder = holder;
		m_aLeases.Insert(lease);
		Watch(lease);
		lease.m_HandsetItem = LC_HandsetItem.ServerGive(holder, GetHandsetItemPrefab(kind, radio));
		Print(string.Format("[LC] Radio lease granted: player %1, radio %2, %3", playerId, radioId, GetKindName(kind)), LogLevel.NORMAL);

		UpdateChecking();
		Publish();
		return LC_ERadioLeaseRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	void ServerRelease(int playerId, RplId radioId, string reason)
	{
		for (int i = m_aLeases.Count() - 1; i >= 0; i--)
		{
			LC_RadioLease lease = m_aLeases[i];
			if (lease.m_iPlayerId == playerId && lease.m_RadioId == radioId)
			{
				End(i, reason);
				UpdateChecking();
				Publish();
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ends every lease a player holds: death, disconnect
	void ServerReleasePlayer(int playerId, string reason)
	{
		bool changed;
		for (int i = m_aLeases.Count() - 1; i >= 0; i--)
		{
			if (m_aLeases[i].m_iPlayerId != playerId)
				continue;

			End(i, reason);
			changed = true;
		}

		if (!changed)
			return;

		UpdateChecking();
		Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! Override point for what uses leases (vehicle radios, handsets, the infantry phone, SATCOM links): the
	//! rules a lease of this kind on this radio is held under. Leaving the list empty refuses the request,
	//! so no kind ever reaches a radio across the map by default.
	protected void CollectRules(LC_ERadioLeaseKind kind, notnull IEntity radio, notnull IEntity holder, notnull array<ref LC_RadioLeaseRule> rules)
	{
		if (kind == LC_ERadioLeaseKind.BORROWED)
		{
			rules.Insert(new LC_RadioLeaseDistanceRule(HANDSET_CABLE_M));
			return;
		}

		if (kind == LC_ERadioLeaseKind.VEHICLE)
		{
			// The seats a vehicle's comms component names; a radio on a vehicle without one (vanilla's own
			// vehicle radios) goes by the default crew seats
			IEntity vehicle = LC_RadioLeaseSeatRule.FindVehicle(radio);
			if (!vehicle)
				return;

			int seats = LC_RadioLeaseSeatRule.CREW_SEATS;
			LC_VehicleCommsComponent comms = LC_VehicleCommsComponent.Cast(vehicle.FindComponent(LC_VehicleCommsComponent));
			if (comms)
				seats = comms.GetRadioSeats();

			rules.Insert(new LC_RadioLeaseSeatRule(vehicle, seats));
			return;
		}

		if (kind == LC_ERadioLeaseKind.PHONE)
		{
			// Only a phone wired to an intercom; the cable is measured from the phone, not the vehicle's origin
			LC_InfantryPhoneComponent phone = LC_InfantryPhoneComponent.Cast(radio.FindComponent(LC_InfantryPhoneComponent));
			if (!phone || !phone.GetIntercom())
				return;

			rules.Insert(new LC_RadioLeasePhoneRule(phone.GetCableLength()));
			rules.Insert(new LC_RadioLeaseOnFootRule());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected LC_ERadioLeaseRefusal Refuse(int playerId, RplId radioId, LC_ERadioLeaseKind kind, LC_ERadioLeaseRefusal refusal)
	{
		Print(string.Format("[LC] Radio lease refused: player %1, radio %2, %3, %4", playerId, radioId, GetKindName(kind), GetRefusalName(refusal)), LogLevel.NORMAL);
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	protected LC_RadioLease FindLease(int playerId, RplId radioId)
	{
		foreach (LC_RadioLease lease : m_aLeases)
		{
			if (lease.m_iPlayerId == playerId && lease.m_RadioId == radioId)
				return lease;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The item the holder carries while the lease lasts (the radio hold pose), or empty for none. Override
	//! point: the manpack handset (A) returns its own for BORROWED.
	protected ResourceName GetHandsetItemPrefab(LC_ERadioLeaseKind kind, notnull IEntity radio)
	{
		if (kind != LC_ERadioLeaseKind.PHONE)
			return ResourceName.Empty;

		LC_InfantryPhoneComponent phone = LC_InfantryPhoneComponent.Cast(radio.FindComponent(LC_InfantryPhoneComponent));
		if (!phone)
			return ResourceName.Empty;

		return phone.GetHandsetItemPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes one lease without publishing, so a sweep publishes once
	protected void End(int index, string reason)
	{
		LC_RadioLease lease = m_aLeases[index];
		Print(string.Format("[LC] Radio lease ended: player %1, radio %2, %3", lease.m_iPlayerId, lease.m_RadioId, reason), LogLevel.NORMAL);
		m_aLeases.Remove(index);
		Unwatch(lease);
		LC_HandsetItem.ServerTake(lease.m_HandsetItem);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-checks every lease: the radio still exists, the holder is still the same living character, and
	//! every rule still allows it
	protected void ServerCheck()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		bool changed;
		for (int i = m_aLeases.Count() - 1; i >= 0; i--)
		{
			LC_RadioLease lease = m_aLeases[i];
			string reason = GetEndReason(lease, playerManager.GetPlayerControlledEntity(lease.m_iPlayerId));
			if (reason.IsEmpty())
				continue;

			End(i, reason);
			changed = true;
		}

		if (!changed)
			return;

		UpdateChecking();
		Publish();
	}

	//------------------------------------------------------------------------------------------------
	//! Why a held lease has to end now, or empty if it holds
	protected string GetEndReason(notnull LC_RadioLease lease, IEntity controlled)
	{
		if (!lease.m_Radio)
			return "radio gone";

		// Respawned, possessed something else, or the character was deleted
		if (!lease.m_Holder || controlled != lease.m_Holder)
			return "holder changed";

		if (!LC_Life.CanHear(lease.m_Holder))
			return "holder dead";

		// Dropped or handed over: the cable can't follow it
		if (lease.m_HandsetItem && lease.m_HandsetItem.GetRootParent() != lease.m_Holder)
			return "handset dropped";

		foreach (LC_RadioLeaseRule rule : lease.m_aRules)
		{
			if (!rule.Allows(lease.m_Holder, lease.m_Radio, false))
				return GetRefusalName(rule.GetRefusal());
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the periodic check only while there is something to check
	protected void UpdateChecking()
	{
		bool wanted = !m_aLeases.IsEmpty();
		if (wanted == m_bChecking)
			return;

		m_bChecking = wanted;
		if (wanted)
			GetGame().GetCallqueue().CallLater(ServerCheck, CHECK_INTERVAL_MS, true);
		else
			GetGame().GetCallqueue().Remove(ServerCheck);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the leases into the game mode's replicated state and bumps it
	protected void Publish()
	{
		array<int> state = GetState();
		if (!state)
			return;

		state.Clear();
		foreach (LC_RadioLease lease : m_aLeases)
		{
			state.Insert(lease.m_RadioId);
			state.Insert(lease.m_iPlayerId);
			state.Insert(lease.m_eKind);
		}

		m_GameMode.LC_PublishRadioLeases();
	}

	//------------------------------------------------------------------------------------------------
	//! Seat changes are checked as they happen, not only at the next periodic check
	protected void Watch(notnull LC_RadioLease lease)
	{
		if (!lease.WatchesSeat())
			return;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(lease.m_Holder.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return;

		bool subscribed = IsWatched(access, lease);
		lease.m_SeatWatch = access;
		if (subscribed)
			return;

		access.GetOnCompartmentEntered().Insert(OnSeatEntered);
		access.GetOnCompartmentLeft().Insert(OnSeatLeft);
	}

	//------------------------------------------------------------------------------------------------
	protected void Unwatch(notnull LC_RadioLease lease)
	{
		SCR_CompartmentAccessComponent access = lease.m_SeatWatch;
		lease.m_SeatWatch = null;
		if (access && !IsWatched(access, lease))
			RemoveSeatHandlers(access);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveSeatHandlers(notnull SCR_CompartmentAccessComponent access)
	{
		ScriptInvoker entered = access.GetOnCompartmentEntered(false);
		if (entered)
			entered.Remove(OnSeatEntered);

		ScriptInvoker left = access.GetOnCompartmentLeft(false);
		if (left)
			left.Remove(OnSeatLeft);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether another held lease already listens to this character's seat events
	protected bool IsWatched(notnull SCR_CompartmentAccessComponent access, LC_RadioLease except)
	{
		foreach (LC_RadioLease lease : m_aLeases)
		{
			if (lease != except && lease.m_SeatWatch == access)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSeatEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		QueueCheck();
	}

	//------------------------------------------------------------------------------------------------
	//! A seat switch leaves one seat and enters the next; only the entry is checked, or moving between two
	//! allowed seats would drop the lease in between
	protected void OnSeatLeft(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (!move)
			QueueCheck();
	}

	//------------------------------------------------------------------------------------------------
	//! Checks after the seat event has finished, rather than changing subscriptions from inside it
	protected void QueueCheck()
	{
		if (m_bCheckQueued)
			return;

		m_bCheckQueued = true;
		GetGame().GetCallqueue().CallLater(ServerCheckQueued, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerCheckQueued()
	{
		m_bCheckQueued = false;
		ServerCheck();
	}
}
