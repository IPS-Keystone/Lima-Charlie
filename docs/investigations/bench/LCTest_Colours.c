// Dev-only test of Lima Charlie channel label colours. ClaudeDevTools is never published. Temporary.
// Called via NET API: APIFunc = "LCTest_Colours"

class LCTest_ColoursRequest : JsonApiStruct
{
}

class LCTest_ColoursResponse : JsonApiStruct
{
	string results;

	void LCTest_ColoursResponse()
	{
		RegV("results");
	}
}

class LCTest_Colours : NetApiHandler
{
	protected string m_sOut;
	protected int m_iFailed;

	override JsonApiStruct GetRequest()
	{
		return new LCTest_ColoursRequest();
	}

	protected void Check(string what, string got, string expected)
	{
		string verdict = "ok  ";
		if (got != expected)
		{
			verdict = "FAIL";
			m_iFailed++;
		}

		m_sOut += verdict + " " + what + ": '" + got + "' expected '" + expected + "'\n";
	}

	protected string Rgb(LC_ChannelLabel label)
	{
		if (!label)
			return "null";

		int colour = label.m_eColour;
		return colour.ToString() + " " + label.m_iRed.ToString() + "," + label.m_iGreen.ToString() + "," + label.m_iBlue.ToString() + " dark=" + LC_ChannelLabels.IsDark(label).ToString();
	}

	override JsonApiStruct GetResponse(JsonApiStruct request)
	{
		// server.json text -> packed
		string packed = LC_ChannelLabels.PackFromText("50,#ff0000,PLATOON;52,#49B3FF,AIR;60,YELLOW,COMMAND;61,#12345,BAD;62,#GGGGGG,BAD2;63,3,NUMBER");
		Check("PackFromText", packed, "50000,#FF0000,PLATOON;52000,#49B3FF,AIR;60000,3,COMMAND;61000,0,BAD;62000,0,BAD2;63000,3,NUMBER;");
		Check("UnpackToText", LC_ChannelLabels.UnpackToText(packed), "50.000,#FF0000,PLATOON;52.000,#49B3FF,AIR;60.000,YELLOW,COMMAND;61.000,WHITE,BAD;62.000,WHITE,BAD2;63.000,YELLOW,NUMBER");

		// packed -> client labels
		LC_ChannelLabels.Unpack(packed);
		Check("50 MHz", Rgb(LC_ChannelLabels.Find(50000)), "8 255,0,0 dark=true");
		Check("52 MHz", Rgb(LC_ChannelLabels.Find(52000)), "8 73,179,255 dark=false");
		Check("60 MHz", Rgb(LC_ChannelLabels.Find(60000)), "3 226,201,41 dark=false");
		Check("61 MHz", Rgb(LC_ChannelLabels.Find(61000)), "0 255,255,255 dark=false");

		// every palette entry keeps its old white/black split (RED, BLUE, PURPLE dark)
		string split;
		for (int colour = 0; colour < 8; colour++)
		{
			LC_ChannelLabel palette = new LC_ChannelLabel();
			int red;
			int green;
			int blue;
			LC_ChannelLabels.GetPaletteRgb(colour, red, green, blue);
			palette.m_iRed = red;
			palette.m_iGreen = green;
			palette.m_iBlue = blue;
			if (LC_ChannelLabels.IsDark(palette))
				split += "1";
			else
				split += "0";
		}
		Check("palette dark split W R O Y G C B P", split, "01000011");

		// Workbench colour picker (linear) -> hex: should come back as the sRGB it was made from
		array<ref LC_ChannelLabel> configured = {};
		LC_ChannelLabel custom = new LC_ChannelLabel();
		custom.m_fFrequencyMHz = 45.5;
		custom.m_sName = "PICKED";
		custom.m_eColour = LC_EChannelColour.CUSTOM;
		custom.m_CustomColour = Color.FromSRGBA(73, 179, 255, 255);
		configured.Insert(custom);
		LC_ChannelLabel noColour = new LC_ChannelLabel();
		noColour.m_fFrequencyMHz = 46;
		noColour.m_sName = "NOPICK";
		noColour.m_eColour = LC_EChannelColour.CUSTOM;
		configured.Insert(noColour);
		LC_ChannelLabel named = new LC_ChannelLabel();
		named.m_fFrequencyMHz = 47;
		named.m_sName = "NAMED";
		named.m_eColour = LC_EChannelColour.BLUE;
		configured.Insert(named);
		Check("Pack picker", LC_ChannelLabels.Pack(configured), "45500,#49B3FF,PICKED;46000,0,NOPICK;47000,6,NAMED;");

		LC_ChannelLabels.Unpack(string.Empty);

		LCTest_ColoursResponse response = new LCTest_ColoursResponse();
		response.results = m_sOut + "failed " + m_iFailed.ToString();
		return response;
	}
}
