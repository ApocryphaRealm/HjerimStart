#pragma once

// Settings for Alternate Perspective Hjerim Start. Plain-file INI beside the DLL (the project
// standard: redirector-proof, and readable by a player without a mod manager).
//
// Every value here is also a control on the mod's Apocrypha Menu Framework page, and every compiled
// default below is the value the shipped INI carries (rule 16) - pre-finalize-check compares them.

#include <cstdint>
#include <string>

namespace settings
{
	namespace debug
	{
		inline std::uint32_t logLevel = 0;  // uLogLevel:Debug - 0 is trace, the shipped default
	}

	namespace general
	{
		// bEnabled:General - with this off the start still appears in Alternate Perspective's menu (the
		// card is AP's, read from our JSON before the game runs) but choosing it does nothing, leaving
		// the player in the Resting Pilgrim. It exists so the start can be switched off without
		// uninstalling mid-playthrough.
		inline bool enabled = true;

		// fStartDelaySeconds:General - how long to wait after Alternate Perspective starts our quest
		// before acting. AP is still finishing its own hand-off in that moment (fade, freeze, watching
		// whether the start moves the player out of the start cell). A quarter of a second covers the
		// fade; too long trips AP's own safety net, which moves the player to the Helgen inn instead.
		inline float startDelaySeconds = 0.25f;

		// fStageGapSeconds:General - the beat between the parts of the start: the move, then the deed
		// and key, then each room's furniture appearing. Deliberately NOT one frame: a cross-cell
		// teleport and eight rooms of furniture enabling at once is a lot of 3D work for every mod that
		// rebuilds itself from the loaded cell (Civil War Starts crashed Faster HDT-SMP that way on its
		// first run, 2026-09-16). 0 puts everything back in one frame.
		inline float stageGapSeconds = 0.15f;
	}

	namespace start
	{
		inline bool moveToHjerim = true;               // bMoveToHjerim:Start
		inline bool furnishEveryRoom = true;           // bFurnishEveryRoom:Start
		inline bool cleanUpAfterBloodOnTheIce = true;  // bCleanUpAfterBloodOnTheIce:Start
	}

	void Init(const std::string& a_iniFileName);
	bool Reload();
	bool Save();
	void RestoreDefaults();
	void ApplyLogLevel();
	const std::string& GetIniPath();
}
