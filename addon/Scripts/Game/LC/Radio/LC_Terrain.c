//------------------------------------------------------------------------------------------------
//! How terrain blocks the path between two radios, computed from terrain height queries
class LC_Terrain
{
	protected static const float SAMPLE_SPACING_M = 25;
	protected static const int MAX_SAMPLES = 128;
	//! Ground right next to either end is ignored, so standing on a slope does not block your own radio
	protected static const float END_MARGIN_M = 10;
	protected static const float MAX_HEIGHT_M = 250;
	//! The highest this many are kept; more than that between two radios is beyond what matters
	static const int MAX_HILLS = 8;

	//------------------------------------------------------------------------------------------------
	//! Every hill that rises above the straight line between the two ends, as its peak height above that
	//! line (up to 250 m), highest first. Ground dipping back below the line separates one hill from the
	//! next. Returns the highest, 0 with line of sight.
	//!
	//! Heights are the obstacle's own wherever it stands. TFAR measured how far the middle of the path would
	//! have to rise instead, which multiplies an obstacle near either end many times over: a ridge 100 m from
	//! you on a 10 km path counted 50 times its height, so almost any hill beside a long range radio took
	//! the whole penalty.
	static float Hills(vector from, vector to, notnull array<float> hills)
	{
		hills.Clear();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		float dx = to[0] - from[0];
		float dz = to[2] - from[2];
		float distance = Math.Sqrt(dx * dx + dz * dz);
		if (distance <= 2 * END_MARGIN_M)
			return 0;

		int samples = Math.Ceil(distance / SAMPLE_SPACING_M);
		samples = Math.ClampInt(samples, 2, MAX_SAMPLES);

		// Peak of the hill being crossed, 0 while the path is clear
		float peak = 0;
		for (int i = 1; i < samples; i++)
		{
			float t = i;
			t /= samples;

			float along = t * distance;
			if (along < END_MARGIN_M || distance - along < END_MARGIN_M)
				continue;

			float ground = world.GetSurfaceY(from[0] + dx * t, from[2] + dz * t);
			float above = ground - (from[1] + (to[1] - from[1]) * t);
			if (above > 0)
			{
				if (above > peak)
					peak = above;

				continue;
			}

			if (peak > 0)
				hills.Insert(Math.Min(peak, MAX_HEIGHT_M));

			peak = 0;
		}

		if (peak > 0)
			hills.Insert(Math.Min(peak, MAX_HEIGHT_M));

		if (hills.IsEmpty())
			return 0;

		hills.Sort(true);
		if (hills.Count() > MAX_HILLS)
			hills.Resize(MAX_HILLS);

		return hills[0];
	}
}
