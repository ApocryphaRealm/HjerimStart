// Alternate Perspective Hjerim Addon. Own code, GPL-3.0-or-later (2026-09-18).
//
// An add-on for Alternate Perspective (Nexus 50307). It adds Hjerim - the house in Windhelm, already
// the player's, deed, key and every room furnished - to AP's own "My own home" card, beside the four
// houses vanilla lets you own without a quest.
//
// Two pieces make that work, and only one of them is code:
//   * Alternate Perspective Hjerim Addon.esp - one quest record. It exists so AP has something to name
//     and start; it holds no script and no aliases.
//   * This DLL - it adds the option to AP's menu when the menu opens (ApMenu.cpp; AP merges nothing
//     between add-on files, so a JSON of our own could only ever be a separate card), then watches for
//     that quest starting and does the work, through the same markers, script variable and cell owner
//     the steward's own dialogue uses.
//
// No vanilla record is edited and no game file is replaced, so removing the mod removes it entirely,
// and a mod that refurnishes or improves Hjerim sees exactly what it would see after a vanilla purchase.
#include "PCH.h"

#include "ApMenu.h"
#include "DevBenchTool.h"
#include "Hjerim.h"
#include "Settings.h"
#include "Start.h"

#include "utils/AddressLibraryGuard.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			// Forms exist from here on, which is the earliest our own quest can be looked up.
			strings::Configure(hjerim::kTranslationStem);
			start::Install();
			apmenu::Install();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kNewGame:
		case SKSE::MessagingInterface::kPostLoadGame:
			start::OnGameBegun();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::log::init(hjerim::kLogName);
	// Address Library pre-check (the guard every mod of ours carries): when the file for this game is
	// missing the plugin loads inert with a message that names it, instead of CommonLib's unreadable
	// fail. It runs BEFORE SKSE::Init, which opens the Address Library itself (logic library 6026).
	if (!AddressLibraryGuard::Guard(hjerim::kDisplayName))
	{
		return true;
	}
	SKSE::Init(a_skse);

	settings::Init(hjerim::kIniName);
	settings::ApplyLogLevel();
	SKSE::log::describe_level(hjerim::kIniName);

	logger::info("{} {} loading", hjerim::kDisplayName,
				 SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));

	SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

	// One byte in the co-save: whether this mod handed the house over in this game. It decides whether
	// the Blood on the Ice clean-up is ours to do.
	if (auto* serialization = SKSE::GetSerializationInterface())
	{
		serialization->SetUniqueID('HJST');
		serialization->SetSaveCallback(start::OnSave);
		serialization->SetLoadCallback(start::OnLoad);
		serialization->SetRevertCallback(start::OnRevert);
	}

	return true;
}
