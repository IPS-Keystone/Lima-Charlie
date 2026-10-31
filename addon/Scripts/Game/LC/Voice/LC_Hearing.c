//------------------------------------------------------------------------------------------------
//! How the local player's own hearing changes what Lima Charlie plays them in TeamSpeak, for another mod
//! to report: an ear protection headset, earplugs. Fill one in and hand it to LC_Hearing.SetLocalProfile.
//! Every field defaults to 0, and 0 means unchanged. The names are a contract other mods compile against:
//! fields may be added, never renamed or removed.
class LC_HearingProfile
{
	//! Nearby voices, at the listener's ear after walls and distance, and before the room's reverb
	float m_fDirectGainDb;		//!< added gain in dB, -40 .. +12
	float m_fDirectHighPassHz;	//!< 0 no filter, else 20 .. 20000
	float m_fDirectLowPassHz;	//!< 0 no filter, else 20 .. 20000
	float m_fDirectDrive;		//!< soft distortion, 0 none .. 1 heavy

	//! Radio voices, after the radio's own effect. Radio beeps are never touched.
	float m_fRadioGainDb;
	float m_fRadioHighPassHz;
	float m_fRadioLowPassHz;
	float m_fRadioDrive;
}

//------------------------------------------------------------------------------------------------
//! One mod's profile as it last reported it
class LC_HearingSource
{
	ref LC_HearingProfile m_Profile = new LC_HearingProfile();
	int m_iTick;
}

//------------------------------------------------------------------------------------------------
//! Collects what other mods report about the local player's hearing and hands the combination to the
//! plugin with the game state. Each mod reports under its own key, so clearing one never clears another.
class LC_Hearing
{
	//! A source not refreshed for this long is dropped, so a mod that stops reporting cannot leave its
	//! headset on the player for good
	static const int EXPIRY_MS = 1000;

	protected static ref map<string, ref LC_HearingSource> s_mSources;
	protected static string s_sLogged;

