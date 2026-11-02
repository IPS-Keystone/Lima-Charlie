[ComponentEditorProps(category: "Lima Charlie", description: "Lima Charlie infantry phone: a handset outside the vehicle that puts whoever holds it on the vehicle's intercom. Needs LC_VehicleCommsComponent (with its intercom) on the same vehicle, and LC_InfantryPhoneAction in the named action context of the vehicle's ActionsManagerComponent.")]
class LC_InfantryPhoneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A vehicle's infantry phone. Holding it is a PHONE radio lease on the vehicle (LC_RadioLeases), measured
//! from the phone, which puts the holder on the vehicle's intercom (LC_VehicleCommsComponent.GetPhoneChannel,
//! LC_VehicleSeat.FindPhoneIntercom). Data only: a modded vehicle gets a phone from one prefab override that
//! adds this component and LC_InfantryPhoneAction, no script.
class LC_InfantryPhoneComponent : ScriptComponent
{
	[Attribute("push_rear", desc: "Action context (ContextName in the vehicle's ActionsManagerComponent) the phone hangs at. Put LC_InfantryPhoneAction in the same context; the cable is measured from here.", category: "Infantry phone")]
	protected string m_sContextName;

	[Attribute("5", UIWidgets.Slider, "How far the holder may walk from the phone before it is hung up (m)", "1 30 0.5", category: "Infantry phone")]
	protected float m_fCableLength;

	[Attribute("4", UIWidgets.ComboBox, "Intercom channel the holder starts on; they can pick another from the radio menu like any seat", "", ParamEnumArray.FromEnum(LC_EIntercomChannel), category: "Infantry phone")]
	protected LC_EIntercomChannel m_eChannel;

	//------------------------------------------------------------------------------------------------
	//! The phone of the vehicle an entity belongs to, or null
	static LC_InfantryPhoneComponent Find(IEntity entity)
	{
		IEntity vehicle = LC_RadioLeaseSeatRule.FindVehicle(entity);
		if (!vehicle)
			return null;

		return LC_InfantryPhoneComponent.Cast(vehicle.FindComponent(LC_InfantryPhoneComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The intercom the phone is wired to, or null when the vehicle has none (no comms component, intercom
	//! off, or a civilian vehicle with a military-only intercom): then the phone does nothing
	LC_VehicleCommsComponent GetIntercom()
	{
		LC_VehicleCommsComponent comms = LC_VehicleCommsComponent.Cast(GetOwner().FindComponent(LC_VehicleCommsComponent));
		if (!comms || !comms.HasIntercom())
			return null;

		return comms;
	}

	//------------------------------------------------------------------------------------------------
	//! The channel the holder starts on (a misconfigured NONE counts as MISC, or the phone would be dead)
	LC_EIntercomChannel GetChannel()
	{
		if (m_eChannel == LC_EIntercomChannel.NONE || !LC_Intercom.IsChannel(m_eChannel))
			return LC_EIntercomChannel.MISC;

		return m_eChannel;
	}

	//------------------------------------------------------------------------------------------------
	float GetCableLength()
	{
		return m_fCableLength;
	}

	//------------------------------------------------------------------------------------------------
	string GetContextName()
	{
		return m_sContextName;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the phone hangs: the action context's position, or the vehicle's origin if the context is missing
	vector GetPhonePosition()
	{
		IEntity owner = GetOwner();
		BaseActionsManagerComponent actions = BaseActionsManagerComponent.Cast(owner.FindComponent(BaseActionsManagerComponent));
		if (actions)
		{
			UserActionContext context = actions.GetContext(m_sContextName);
			vector transform[4];
			if (context && context.GetTransformationWorld(transform))
				return transform[3];
		}

		return owner.GetOrigin();
	}
}

//------------------------------------------------------------------------------------------------
//! The infantry phone's cable: the holder stays within its length (the constructor's distance) of the phone,
//! not of the vehicle's origin. The leased "radio" is the vehicle, so the phone is found on it. (No
//! constructor of its own: Enforce wants a subclass constructor to take the parent's argument types.)
class LC_RadioLeasePhoneRule : LC_RadioLeaseDistanceRule
{
	//------------------------------------------------------------------------------------------------
	protected override vector GetAnchor(notnull IEntity radio)
	{
		LC_InfantryPhoneComponent phone = LC_InfantryPhoneComponent.Cast(radio.FindComponent(LC_InfantryPhoneComponent));
		if (!phone)
			return GetWorldPosition(radio);

		return phone.GetPhonePosition();
	}
}

//------------------------------------------------------------------------------------------------
//! Picks up or hangs up a vehicle's infantry phone. Goes in the vehicle's ActionsManagerComponent
//! additionalActions with ParentContextList set to the phone component's context. Only the performing
//! player's machine runs it: it asks the server for (or gives back) the PHONE lease, and the server decides.
class LC_InfantryPhoneAction : ScriptedUserAction
{
	protected static const string NAME_PICK_UP = "Pick Up Infantry Phone";
	protected static const string NAME_HANG_UP = "Hang Up Infantry Phone";

	//! Weak: the vehicle owns it
	protected LC_InfantryPhoneComponent m_Phone;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Phone = LC_InfantryPhoneComponent.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Phone || !m_Phone.GetIntercom())
			return false;

		// From outside only; inside, the seat already puts the player on the intercom
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || !LC_Life.CanHear(character))
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		return !access || !access.GetCompartment();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (IsHeldByLocalPlayer())
			outName = NAME_HANG_UP;
		else
			outName = NAME_PICK_UP;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Phone || SCR_PlayerController.GetLocalControlledEntity() != pUserEntity)
			return;

		LC_RadioLeases leases = LC_RadioLeases.Get();
		if (!leases)
			return;

		IEntity vehicle = m_Phone.GetOwner();
		if (IsHeldByLocalPlayer())
		{
			leases.Release(vehicle);
			return;
		}

		// One phone at a time: picking this one up hangs up any other
		RplId vehicleId = LC_RadioLeases.GetRadioId(vehicle);
		array<RplId> radios = {};
		array<int> kinds = {};
		leases.GetOwnLeases(radios, kinds);
		foreach (int i, RplId radioId : radios)
		{
			if (kinds[i] == LC_ERadioLeaseKind.PHONE && radioId != vehicleId)
				leases.ReleaseById(radioId);
		}

		leases.Request(vehicle, LC_ERadioLeaseKind.PHONE);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHeldByLocalPlayer()
	{
		LC_RadioLeases leases = LC_RadioLeases.Get();
		if (!m_Phone || !leases)
			return false;

		return leases.GetOwnKind(LC_RadioLeases.GetRadioId(m_Phone.GetOwner())) == LC_ERadioLeaseKind.PHONE;
	}
}
