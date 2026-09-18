#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "ApMenu.h"
#include "Hjerim.h"
#include "Settings.h"
#include "Start.h"
#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <algorithm>
#include <format>
#include <functional>
#include <string>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		constexpr const char* kBotiKeys[]{ "HJS_BotiLeave", "HJS_BotiComplete", "HJS_BotiStop" };
		constexpr const char* kBotiEnglish[]{ "Leave it alone", "Mark it solved", "Turn it off" };
		constexpr int kBotiCount{ 3 };

		constexpr const char* kLogLevelKeys[]{ "HJS_LogTrace", "HJS_LogDebug", "HJS_LogInfo", "HJS_LogWarning", "HJS_LogError", "HJS_LogCritical", "HJS_LogOff" };
		constexpr const char* kLogLevelEnglish[]{ "Trace", "Debug", "Info", "Warning", "Error", "Critical", "Off" };
		constexpr int kLogLevelCount{ 7 };

		// The framework draws from the renderer's present hook. Anything that touches the game rather than
		// this page's own variables is handed to the main thread first.
		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface()) { taskInterface->AddTask(std::move(a_task)); }
		}

		void Help(const char* a_text)
		{
			if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", a_text); }
		}

		// A page that calls an export the installed framework does not have crashes on its first draw rather
		// than failing to register, so every export this page resolves is probed by its resolved name first.
		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextWrappedV",
				"igSeparatorText",
				"igCombo_Str_arr",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igSliderFloat",
				"igIsItemHovered",
				"igSetTooltip",
				// The hand-drawn switch (rule 32: a boolean is a switch, never a tick-box).
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
			};
			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("the menu framework does not export \"{}\"", name);
					return false;
				}
			}
			return true;
		}
	}

	void Register() noexcept
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("the Apocrypha Menu Framework is not installed; settings are read from the INI only");
			return;
		}
		if (!HasRequiredExports())
		{
			logger::warn("the installed menu framework is older than this mod's settings page needs; "
						 "update it to configure {} in game", hjerim::kDisplayName);
			return;
		}

		SKSEMenuFramework::SetSection(hjerim::kDisplayName);
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);
		SKSEMenuFramework::AddSectionItem("Status", StatusPanel::Render);

		logger::info("settings page registered with the menu framework");
	}

	void __stdcall SettingsPanel::Render()
	{
		strings::Tick();

		ImGuiMCP::TextWrapped("%s", strings::TR("HJS_Intro",
			"One start in Alternate Perspective's menu: Hjerim, the house in Windhelm, already yours - deed, key and "
			"every room furnished. Everything below is read when the start runs, so a change here applies to the "
			"next new game."));
		ImGuiMCP::Spacing();

		ImGuiMCP::SeparatorText(strings::TR("HJS_SecStart", "The start"));

		ImGuiMCP::Toggle(strings::TR("HJS_Enabled", "Enabled"), &settings::general::enabled);
		Help(strings::TR("HJS_HelpEnabled", "Off leaves the option in Alternate Perspective's menu but makes choosing it do nothing, "
										   "so it can be switched off without uninstalling mid-playthrough."));

		ImGuiMCP::Toggle(strings::TR("HJS_Move", "Begin inside Hjerim"), &settings::start::moveToHjerim);
		Help(strings::TR("HJS_HelpMove", "Moves you into the main hall. Off hands the house over wherever you are."));

		ImGuiMCP::Toggle(strings::TR("HJS_Furnish", "Furnish every room"), &settings::start::furnishEveryRoom);
		Help(strings::TR("HJS_HelpFurnish", "The kitchen, living room, bedroom, enchanting and alchemy laboratories, armory, guest room "
										   "and basement lights - everything the steward sells, in one go. Off gives the house with its "
										   "basic furnishing only, as the sale itself does."));

		const char* boti[kBotiCount];
		for (int i = 0; i < kBotiCount; ++i) { boti[i] = strings::TR(kBotiKeys[i], kBotiEnglish[i]); }
		int botiChoice{ static_cast<int>(settings::start::bloodOnTheIce) };
		if (ImGuiMCP::Combo(strings::TR("HJS_Boti", "Blood on the Ice"), &botiChoice, boti, kBotiCount))
		{
			settings::start::bloodOnTheIce = static_cast<std::uint32_t>(std::clamp(botiChoice, 0, kBotiCount - 1));
		}
		Help(strings::TR("HJS_HelpBoti", "Hjerim is the Butcher's house, and vanilla only sells it once that murder case is closed. "
										 "\"Mark it solved\" puts the case in the state the game itself reaches when it is closed, which is "
										 "what makes the house yours to own; \"Turn it off\" stops the quest instead, claiming nothing in "
										 "your journal. Either clears the grisly clutter in the hidden room. \"Leave it alone\" plays the "
										 "case as usual - the clutter stays until you solve it."));

		ImGuiMCP::Toggle(strings::TR("HJS_CleanUp", "Clear the Butcher's mess once Blood on the Ice is solved"), &settings::start::cleanUpAfterBloodOnTheIce);
		Help(strings::TR("HJS_HelpCleanUp", "Only matters when the case is left alone above: the hidden room's grisly clutter is the quest's "
										   "evidence, so it stays until the case is closed, then goes as the steward's clean-up would have "
										   "removed it. Off leaves it for the steward."));

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("HJS_SecTiming", "Timing"));

		float delay = settings::general::startDelaySeconds;
		if (ImGuiMCP::SliderFloat(strings::TR("HJS_Delay", "Wait before starting (seconds)"), &delay, 0.0f, 10.0f, "%.2f"))
		{
			settings::general::startDelaySeconds = std::clamp(delay, 0.0f, 30.0f);
		}
		Help(strings::TR("HJS_HelpDelay", "Alternate Perspective is still finishing its own hand-off when it starts this mod's quest. "
										 "Raise this if you ever arrive back in the Resting Pilgrim, lower it if you ever land in Helgen."));

		float gap = settings::general::stageGapSeconds;
		if (ImGuiMCP::SliderFloat(strings::TR("HJS_Gap", "Beat between the steps (seconds)"), &gap, 0.0f, 2.0f, "%.2f"))
		{
			settings::general::stageGapSeconds = std::clamp(gap, 0.0f, 5.0f);
		}
		Help(strings::TR("HJS_HelpGap", "The start is spread over several frames on purpose - arrive, then the deed and key, then one "
									   "room's furniture at a time. It is also how often the mod checks whether you have finished "
									   "arriving. 0 puts it all in one frame."));

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("HJS_SecLogging", "Logging"));

		const char* levels[kLogLevelCount];
		for (int i = 0; i < kLogLevelCount; ++i) { levels[i] = strings::TR(kLogLevelKeys[i], kLogLevelEnglish[i]); }
		int logLevel{ static_cast<int>(settings::debug::logLevel) };
		if (ImGuiMCP::Combo(strings::TR("HJS_LogLevel", "Log level"), &logLevel, levels, kLogLevelCount))
		{
			settings::debug::logLevel = static_cast<std::uint32_t>(std::clamp(logLevel, 0, kLogLevelCount - 1));
			OnMainThread([]() { settings::ApplyLogLevel(); });
		}
		Help(strings::TR("HJS_HelpLogLevel", "How much detail reaches the log. It ships at Trace, so a bug report arrives with the log already complete."));

		ImGuiMCP::Spacing();

		if (ImGuiMCP::Button(strings::TR("HJS_SaveBtn", "Save")))
		{
			OnMainThread([]() {
				statusMessage = settings::Save() ? strings::TR("HJS_StatusSaved", "Settings saved.")
												 : strings::TR("HJS_StatusSaveFail", "Could not save the INI. See the log for why.");
			});
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button(strings::TR("HJS_ReloadBtn", "Reload from INI")))
		{
			OnMainThread([]() {
				statusMessage = settings::Reload() ? strings::TR("HJS_StatusReloaded", "Settings reloaded from the INI.")
												   : strings::TR("HJS_StatusReloadFail", "Could not read the INI. See the log for why.");
			});
		}
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button(strings::TR("HJS_DefaultsBtn", "Restore defaults")))
		{
			OnMainThread([]() { settings::RestoreDefaults(); });
			statusMessage = strings::TR("HJS_StatusDefaults", "Defaults restored. Press Save to keep them.");
		}

		if (!statusMessage.empty()) { ImGuiMCP::TextWrapped("%s", statusMessage.c_str()); }

		ImGuiMCP::Spacing();
		ImGuiMCP::TextWrapped(".\\Data\\SKSE\\Plugins\\%s", hjerim::kIniName);
	}

	void __stdcall StatusPanel::Render()
	{
		strings::Tick();

		// "Nothing happened" and "you did not pick this start" look identical from the player's side, so
		// the page says which of the two it was rather than leaving them to guess.
		const auto& run = start::LastRun();

		ImGuiMCP::SeparatorText(strings::TR("HJS_SecSession", "This session"));
		if (!start::WasChosen() && !run.ran)
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("HJS_NotChosen",
				"The start has not been chosen in this session. It runs once, when Alternate Perspective starts its "
				"quest - which happens after you pick \"Hjerim\" from the Messenger in the Resting Pilgrim."));
		}
		else if (!run.ran)
		{
			ImGuiMCP::TextWrapped(strings::TR("HJS_Waiting", "The start was chosen and is waiting out its delay of %.2f seconds."),
								  settings::general::startDelaySeconds);
		}
		else
		{
			ImGuiMCP::TextWrapped(strings::TR("HJS_Ran", "The start ran (%s)."), run.reason.c_str());
			ImGuiMCP::Text(strings::TR("HJS_RanMoved", "Moved into Hjerim: %s"), run.moved ? strings::TR("HJS_Yes", "yes") : strings::TR("HJS_No", "no"));
			ImGuiMCP::Text(strings::TR("HJS_RanOwned", "House owned, key and guide given: %s / %s / %s"),
						   run.owned ? strings::TR("HJS_Yes", "yes") : strings::TR("HJS_No", "no"),
						   run.keyGiven ? strings::TR("HJS_Yes", "yes") : strings::TR("HJS_No", "no"),
						   run.guideGiven ? strings::TR("HJS_Yes", "yes") : strings::TR("HJS_No", "no"));
			ImGuiMCP::Text(strings::TR("HJS_RanRooms", "Rooms furnished: %d"), run.roomsFurnished);
			if (!run.problem.empty()) { ImGuiMCP::TextWrapped(strings::TR("HJS_RanProblem", "Problem: %s"), run.problem.c_str()); }
		}
		ImGuiMCP::Text(strings::TR("HJS_InThisGame", "Handed over by this mod in this game: %s"),
					   start::StartRanInThisGame() ? strings::TR("HJS_Yes", "yes") : strings::TR("HJS_No", "no"));

		// Where the option ended up: it is added to Alternate Perspective's own "My own home" card when its
		// menu opens, so until the player has opened that menu there is nothing to report.
		const auto& menu = apmenu::Last();
		const char* where = menu.injected    ? strings::TR("HJS_MenuInHome", "in Alternate Perspective's \"My own home\" card")
						  : menu.ownCardAdded ? strings::TR("HJS_MenuOwnCard", "as a card of its own (this Alternate Perspective has no \"My own home\" card)")
						  : menu.sawMenu      ? strings::TR("HJS_MenuFailed", "NOT added - see the log")
											  : strings::TR("HJS_MenuUnseen", "not yet - open the Messenger's menu once");
		ImGuiMCP::TextWrapped(strings::TR("HJS_MenuState", "The start option: %s"), where);

		ImGuiMCP::Spacing();
		ImGuiMCP::SeparatorText(strings::TR("HJS_SecRunNow", "Run it now"));
		ImGuiMCP::TextWrapped("%s", strings::TR("HJS_RunNowHelp",
			"For testing, and for a game where the start was interrupted. This does exactly what choosing the start "
			"does - it moves you into Hjerim and hands the house over - so do not press it in a playthrough you care about."));
		if (ImGuiMCP::Button(strings::TR("HJS_RunNowBtn", "Run the Hjerim start now")))
		{
			OnMainThread([]() { start::Run("settings page"); });
			statusMessage = strings::TR("HJS_StatusRun", "The start was run from the settings page.");
		}
	}
}
