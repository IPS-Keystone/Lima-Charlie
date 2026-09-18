//------------------------------------------------------------------------------------------------
//! Minimal JSON text helpers for the plugin bridge
class LC_Json
{
	//------------------------------------------------------------------------------------------------
	static string Escape(string value)
	{
		string escaped = value;
		escaped.Replace("\\", "\\\\");
		escaped.Replace("\"", "\\\"");
		return escaped;
	}

	//------------------------------------------------------------------------------------------------
	static string String(string value)
	{
		return "\"" + Escape(value) + "\"";
	}

	//------------------------------------------------------------------------------------------------
	static string Bool(bool value)
	{
		if (value)
			return "true";

		return "false";
	}

	//------------------------------------------------------------------------------------------------
	//! World position, decimetre precision: finer than that changes nothing audible and costs bytes every write
	static string Position(vector value)
	{
		return "[" + value[0].ToString(-1, 1) + "," + value[1].ToString(-1, 1) + "," + value[2].ToString(-1, 1) + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! Unit direction vector
	static string Direction(vector value)
	{
		return "[" + value[0].ToString(-1, 3) + "," + value[1].ToString(-1, 3) + "," + value[2].ToString(-1, 3) + "]";
	}
}
