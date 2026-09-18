//------------------------------------------------------------------------------------------------
//! TFAR's terrain interception for radio signals, computed from terrain height queries
class LC_Terrain
{
	protected static const float SAMPLE_SPACING_M = 25;
	protected static const int MAX_SAMPLES = 128;
	//! Ground right next to either end is ignored, so standing on a slope does not block your own radio
	protected static const float END_MARGIN_M = 10;
	protected static const float MIN_CLEARANCE_M = 10;
	protected static const float MAX_CLEARANCE_M = 250;

	//------------------------------------------------------------------------------------------------
	//! How far the middle of the path must rise for both halves to clear the terrain, like
	//! TFAR_fnc_calcTerrainInterception: 0 with line of sight, otherwise 10 to 250 m.
	static float Clearance(vector from, vector to)
	{
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

		float middleY = (from[1] + to[1]) * 0.5;
		bool blocked;
		float required = -MAX_CLEARANCE_M;
		for (int i = 1; i < samples; i++)
		{
			float t = i;
			t /= samples;

			float along = t * distance;
			if (along < END_MARGIN_M || distance - along < END_MARGIN_M)
				continue;

			float ground = world.GetSurfaceY(from[0] + dx * t, from[2] + dz * t);
			if (ground > from[1] + (to[1] - from[1]) * t)
				blocked = true;

			// Height the raised middle point needs so the half of the path over this sample clears it
			float needed;
			if (t <= 0.5)
				needed = (ground - from[1]) / (2 * t) - (middleY - from[1]);
			else
				needed = (ground - to[1]) / (2 * (1 - t)) - (middleY - to[1]);

			if (needed > required)
				required = needed;
		}

		if (!blocked)
			return 0;

		return Math.Clamp(required, MIN_CLEARANCE_M, MAX_CLEARANCE_M);
	}
}
