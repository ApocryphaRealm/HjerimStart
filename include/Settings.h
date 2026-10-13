#pragma once

// Settings for Alternate Perspective Hjerim Start. Plain-file INI beside the DLL (the project
// standard: redirector-proof, and readable by a player without a mod manager).
//
// There is no in-game page (the owner, 2026-10-02: Alternate Perspective add-ons are simple mods with no
// AMF page); the INI is the only place these are set. Every compiled default below is the value the
// shipped INI carries (rule 16) - pre-finalize-check compares them.

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
		// bEnabled:General - with this off the start still appears in Alternate Perspective's menu (it is
		// added to AP's "My own Home" card when the menu opens) but choosing it does nothing, leaving
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

		// uBloodOnTheIce:Start - what the start does about the murder investigation this house belongs to
		// (the owner, 2026-09-18: "hjerim should either deactivate the blood on the ice quest or set it as
		// complete"). 0 leaves it alone and the Butcher's clutter stays until the case is closed the usual
		// way; 1 marks it complete, which is the state the game itself reaches when the case is closed and
		// is what makes the house sellable in vanilla; 2 stops it, the lighter touch - nothing is claimed in
		// the journal, but a Windhelm trigger could start it again. Either of 1 and 2 clears the clutter.
		inline constexpr std::uint32_t kBotiLeave = 0, kBotiComplete = 1, kBotiStop = 2;
		inline std::uint32_t bloodOnTheIce = kBotiComplete;

		inline bool cleanUpAfterBloodOnTheIce = true;  // bCleanUpAfterBloodOnTheIce:Start
	}

	void Init(const std::string& a_iniFileName);
	bool Reload();
	bool Save();
	void ApplyLogLevel();
	const std::string& GetIniPath();
}
