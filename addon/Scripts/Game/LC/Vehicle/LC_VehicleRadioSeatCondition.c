//------------------------------------------------------------------------------------------------
//! Control hint condition: the local player sits in a vehicle seat that reaches the vehicle's radio
[BaseContainerProps()]
class LC_VehicleRadioSeatCondition : SCR_AvailableActionCondition
{
	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(notnull SCR_AvailableActionsConditionData data)
	{
		return GetReturnResult(LC_VehicleSeat.FindSeatRadio(SCR_PlayerController.GetLocalControlledEntity()) != null);
	}
}
