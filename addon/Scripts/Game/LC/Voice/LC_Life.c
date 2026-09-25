//------------------------------------------------------------------------------------------------
//! What a character's state allows them to do with their voice. Hearing and speaking are not the same
//! question: someone bleeding out on the floor still listens to the people standing over them, but they
//! do not join in.
class LC_Life
{
	//! Server setting, delivered with the session: whether an unconscious character can still be heard.
	//! Off by default, so bleeding out on the floor is silent.
	protected static bool s_bUnconsciousCanSpeak;

	//------------------------------------------------------------------------------------------------
	//! Set on the server as the settings resolve, and on each client as the session arrives
	static void SetUnconsciousCanSpeak(bool canSpeak)
	{
		s_bUnconsciousCanSpeak = canSpeak;
	}

	//------------------------------------------------------------------------------------------------
	static bool GetUnconsciousCanSpeak()
	{
		return s_bUnconsciousCanSpeak;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this character can be heard speaking out loud. Unconscious is not dead, but by default it
	//! is not talking either; the server can allow it.
	static bool CanSpeak(IEntity entity)
	{
		ECharacterLifeState state = GetLifeState(entity);
		if (state == ECharacterLifeState.ALIVE)
			return true;

		// Radios stay out of reach either way: vanilla's own activation refuses anything but direct speech
		// while incapacitated, and that rule is kept.
		return s_bUnconsciousCanSpeak && state == ECharacterLifeState.INCAPACITATED;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this character hears anything at all. An unconscious one does; a dead one does not.
	static bool CanHear(IEntity entity)
	{
		return GetLifeState(entity) != ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	//! DEAD for anything that is not a character with a controller, so a prop or a camera neither speaks
	//! nor hears on its own account
	protected static ECharacterLifeState GetLifeState(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return ECharacterLifeState.DEAD;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (!controller)
			return ECharacterLifeState.DEAD;

		return controller.GetLifeState();
	}
}