	//------------------------------------------------------------------------------------------------
	//! Sets or refreshes this source's profile for the local player; null clears this source only. The
	//! values are copied, so the caller may reuse or drop the object. Refresh at least once a second.
	//! Only clients use it: on a dedicated server it is kept but never read.
	static void SetLocalProfile(string source, LC_HearingProfile profile)
	{
		if (source.IsEmpty())
			return;

		if (!s_mSources)
			s_mSources = new map<string, ref LC_HearingSource>();

		if (!profile)
		{
			s_mSources.Remove(source);
			return;
		}

		LC_HearingSource entry = s_mSources.Get(source);
		if (!entry)
		{
			entry = new LC_HearingSource();
			s_mSources.Insert(source, entry);
		}

		LC_HearingProfile copy = entry.m_Profile;
		copy.m_fDirectGainDb = Sanitize(profile.m_fDirectGainDb, -40, 12);
		copy.m_fDirectHighPassHz = Sanitize(profile.m_fDirectHighPassHz, 20, 20000);
		copy.m_fDirectLowPassHz = Sanitize(profile.m_fDirectLowPassHz, 20, 20000);
		copy.m_fDirectDrive = Sanitize(profile.m_fDirectDrive, 0, 1);
		copy.m_fRadioGainDb = Sanitize(profile.m_fRadioGainDb, -40, 12);
		copy.m_fRadioHighPassHz = Sanitize(profile.m_fRadioHighPassHz, 20, 20000);
		copy.m_fRadioLowPassHz = Sanitize(profile.m_fRadioLowPassHz, 20, 20000);
		copy.m_fRadioDrive = Sanitize(profile.m_fRadioDrive, 0, 1);
		entry.m_iTick = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Every live source combined, or null when there is none. Gains add; filters stack in series, so the
	//! highest high-pass and the lowest low-pass win; the strongest drive wins. Drops expired sources.
	static LC_HearingProfile GetCombined(int now)
	{
		if (!s_mSources || s_mSources.IsEmpty())
			return null;

		LC_HearingProfile combined;
		array<string> expired = {};
		foreach (string source, LC_HearingSource entry : s_mSources)
		{
			if (now - entry.m_iTick > EXPIRY_MS)
			{
				expired.Insert(source);
				continue;
			}

			if (!combined)
				combined = new LC_HearingProfile();

			LC_HearingProfile profile = entry.m_Profile;
			combined.m_fDirectGainDb = combined.m_fDirectGainDb + profile.m_fDirectGainDb;
			combined.m_fDirectHighPassHz = Math.Max(combined.m_fDirectHighPassHz, profile.m_fDirectHighPassHz);
			combined.m_fDirectLowPassHz = LowestCutoff(combined.m_fDirectLowPassHz, profile.m_fDirectLowPassHz);
			combined.m_fDirectDrive = Math.Max(combined.m_fDirectDrive, profile.m_fDirectDrive);
			combined.m_fRadioGainDb = combined.m_fRadioGainDb + profile.m_fRadioGainDb;
			combined.m_fRadioHighPassHz = Math.Max(combined.m_fRadioHighPassHz, profile.m_fRadioHighPassHz);
			combined.m_fRadioLowPassHz = LowestCutoff(combined.m_fRadioLowPassHz, profile.m_fRadioLowPassHz);
			combined.m_fRadioDrive = Math.Max(combined.m_fRadioDrive, profile.m_fRadioDrive);
		}

		foreach (string expiredSource : expired)
		{
			s_mSources.Remove(expiredSource);
		}

		if (combined)
		{
			combined.m_fDirectGainDb = Math.Clamp(combined.m_fDirectGainDb, -40, 12);
			combined.m_fRadioGainDb = Math.Clamp(combined.m_fRadioGainDb, -40, 12);
		}

		return combined;
	}

	//------------------------------------------------------------------------------------------------
	//! The game state's "hearing" object for a combined profile, logged once whenever it changes
	static string BuildJson(notnull LC_HearingProfile profile)
	{
		string json = "{\"direct\":" + BuildPathJson(profile.m_fDirectGainDb, profile.m_fDirectHighPassHz, profile.m_fDirectLowPassHz, profile.m_fDirectDrive);
		json += ",\"radio\":" + BuildPathJson(profile.m_fRadioGainDb, profile.m_fRadioHighPassHz, profile.m_fRadioLowPassHz, profile.m_fRadioDrive) + "}";
		LogChange(json);
		return json;
	}

	//------------------------------------------------------------------------------------------------
	//! For a game state carrying no hearing at all
	static void LogUnchanged()
	{
		LogChange(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogChange(string json)
	{
		if (json == s_sLogged)
			return;

		s_sLogged = json;
		if (json.IsEmpty())
			Print("[LC] Hearing unchanged", LogLevel.NORMAL);
		else
			Print("[LC] Hearing " + json, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildPathJson(float gainDb, float highPassHz, float lowPassHz, float drive)
	{
		string json = "{\"gainDb\":" + gainDb.ToString(-1, 2);
		json += ",\"highPassHz\":" + highPassHz.ToString(-1, 0);
		json += ",\"lowPassHz\":" + lowPassHz.ToString(-1, 0);
		json += ",\"drive\":" + drive.ToString(-1, 3) + "}";
		return json;
	}

	//------------------------------------------------------------------------------------------------
	//! Anything that is not a finite number reads as unset (a NaN fails both comparisons), and 0 stays 0 so
	//! an unset field never switches a filter on at its lowest cut-off
	protected static float Sanitize(float value, float lowest, float highest)
	{
		if (!(value > -1000000 && value < 1000000) || value == 0)
			return 0;

		return Math.Clamp(value, lowest, highest);
	}

	//------------------------------------------------------------------------------------------------
	//! The lower of two low-pass cut-offs, where 0 is no filter rather than the lowest of all
	protected static float LowestCutoff(float a, float b)
	{
		if (a == 0)
			return b;

		if (b == 0)
			return a;

		return Math.Min(a, b);
	}
}
