//------------------------------------------------------------------------------------------------
//! Per scenario settings, carried on the mission header the way ACE and the vanilla game modes do it.
//! A scenario that fills this in overrides Configs/LC/Settings.conf for that scenario only.
//!
//! SCR_MissionHeader is vanilla and declares no [BaseContainerProps()], so this modded version must not
//! add one either: the attribute is repeated exactly as the original class declares it.
modded class SCR_MissionHeader : MissionHeader
{
	[Attribute(desc: "Lima Charlie settings for this scenario. Left out, the settings shipped in Configs/LC/Settings.conf are used instead.")]
	protected ref LC_Settings m_LC_Settings;

	//------------------------------------------------------------------------------------------------
	//! Null unless this scenario carries Lima Charlie settings of its own
	LC_Settings LC_GetSettings()
	{
		return m_LC_Settings;
	}
}
