#pragma once

// Alternate Perspective Hjerim Start.
//
// THE WHOLE MOD IS THIS TABLE PLUS THE CODE THAT READS IT. Every FormID below was read out of the
// game's own Skyrim.esm on 2026-09-18 (scratchpad esm-scan.py over the record headers, decompressed
// where the record was) and cross-checked against the vanilla scripts that use them
// (HousePurchaseScript.psc, QF_HousePurchase_000A7B33.psc, QF_MS11_0001F7A3.psc, the TIF__ fragments of
// the Windhelm steward's decorate topics). Nothing here is recalled.
//
// The owner, 2026-09-16: "make an alternate perspective mod that adds hjerim house in windhelm as a
// starting home option". 2026-09-18: "It should be fully furnished. And there's no need to involve the
// steward." and "it should be compatible with mods that refurnish or improve the house."
//
// So the plugin carries ONE quest record and edits no cell, reference or global. Everything is done at
// runtime through the same handles the steward's own dialogue fragments use - the markers the
// furniture is enable-parented to, the HousePurchase quest's script variable, the cell's owner - which
// is why a house overhaul that survives the vanilla purchase survives this start too.

#include <cstddef>
#include <cstdint>

namespace hjerim
{
	inline constexpr const char* kLogName = "HjerimStart";
	inline constexpr const char* kDisplayName = "Alternate Perspective Hjerim Start";
	inline constexpr const char* kIniName = "HjerimStart.ini";
	inline constexpr const char* kPluginName = "Alternate Perspective Hjerim Start.esp";
	inline constexpr const char* kTranslationStem = "HjerimStart";

	// Our own quest, the one Alternate Perspective starts. LOCAL FormID inside our plugin - the index
	// depends on the player's load order and is resolved at runtime, never baked in here.
	inline constexpr std::uint32_t kStartQuestLocalID = 0x800;

	// ---- The vanilla purchase, piece by piece (QF_HousePurchase Fragment_7 + HousePurchaseScript) ----
	inline constexpr std::uint32_t kHjerimCell = 0x00016778;        // CELL WindhelmHjerim
	inline constexpr std::uint32_t kPlayerFaction = 0x00000DB1;     // FACT PlayerFaction
	inline constexpr std::uint32_t kHouseKey = 0x00021679;          // KEYM MS11HjerimKey - the key the steward hands over
	inline constexpr std::uint32_t kDecoratingGuide = 0x000F1446;   // BOOK HouseDecorateWindhelm - "Hjerim decorating guide"
	inline constexpr std::uint32_t kHousePurchaseQuest = 0x000A7B33;  // QUST HousePurchase, script HousePurchaseScript
	inline constexpr const char* kHousePurchaseScript = "HousePurchaseScript";
	inline constexpr const char* kWindhelmHouseVar = "WindhelmHouseVar";  // 1 after the sale, 2 after the first door use
	inline constexpr std::int32_t kWindhelmHouseVarOwned = 2;

	// ---- The house itself ---------------------------------------------------------------------------
	// Enable-parent markers, all persistent XMarkers standing in WindhelmHjerim at z=192. The steward's
	// room fragments do exactly `decoratemarker.enable()` on one of these each; Blood on the Ice's
	// resolution does `HjerimAbandoned.Disable()`, `WindhelmPlayerHouseDecorateBasic.Enable()` and
	// `WindhelmHjerimPlayerChestEnable.Enable()`. HDWindhelm* globals hold PRICES only - the steward's
	// "already bought" is the marker's enabled state - so no global is touched.
	inline constexpr std::uint32_t kAbandonedClutter = 0x000DF495;  // REFR HjerimAbandoned - enabled from the start of the game
	inline constexpr std::uint32_t kKillerClutter = 0x000DF492;     // REFR HjerimKillerClutter - the Butcher's mess; MS11's evidence
	inline constexpr std::uint32_t kPlayerChest = 0x0010FD82;       // REFR WindhelmHjerimPlayerChestEnable
	inline constexpr std::uint32_t kArrivalMarker = 0x000DF48C;     // REFR WindhelmPlayerHouseDecorateBasic (3935, -1890, 192) - the main hall

	struct Room
	{
		std::uint32_t marker;
		const char* name;  // for the log only
	};

	// The basic furnishing first (it is what the sale itself enables), then every room the steward
	// sells. The Hearthfire child's bedroom is NOT here: it is a choice that replaces the guest room's
	// furniture (its fragment disables the basic set), not an upgrade, so it stays with the steward.
	inline constexpr Room kRooms[] = {
		{ 0x000DF48C, "basic furnishing" },
		{ 0x000DF48D, "kitchen" },
		{ 0x000DF48E, "living room" },
		{ 0x000DF48F, "bedroom" },
		{ 0x000DF490, "enchanting laboratory" },
		{ 0x000DF491, "alchemy laboratory" },
		{ 0x000DF493, "armory and displays" },
		{ 0x000DF496, "guest room" },
		{ 0x000DF6EB, "basement lights" },
	};
	inline constexpr std::size_t kRoomCount = std::size(kRooms);

	// ---- Blood on the Ice -----------------------------------------------------------------------------
	// MS11 starts from its own Windhelm triggers whatever the house's state, and its clue items live
	// under HjerimKillerClutter, so that marker is left alone until the quest is COMPLETED - at which
	// point the steward would sell "clean up the mess" (TIF__000F726C: `decoratemarker.disable()` on
	// HjerimKillerClutter). No steward here: the mess goes the moment the case is closed.
	inline constexpr std::uint32_t kBloodOnTheIce = 0x0001F7A3;  // QUST MS11
}
