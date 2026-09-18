//------------------------------------------------------------------------------------------------
//! Whether a radio can listen while it is talking
enum LC_ERadioMode
{
	//! Hears incoming traffic even while transmitting, which is how the game behaves on its own
	FULL_DUPLEX,
	//! Deaf on that radio for as long as you hold the key, like a real single frequency set
	HALF_DUPLEX
}

//------------------------------------------------------------------------------------------------
//! Adds the duplex mode to the radio gadget component.
//!
//! SCR_RadioComponent is scripted and already sits in Prefabs/Items/Core/Radio_base.et, so every radio
//! that derives from it inherits this attribute without any prefab being overridden. Several mods can add
//! their own attributes to the same component, which overriding Radio_base.et would not allow.
//!
//! The default is FULL_DUPLEX, so nothing changes until someone sets it on a radio's prefab.
//! Static radio stations derive from Props_Base.et and have no SCR_RadioComponent, so they are unaffected.
modded class SCR_RadioComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Whether this radio can receive while it is transmitting. Half duplex goes deaf on this radio while its transmit key is held; other radios you carry keep working.", "", ParamEnumArray.FromEnum(LC_ERadioMode), category: "Radio")]
	protected LC_ERadioMode m_eLC_RadioMode;

	//------------------------------------------------------------------------------------------------
	LC_ERadioMode LC_GetRadioMode()
	{
		return m_eLC_RadioMode;
	}
}
