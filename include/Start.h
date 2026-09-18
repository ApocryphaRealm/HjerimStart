#pragma once

// The start sequence itself.
//
// Alternate Perspective owns the menu and the choosing; this mod owns exactly one moment - the one
// where AP starts the quest our JSON named. From there the mod puts the player in Hjerim's main hall
// and hands the house over the way the steward's own sale and room purchases would have, all at once.
// Nothing else in the game is touched.

#include <cstddef>
#include <string>

namespace start
{
	// Resolve our quest and start listening for it starting. Called once at kDataLoaded; safe to call
	// again (it will not register twice).
	void Install();

	// Called when a game begins (new or loaded). Catches up the Blood on the Ice clean-up if the case
	// was closed while this mod was not watching (a save made without it, an update).
	void OnGameBegun();

	// Run the sequence now. Called by the quest-start listener after the configured delay, and by the
	// DevBench tool and the settings page so the whole thing can be exercised without going through the
	// menu. Returns false when it could not run at all (no player, disabled in the settings); a step that
	// is switched off is not a failure.
	bool Run(const char* a_reason);

	// What the last run did, for the settings page and the DevBench tool.
	struct Report
	{
		bool ran = false;
		std::string reason;
		bool moved = false;
		bool owned = false;      // the cell now belongs to the player's faction
		bool keyGiven = false;
		bool guideGiven = false;
		bool scriptVarSet = false;
		bool bloodOnTheIce = false;  // the murder investigation was completed or stopped, as the setting says
		int roomsFurnished = 0;
		std::string problem;  // empty when everything asked for actually happened
	};

	const Report& LastRun();

	// True once our quest has been seen to start in this session - i.e. the player really did pick this
	// start. The settings page says so, because "nothing happened" and "you did not choose this start"
	// look identical from the player's side otherwise.
	bool WasChosen();

	// True in this GAME (persisted in the SKSE co-save) once the start has handed the house over. It is
	// what decides whether the Blood on the Ice clean-up belongs to us: a player who bought Hjerim from
	// the steward keeps the steward's own "clean up the mess" purchase.
	bool StartRanInThisGame();

	// Co-save hooks, registered by main.cpp.
	void OnSave(SKSE::SerializationInterface* a_intfc);
	void OnLoad(SKSE::SerializationInterface* a_intfc);
	void OnRevert(SKSE::SerializationInterface* a_intfc);

	std::string StateJson();
}
