//------------------------------------------------------------------------------------------------
//! Selectable direct speech range, TFAR style. The local player's plugin announces the range to other
//! players' plugins through TeamSpeak, so their games do not need to know it.
class LC_VoiceLevel
{
	static const string ACTION_CYCLE = "LC_VoiceLevelCycle";

	static const int WHISPER = 0;
	static const int NORMAL = 1;
	static const int SHOUT = 2;
	protected static const int LEVEL_COUNT = 3;

	protected static int s_iLevel = NORMAL;

	//------------------------------------------------------------------------------------------------
	//! Range in metres at which the local player can still be heard
	static float GetRange()
	{
		switch (s_iLevel)
		{
			case WHISPER:
				return 5;

			case SHOUT:
				return 60;
		}

		return 20;
	}

	//------------------------------------------------------------------------------------------------
	//! Advances to the next level and returns it
	static int Cycle()
	{
		s_iLevel = (s_iLevel + 1) % LEVEL_COUNT;
		return s_iLevel;
	}
}
