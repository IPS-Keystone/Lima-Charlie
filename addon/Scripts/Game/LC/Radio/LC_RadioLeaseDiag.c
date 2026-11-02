#ifdef ENABLE_DIAG
//------------------------------------------------------------------------------------------------
modded enum SCR_DebugMenuID
{
	LC_DEBUGUI_MENU,
	LC_DEBUGUI_LEASE_NEAREST,
	LC_DEBUGUI_LEASE_VEHICLE
}

//------------------------------------------------------------------------------------------------
//! Diag menu (Workbench and the diag executable only) to try radio leases before anything uses them:
//! "Lima Charlie > Lease nearest radio" asks for the nearest radio within a handset cable (a dropped manpack,
//! or one worn by someone nearby), "Lease this vehicle's radio" for a radio on the vehicle you sit in.
//! Unticking either gives back every lease you hold. Outcomes are in the log ("[LC] Radio lease ...").
class LC_RadioLeaseDiag
{
	protected bool m_bNearest;
	protected bool m_bVehicle;

	//! Search state for the sphere query callback
	protected IEntity m_Character;
	protected IEntity m_Best;
	protected float m_fBestDistanceSq;

	//------------------------------------------------------------------------------------------------
	void LC_RadioLeaseDiag()
	{
		DiagMenu.RegisterMenu(SCR_DebugMenuID.LC_DEBUGUI_MENU, "Lima Charlie", "");
		DiagMenu.RegisterBool(SCR_DebugMenuID.LC_DEBUGUI_LEASE_NEAREST, "", "Lease nearest radio", "Lima Charlie");
		DiagMenu.RegisterBool(SCR_DebugMenuID.LC_DEBUGUI_LEASE_VEHICLE, "", "Lease this vehicle's radio", "Lima Charlie");
	}

	//------------------------------------------------------------------------------------------------
	void Update(IEntity character)
	{
		bool nearest = DiagMenu.GetBool(SCR_DebugMenuID.LC_DEBUGUI_LEASE_NEAREST);
		bool vehicle = DiagMenu.GetBool(SCR_DebugMenuID.LC_DEBUGUI_LEASE_VEHICLE);
		if (nearest == m_bNearest && vehicle == m_bVehicle)
			return;

		LC_RadioLeases leases = LC_RadioLeases.Get();
		if (!leases || !character)
			return;

		if (nearest && !m_bNearest)
			RequestNearest(leases, character);

		if (vehicle && !m_bVehicle)
			RequestVehicle(leases, character);

		if ((!nearest && m_bNearest) || (!vehicle && m_bVehicle))
		{
			Print("[LC] Diag: releasing all radio leases", LogLevel.NORMAL);
			leases.ReleaseAll();
		}

		m_bNearest = nearest;
		m_bVehicle = vehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestNearest(notnull LC_RadioLeases leases, notnull IEntity character)
	{
		m_Character = character;
		m_Best = null;
		float radius = LC_RadioLeases.HANDSET_CABLE_M;
		m_fBestDistanceSq = radius * radius;
		GetGame().GetWorld().QueryEntitiesBySphere(character.GetOrigin(), radius, AddCandidate, null, EQueryEntitiesFlags.ALL);

		if (!m_Best)
		{
			Print(string.Format("[LC] Diag: no radio within %1 m", radius), LogLevel.NORMAL);
			return;
		}

		Print(string.Format("[LC] Diag: requesting lease of %1 (radio %2, %3 m)", m_Best, LC_RadioLeases.GetRadioId(m_Best), Math.Sqrt(m_fBestDistanceSq)), LogLevel.NORMAL);
		leases.Request(m_Best, LC_ERadioLeaseKind.BORROWED);
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle itself if it has a radio (vanilla cars carry an unused one), else its first radio child
	protected void RequestVehicle(notnull LC_RadioLeases leases, notnull IEntity character)
	{
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(character);
		if (!vehicle)
		{
			Print("[LC] Diag: not in a vehicle", LogLevel.NORMAL);
			return;
		}

		IEntity radio = vehicle;
		if (!radio.FindComponent(BaseRadioComponent))
		{
			radio = vehicle.GetChildren();
			while (radio && !radio.FindComponent(BaseRadioComponent))
			{
				radio = radio.GetSibling();
			}
		}

		if (!radio)
		{
			Print("[LC] Diag: this vehicle has no radio", LogLevel.NORMAL);
			return;
		}

		Print(string.Format("[LC] Diag: requesting vehicle lease of radio %1", LC_RadioLeases.GetRadioId(radio)), LogLevel.NORMAL);
		leases.Request(radio, LC_ERadioLeaseKind.VEHICLE);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddCandidate(IEntity entity)
	{
		Consider(entity);

		// A manpack someone wears is part of their character, so look at the backpacks of people found
		if (ChimeraCharacter.Cast(entity))
			Consider(LC_Radio.GetWornBackpack(entity));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Consider(IEntity entity)
	{
		if (!entity || !entity.FindComponent(BaseRadioComponent) || entity.GetRootParent() == m_Character)
			return;

		float distanceSq = vector.DistanceSq(LC_RadioLeaseRule.GetWorldPosition(entity), m_Character.GetOrigin());
		if (distanceSq > m_fBestDistanceSq)
			return;

		m_Best = entity;
		m_fBestDistanceSq = distanceSq;
	}
}
#endif
