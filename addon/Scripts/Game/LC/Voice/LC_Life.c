//------------------------------------------------------------------------------------------------
//! What a character's state allows them to do with their voice. Hearing and speaking are not the same
//! question: someone bleeding out on the floor still listens to the people standing over them, but they
//! do not join in.
class LC_Life
{
	//------------------------------------------------------------------------------------------------
	//! Whether this character can be heard speaking out loud. Unconscious is not dead, but it is not
	//! talking either.
	static bool CanSpeak(IEntity entity)
	{
		return GetLifeState(entity) == ECharacterLifeState.ALIVE;
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
