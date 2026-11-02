//------------------------------------------------------------------------------------------------
//! A vehicle intercom channel. The values are the bits game_state.json carries in self.ic and players[].ic.
//! One channel at a time; any seat may pick any of them.
enum LC_EIntercomChannel
{
	NONE = 0,
	CREW = 1,
	CARGO = 2,
	MISC = 4
}

//------------------------------------------------------------------------------------------------
class LC_Intercom
{
	//! The order the radio menu entry cycles through
	protected static const ref array<LC_EIntercomChannel> CYCLE = { LC_EIntercomChannel.NONE, LC_EIntercomChannel.CREW, LC_EIntercomChannel.CARGO, LC_EIntercomChannel.MISC };

	//------------------------------------------------------------------------------------------------
	static bool IsChannel(int channel)
	{
		return CYCLE.Contains(channel);
	}

	//------------------------------------------------------------------------------------------------
	//! The channel after (step 1) or before (step -1) this one, wrapping round
	static LC_EIntercomChannel Step(LC_EIntercomChannel channel, int step)
	{
		int index = CYCLE.Find(channel);
		if (index < 0)
			return LC_EIntercomChannel.NONE;

		int count = CYCLE.Count();
		index = (index + step + count) % count;
		return CYCLE[index];
	}

	//------------------------------------------------------------------------------------------------
	static string GetChannelName(LC_EIntercomChannel channel)
	{
		switch (channel)
		{
			case LC_EIntercomChannel.CREW:
				return "Crew";
			case LC_EIntercomChannel.CARGO:
				return "Cargo";
			case LC_EIntercomChannel.MISC:
				return "Misc";
		}

		return "No Intercom";
	}
}

//------------------------------------------------------------------------------------------------
//! The intercom in the VON radial menu: one entry that cycles No Intercom, Crew, Cargo and Misc on scroll.
//! It has no transmit key, since the intercom carries direct speech; LC's modded SCR_VONController refuses to
//! make it the active entry, so picking it never turns the radio key into direct speech. LC_VehicleSeat adds
//! it last while the local player is on a vehicle's intercom and removes it when they leave.
class LC_VONEntryIntercom : SCR_VONEntry
{
	protected static const ResourceName LAYOUT = "{033302D7C8158EF8}UI/layouts/HUD/VON/VONEntry.layout";

	//! Weak: the reconciler owns this entry
	protected LC_VehicleSeat m_Seat;

	//------------------------------------------------------------------------------------------------
	void LC_VONEntryIntercom(LC_VehicleSeat seat)
	{
		m_Seat = seat;
		m_sText = GetChannelText();
	}

	//------------------------------------------------------------------------------------------------
	override void InitEntry()
	{
		super.InitEntry();
		SetCustomLayout(LAYOUT);
		m_sText = GetChannelText();
		SetName(m_sText);
	}

	//------------------------------------------------------------------------------------------------
	//! Scroll in the menu
	override void AdjustEntryModif(int modifier)
	{
		Cycle(modifier);
	}

	//------------------------------------------------------------------------------------------------
	//! The cycle-channel input in the menu
	override void AdjustEntry(int modifier)
	{
		Cycle(modifier);
	}

	//------------------------------------------------------------------------------------------------
	//! The menu's action key steps forward too, so the entry does something whichever key is pressed
	override void ToggleEntry()
	{
		Cycle(1);
	}

	//------------------------------------------------------------------------------------------------
	override string GetDisplayText()
	{
		return GetChannelText();
	}

	//------------------------------------------------------------------------------------------------
	override void Update()
	{
		super.Update();

		SCR_VONEntryComponent entryComp = SCR_VONEntryComponent.Cast(m_EntryComponent);
		if (!entryComp)
			return;

		bool on = GetChannel() != LC_EIntercomChannel.NONE;
		entryComp.SetTransceiverText("IC");
		entryComp.SetFrequencyText(GetChannelText());
		entryComp.SetChannelText("Intercom");
		entryComp.SetActiveIcon(false);
		entryComp.SetMuteIcon(!on);
		entryComp.SetPowerIcon(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cycle(int modifier)
	{
		if (!m_Seat || modifier == 0)
			return;

		int step = 1;
		if (modifier < 0)
			step = -1;

		m_Seat.SetIntercomChannel(LC_Intercom.Step(GetChannel(), step));
		m_sText = GetChannelText();
		SetName(m_sText);
		LC_Client.PlayUiSound("cycle");
	}

	//------------------------------------------------------------------------------------------------
	protected LC_EIntercomChannel GetChannel()
	{
		if (!m_Seat)
			return LC_EIntercomChannel.NONE;

		return m_Seat.GetIntercomChannel();
	}

	//------------------------------------------------------------------------------------------------
	protected string GetChannelText()
	{
		return LC_Intercom.GetChannelName(GetChannel());
	}
}
