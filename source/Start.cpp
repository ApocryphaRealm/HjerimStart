#include "PCH.h"

#include "Start.h"

#include "Hjerim.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace start
{
	namespace
	{
		std::mutex g_lock;
		Report g_report;
		std::atomic<bool> g_installed{ false };
		std::atomic<bool> g_chosen{ false };
		std::atomic<bool> g_queued{ false };
		// Persisted in the co-save: the house was handed over by THIS mod in this game.
		std::atomic<bool> g_ranInThisGame{ false };
		// Where the move was aimed. Nothing is handed over until the player is actually THERE.
		RE::TESObjectCELL* g_destination = nullptr;
		// Our quest's real FormID, filled in at kDataLoaded.
		RE::FormID g_questID = 0;

		constexpr std::uint32_t kRecordRan = 'RAN_';
		constexpr std::uint32_t kRecordVersion = 1;

		std::string JsonText(const std::string& a_text)
		{
			std::string out;
			for (char c : a_text)
			{
				if (c == '"' || c == '\\') { out += '\\'; out += c; }
				else if (c == '\n') { out += "\\n"; }
				else { out += c; }
			}
			return out;
		}

		// Our own quest, looked up by the plugin that owns it rather than by a baked FormID, because the
		// index depends on where the player's load order puts us.
		RE::TESQuest* OurQuest()
		{
			auto* handler = RE::TESDataHandler::GetSingleton();
			return handler ? handler->LookupForm<RE::TESQuest>(hjerim::kStartQuestLocalID, hjerim::kPluginName) : nullptr;
		}

		// Everything else is a vanilla record, so the FormID is the whole address. LookupByID<T> walks
		// the class hierarchy (logic library 5459), so asking for a base class is fine here.
		template <class T>
		T* Vanilla(RE::FormID a_id)
		{
			return RE::TESForm::LookupByID<T>(a_id);
		}

		// A reference that a house overhaul may have removed or replaced: missing is skipped and said,
		// never treated as fatal (the owner, 2026-09-18: compatible with mods that refurnish the house).
		RE::TESObjectREFR* Marker(RE::FormID a_id, const char* a_what)
		{
			auto* ref = Vanilla<RE::TESObjectREFR>(a_id);
			if (!ref)
			{
				logger::warn("house: {} ({:08X}) is not in the game (removed by another mod?); skipped", a_what, a_id);
				return nullptr;
			}
			if (ref->IsDeleted())
			{
				logger::warn("house: {} ({:08X}) is deleted by another mod; skipped", a_what, a_id);
				return nullptr;
			}
			return ref;
		}

		bool EnableMarker(RE::FormID a_id, const char* a_what)
		{
			auto* ref = Marker(a_id, a_what);
			if (!ref) { return false; }
			if (!ref->IsDisabled())
			{
				logger::debug("house: {} ({:08X}) was already enabled", a_what, a_id);
				return true;
			}
			ref->Enable(false);
			logger::info("house: enabled {} ({:08X})", a_what, a_id);
			return true;
		}

		bool DisableMarker(RE::FormID a_id, const char* a_what)
		{
			auto* ref = Marker(a_id, a_what);
			if (!ref) { return false; }
			if (ref->IsDisabled())
			{
				logger::debug("house: {} ({:08X}) was already disabled", a_what, a_id);
				return true;
			}
			ref->Disable();
			logger::info("house: disabled {} ({:08X})", a_what, a_id);
			return true;
		}

		// The Papyrus side of the sale. HousePurchaseScript's WindhelmHouseVar is a CONDITIONAL script
		// variable - the steward's sale and upgrade lines test it - so it is set through the virtual
		// machine on the quest's own script object, which is the value those conditions read.
		bool SetHouseVar(std::string& a_problem)
		{
			auto* quest = Vanilla<RE::TESQuest>(hjerim::kHousePurchaseQuest);
			if (!quest)
			{
				a_problem = "the HousePurchase quest is not in the game";
				logger::error("script: {}", a_problem);
				return false;
			}
			// The VM singleton itself, not SkyrimVM::impl - the 1.7 line's CommonLibSSE-NG keeps that member
			// behind a runtime-data accessor, and this call exists on both lines.
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			if (!policy)
			{
				a_problem = "the Papyrus virtual machine is not available";
				logger::error("script: {}", a_problem);
				return false;
			}
			const RE::VMHandle handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
			RE::BSTSmartPointer<RE::BSScript::Object> object;
			if (handle == 0 || !vm->FindBoundObject(handle, hjerim::kHousePurchaseScript, object) || !object)
			{
				a_problem = std::format("{} is not bound to the HousePurchase quest", hjerim::kHousePurchaseScript);
				logger::error("script: {}", a_problem);
				return false;
			}
			auto* var = object->GetProperty(RE::BSFixedString(hjerim::kWindhelmHouseVar));
			if (!var)
			{
				a_problem = std::format("{} has no property {}", hjerim::kHousePurchaseScript, hjerim::kWindhelmHouseVar);
				logger::error("script: {}", a_problem);
				return false;
			}
			const std::int32_t before = var->IsInt() ? var->GetSInt() : -1;
			var->SetSInt(hjerim::kWindhelmHouseVarOwned);
			logger::info("script: {}.{} {} -> {}", hjerim::kHousePurchaseScript, hjerim::kWindhelmHouseVar, before,
						 hjerim::kWindhelmHouseVarOwned);
			return true;
		}

		// Blood on the Ice, the case this house belongs to (the owner, 2026-09-18: "hjerim should either
		// deactivate the blood on the ice quest or set it as complete"). CompleteQuest is the game's own
		// "case closed" mark - the same one the console's completequest sets - and it runs no stage
		// fragment, so none of the quest's content fires; Stop() is the lighter touch. Both are native
		// functions on Quest with no C++ handle, so both go through the Papyrus VM.
		void SettleBloodOnTheIce(Report& a_report)
		{
			const std::uint32_t what = settings::start::bloodOnTheIce;
			if (what == settings::start::kBotiLeave)
			{
				logger::info("Blood on the Ice: left alone (uBloodOnTheIce=0); the Butcher's clutter stays until the case is closed");
				return;
			}
			auto* quest = Vanilla<RE::TESQuest>(hjerim::kBloodOnTheIce);
			if (!quest)
			{
				logger::error("Blood on the Ice: quest {:08X} is not in the game", hjerim::kBloodOnTheIce);
				if (a_report.problem.empty()) { a_report.problem = "Blood on the Ice is missing from the game"; }
				return;
			}
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			const RE::VMHandle handle = policy ? policy->GetHandleForObject(RE::FormType::Quest, quest) : 0;
			if (handle == 0)
			{
				logger::error("Blood on the Ice: the Papyrus virtual machine is not available");
				if (a_report.problem.empty()) { a_report.problem = "the Papyrus virtual machine is not available"; }
				return;
			}
			const char* const fn = (what == settings::start::kBotiComplete) ? "CompleteQuest" : "Stop";
			auto args = RE::MakeFunctionArguments();
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			const bool ok = vm->DispatchMethodCall(handle, RE::BSFixedString("Quest"), RE::BSFixedString(fn), args, callback);
			a_report.bloodOnTheIce = ok;
			logger::info("Blood on the Ice: Quest.{}() {} (completed={} enabled={} stage={})", fn,
						 ok ? "dispatched" : "NOT dispatched (the script call was refused)",
						 quest->IsCompleted(), quest->IsEnabled(), quest->GetCurrentStageID());
			if (!ok && a_report.problem.empty())
			{
				a_report.problem = std::format("Blood on the Ice could not be {}",
											   what == settings::start::kBotiComplete ? "completed" : "stopped");
			}
		}

		// A file path naming blood, in any case. Texture and model paths are all a base form still has at
		// runtime - editor IDs are not kept on 1.5.97.
		bool NamesBlood(const char* a_path)
		{
			if (!a_path) { return false; }
			constexpr std::string_view word = "blood";
			const std::string_view path = a_path;
			return std::search(path.begin(), path.end(), word.begin(), word.end(), [](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == b;
			}) != path.end();
		}

		// Blood standing in the house that no list of ours can name: another mod's scene. A placed decal is a
		// reference to a texture set, so its diffuse path says what it paints (Skyrim.esm's blood decals all
		// live in the Blood textures folder); a prop says so in its model path. Nothing else is looked at, so
		// a mod's cobwebs and dust stay.
		bool IsBlood(const RE::TESBoundObject* a_base)
		{
			if (!a_base) { return false; }
			switch (a_base->GetFormType())
			{
			case RE::FormType::TextureSet:
				{
					const auto* set = a_base->As<RE::BGSTextureSet>();
					return set && NamesBlood(set->textures[0].textureName.c_str());
				}
			case RE::FormType::Static:
			case RE::FormType::MovableStatic:
				{
					const auto* model = a_base->As<RE::TESModel>();
					return model && NamesBlood(model->GetModel());
				}
			default:
				return false;
			}
		}

		// Is this reference hung off an enable marker the start has switched off? Set to the marker's state
		// directly as well as through the marker, so a child is right whatever the marker's Disable reached.
		// A child flagged "opposite of parent" comes ON when the marker goes off.
		enum class Child { kNo, kSame, kOpposite };

		Child ChildOfSwitchedOff(RE::TESObjectREFR* a_ref, bool a_mess)
		{
			const auto* link = a_ref->extraList.GetByType<RE::ExtraEnableStateParent>();
			if (!link) { return Child::kNo; }
			const auto parent = link->parent.get();
			if (!parent) { return Child::kNo; }
			const auto id = parent->GetFormID();
			if (id != hjerim::kAbandonedClutter && !(a_mess && id == hjerim::kKillerClutter)) { return Child::kNo; }
			return (link->flags & 1) != 0 ? Child::kOpposite : Child::kSame;
		}

		// Everything that makes it the Butcher's house rather than the player's. Safe to repeat, and cheap:
		// after the first pass it finds the refs already off and writes nothing.
		//
		// WHY IT RUNS BEFORE THE MOVE. A placed decal is painted onto the geometry around it when its cell
		// attaches, and it is not part of the reference's own 3D, so switching the reference off while the
		// player stands there leaves the paint on the floor until the cell next loads. That is how 1.0.3 left
		// blood behind: its log has the player in Hjerim at 00:08:23.090 and the mess off at 00:08:23.277. The
		// steward's clean-up never shows this because the buyer is in the palace at the time. All four vanilla
		// decals are persistent, so they are in memory with the cell unloaded and can be reached from anywhere.
		//
		// Disabled state is written to the save by the game itself, so none of this has to re-run after a
		// load; it runs again on entering the house only to catch references that exist only while the cell is
		// loaded (the bones, and whatever another mod placed). Those are props, gone at once - a non-persistent
		// decal of another mod's would stay painted until the next time the cell loads.
		void ClearTheHouse(const char* a_when, bool a_mess)
		{
			DisableMarker(hjerim::kAbandonedClutter, "the abandoned clutter");
			if (a_mess) { DisableMarker(hjerim::kKillerClutter, "the Butcher's mess"); }

			std::vector<RE::TESObjectREFR*> off;
			std::vector<RE::TESObjectREFR*> on;
			if (a_mess)
			{
				// By FormID first: a mod may have moved one out of the cell or cut its link to the marker.
				for (const auto id : hjerim::kBloodDecals)
				{
					auto* ref = Vanilla<RE::TESObjectREFR>(id);
					if (ref && !ref->IsDeleted() && !ref->IsDisabled()) { off.push_back(ref); }
				}
			}

			// Collected under the cell's lock and acted on outside it: Disable reaches into other locks.
			if (auto* cell = Vanilla<RE::TESObjectCELL>(hjerim::kHjerimCell))
			{
				cell->ForEachReference([&](RE::TESObjectREFR* a_ref) {
					if (a_ref && !a_ref->IsDeleted())
					{
						const Child child = ChildOfSwitchedOff(a_ref, a_mess);
						if (child == Child::kOpposite)
						{
							if (a_ref->IsDisabled()) { on.push_back(a_ref); }
						}
						else if (!a_ref->IsDisabled() && (child == Child::kSame || (a_mess && IsBlood(a_ref->GetBaseObject()))))
						{
							off.push_back(a_ref);
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}

			std::sort(off.begin(), off.end());
			off.erase(std::unique(off.begin(), off.end()), off.end());
			for (auto* ref : off)
			{
				logger::debug("clean-up ({}): disabled {:08X}", a_when, ref->GetFormID());
				ref->Disable();
			}
			for (auto* ref : on) { ref->Enable(false); }
			if (!off.empty() || !on.empty())
			{
				logger::info("clean-up ({}): {} reference(s) still on were disabled, {} opposite child(ren) enabled", a_when, off.size(), on.size());
			}
		}

		// Is the Butcher's scene ours to clear? Asked only in a game where this start ran: any other player has
		// the steward's own purchase and the case as vanilla has it. Here, yes once the case is settled by the
		// setting, or, when the case was left alone, once it is genuinely COMPLETED, because its clue items
		// live under that marker.
		bool SceneIsOurs()
		{
			if (settings::start::bloodOnTheIce != settings::start::kBotiLeave) { return true; }
			if (!settings::start::cleanUpAfterBloodOnTheIce) { return false; }
			const auto* ms11 = Vanilla<RE::TESQuest>(hjerim::kBloodOnTheIce);
			return ms11 && ms11->IsCompleted();
		}

		// Game.IncrementStat("Houses Owned") - the one thing PurchaseHouse does that has no C++ handle.
		void CountTheHouse()
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm) { return; }
			auto args = RE::MakeFunctionArguments(RE::BSFixedString("Houses Owned"), static_cast<std::int32_t>(1));
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			const bool ok = vm->DispatchStaticCall(RE::BSFixedString("Game"), RE::BSFixedString("IncrementStat"), args, callback);
			logger::info("stat: Houses Owned +1 {}", ok ? "dispatched" : "NOT dispatched (the script call was refused)");
		}

		bool MovePlayer(RE::PlayerCharacter* a_player, std::string& a_problem)
		{
			auto* marker = Vanilla<RE::TESObjectREFR>(hjerim::kArrivalMarker);
			if (!marker)
			{
				a_problem = std::format("the arrival marker {:08X} is not in the game", hjerim::kArrivalMarker);
				logger::error("move: {}", a_problem);
				return false;
			}
			g_destination = marker->GetParentCell();
			a_player->MoveTo(marker);
			logger::info("move: the player was sent to Hjerim's main hall (marker {:08X})", hjerim::kArrivalMarker);
			return true;
		}

		// The sale, as HousePurchaseScript.PurchaseHouse does it, minus the gold: the cell to the player's
		// faction, the key and the decorating guide into the pack, the script variable, the count. Then
		// what Blood on the Ice's own resolution does to the house: the basic furnishing and the player's
		// chest on (the abandoned clutter went before the move, see ClearTheHouse).
		void HandOver(RE::PlayerCharacter* a_player, Report& a_report)
		{
			if (auto* cell = Vanilla<RE::TESObjectCELL>(hjerim::kHjerimCell))
			{
				if (auto* faction = Vanilla<RE::TESFaction>(hjerim::kPlayerFaction))
				{
					cell->SetFactionOwner(faction);
					a_report.owned = true;
					logger::info("deed: WindhelmHjerim ({:08X}) now belongs to PlayerFaction", hjerim::kHjerimCell);
				}
				else
				{
					a_report.problem = "PlayerFaction is not in the game";
					logger::error("deed: {}", a_report.problem);
				}
			}
			else
			{
				a_report.problem = "the Hjerim cell is not in the game";
				logger::error("deed: {}", a_report.problem);
			}

			if (auto* key = Vanilla<RE::TESBoundObject>(hjerim::kHouseKey))
			{
				a_player->AddObjectToContainer(key, nullptr, 1, nullptr);
				a_report.keyGiven = true;
				logger::info("deed: gave the house key ({:08X})", hjerim::kHouseKey);
			}
			else
			{
				logger::error("deed: the house key ({:08X}) is not in the game", hjerim::kHouseKey);
				if (a_report.problem.empty()) { a_report.problem = "the house key is missing from the game"; }
			}

			if (auto* guide = Vanilla<RE::TESBoundObject>(hjerim::kDecoratingGuide))
			{
				a_player->AddObjectToContainer(guide, nullptr, 1, nullptr);
				a_report.guideGiven = true;
				logger::info("deed: gave the decorating guide ({:08X})", hjerim::kDecoratingGuide);
			}
			else
			{
				logger::warn("deed: the decorating guide ({:08X}) is not in the game; skipped", hjerim::kDecoratingGuide);
			}

			std::string scriptProblem;
			a_report.scriptVarSet = SetHouseVar(scriptProblem);
			if (!a_report.scriptVarSet && a_report.problem.empty()) { a_report.problem = scriptProblem; }

			CountTheHouse();

			EnableMarker(hjerim::kPlayerChest, "the player's chest");
			SettleBloodOnTheIce(a_report);
		}

		void RunNow(std::string a_reason)
		{
			g_queued.store(false);
			Run(a_reason.c_str());
		}

		// The wait is a real wait, not a frame count: AP is mid-hand-off when it starts our quest. The
		// sleep happens on its own thread and the work itself is handed back to the game thread, because
		// moving the player from anywhere else is not safe.
		void QueueRun(const char* a_reason)
		{
			if (g_queued.exchange(true)) { return; }
			const std::string reason = a_reason;
			const float delay = settings::general::startDelaySeconds;
			std::thread([reason, delay]() {
				if (delay > 0.0f)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000.0f)));
				}
				if (auto* task = SKSE::GetTaskInterface())
				{
					task->AddTask([reason]() { RunNow(reason); });
				}
				else
				{
					RunNow(reason);
				}
			}).detach();
		}

		// THERE IS NO WATCHDOG, AND THAT IS DELIBERATE - see Civil War Starts, where one fired at
		// selection time and teleported the player before they walked through the door (2026-09-16).
		// The quest-start event is the only trigger.

		void QueueStep(std::string a_reason, std::size_t a_step);

		// How many times the arrival step looks before giving up and carrying on anyway.
		inline constexpr int kArrivalLooks = 40;
		int g_arrivalLooks = 0;

		// The order:
		//   0        wait until the player has actually arrived
		//   1        the sale - deed, key, guide, script variable, count, chest on
		//   2..n     one room's furniture per step
		//   last     look over the house once more, stop our own quest, say what happened
		// (The clutter and the blood go before all of this, in Run, before the player is moved.)
		void RunStep(std::string a_reason, std::size_t a_step)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			const std::size_t roomFirst = 2;
			const std::size_t roomLast = roomFirst + hjerim::kRoomCount;  // one past the last room step

			// Step 0: are they there yet? Checked, not timed - a teleport takes as long as it takes.
			if (a_step == 0)
			{
				const auto* here = player ? player->GetParentCell() : nullptr;
				const bool arrived = !g_destination || (here && here == g_destination);
				if (!arrived && ++g_arrivalLooks < kArrivalLooks)
				{
					QueueStep(a_reason, 0);
					return;
				}
				logger::info("move: the player is now in cell {} (expected: WindhelmHjerim){}",
							 here ? here->GetFormEditorID() : "<unknown>", arrived ? "" : " - gave up waiting and carried on");
				QueueStep(a_reason, 1);
				return;
			}

			// Step 1: the sale.
			if (a_step == 1)
			{
				if (player)
				{
					std::scoped_lock l(g_lock);
					HandOver(player, g_report);
				}
				QueueStep(a_reason, settings::start::furnishEveryRoom ? roomFirst : roomLast);
				return;
			}

			if (a_step >= roomFirst && a_step < roomLast)
			{
				const auto& room = hjerim::kRooms[a_step - roomFirst];
				if (EnableMarker(room.marker, room.name))
				{
					std::scoped_lock l(g_lock);
					++g_report.roomsFurnished;
				}
				QueueStep(a_reason, a_step + 1);
				return;
			}

			// The last step: tidy up and say what actually happened. Enabling a room marker turns on all its
			// children, so the house is looked over again after them. Our own quest has nothing left to do once
			// it has run, and a quest left running shows up in save inspections forever.
			ClearTheHouse("furnished", SceneIsOurs());
			if (auto* ours = OurQuest(); ours && ours->IsEnabled())
			{
				ours->Stop();
				logger::debug("start: our own quest stopped");
			}

			std::scoped_lock l(g_lock);
			logger::info("start ({}): finished - moved={} owned={} key={} guide={} scriptVar={} bloodOnTheIce={} rooms={}/{}{}",
						 a_reason, g_report.moved, g_report.owned, g_report.keyGiven, g_report.guideGiven, g_report.scriptVarSet,
						 g_report.bloodOnTheIce,
						 g_report.roomsFurnished, settings::start::furnishEveryRoom ? hjerim::kRoomCount : 0,
						 g_report.problem.empty() ? "" : std::format(" (problem: {})", g_report.problem));
		}

		void QueueStep(std::string a_reason, std::size_t a_step)
		{
			const float gap = std::clamp(settings::general::stageGapSeconds, 0.0f, 5.0f);
			std::thread([a_reason, a_step, gap]() {
				if (gap > 0.0f)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(gap * 1000.0f)));
				}
				if (auto* tasks = SKSE::GetTaskInterface())
				{
					tasks->AddTask([a_reason, a_step]() { RunStep(a_reason, a_step); });
				}
			}).detach();
		}

		void StageTheRest(std::string a_reason)
		{
			g_arrivalLooks = 0;
			QueueStep(std::move(a_reason), 0);
		}

		// The Butcher's mess goes, as the steward's 500-septim "clean up the mess" would have removed it. Only
		// for a house this mod handed over - a player who bought Hjerim keeps the steward's purchase.
		void CleanUpIfCaseClosed(const char* a_when)
		{
			if (!g_ranInThisGame.load()) { return; }
			ClearTheHouse(a_when, SceneIsOurs());
		}

		// Three moments can close the case from this mod's point of view, and none of them alone is enough:
		// MS11's stop event (a completed quest can stay enabled - console completequest showed that on the
		// first live test, 2026-09-18), its stage events (the completion stage sets the flag), and the
		// player walking into Hjerim (where the clutter is seen; this also covers a case closed before an
		// update added this mod). Each hands the same check to the main thread.
		void QueueCleanUp(const char* a_when)
		{
			if (auto* tasks = SKSE::GetTaskInterface())
			{
				const std::string when = a_when;
				tasks->AddTask([when]() { CleanUpIfCaseClosed(when.c_str()); });
			}
		}

		class QuestSink : public RE::BSTEventSink<RE::TESQuestStartStopEvent>,
						  public RE::BSTEventSink<RE::TESQuestStageEvent>
		{
		public:
			static QuestSink* GetSingleton()
			{
				static QuestSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStartStopEvent* a_event,
												  RE::BSTEventSource<RE::TESQuestStartStopEvent>*) override
			{
				if (!a_event) { return RE::BSEventNotifyControl::kContinue; }
				if (a_event->started && g_questID != 0 && a_event->formID == g_questID)
				{
					g_chosen.store(true);
					logger::info("chosen: Alternate Perspective started our quest {:08X}; it runs in {:.2f}s",
								 a_event->formID, settings::general::startDelaySeconds);
					QueueRun("alternate perspective");
				}
				else if (!a_event->started && a_event->formID == hjerim::kBloodOnTheIce)
				{
					QueueCleanUp("Blood on the Ice stopped");
				}
				return RE::BSEventNotifyControl::kContinue;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStageEvent* a_event,
												  RE::BSTEventSource<RE::TESQuestStageEvent>*) override
			{
				if (a_event && a_event->formID == hjerim::kBloodOnTheIce)
				{
					logger::debug("Blood on the Ice reached stage {}", a_event->stage);
					QueueCleanUp("Blood on the Ice stage");
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class CellSink : public RE::BSTEventSink<RE::BGSActorCellEvent>
		{
		public:
			static CellSink* GetSingleton()
			{
				static CellSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::BGSActorCellEvent* a_event,
												  RE::BSTEventSource<RE::BGSActorCellEvent>*) override
			{
				if (a_event && a_event->flags == RE::BGSActorCellEvent::CellFlag::kEnter && a_event->cellID == hjerim::kHjerimCell)
				{
					QueueCleanUp("entered Hjerim");
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		if (g_installed.exchange(true)) { return; }

		auto* quest = OurQuest();
		if (!quest)
		{
			// Said plainly, because this is the one failure a player can actually fix: the ESP is what AP
			// names in its menu, and without it the start option cannot do anything.
			logger::error("install: {} has no quest {:03X}. The plugin ships with the mod - check that it is installed and enabled.",
						  hjerim::kPluginName, hjerim::kStartQuestLocalID);
			return;
		}
		g_questID = quest->GetFormID();
		logger::info("install: the start is quest {:08X} ({})", g_questID, quest->GetFormEditorID());

		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder)
		{
			logger::error("install: the event source holder is not available; a start cannot be noticed");
			return;
		}
		holder->AddEventSink<RE::TESQuestStartStopEvent>(QuestSink::GetSingleton());
		holder->AddEventSink<RE::TESQuestStageEvent>(QuestSink::GetSingleton());
		if (auto* player = RE::PlayerCharacter::GetSingleton())
		{
			player->AsBGSActorCellEventSource()->AddEventSink(CellSink::GetSingleton());
		}
		logger::info("install: listening for the start quest, for Blood on the Ice's stages and stop, and for the player entering Hjerim");
	}

	void OnGameBegun()
	{
		logger::debug("a game has begun; ran-in-this-game={}", g_ranInThisGame.load());
		CleanUpIfCaseClosed("game begun");
	}

	bool Run(const char* a_reason)
	{
		std::scoped_lock l(g_lock);

		Report report;
		report.reason = a_reason ? a_reason : "";

		if (!settings::general::enabled)
		{
			report.problem = "the mod is switched off in its settings (bEnabled=0)";
			logger::warn("start ({}): {}", report.reason, report.problem);
			g_report = report;
			return false;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player)
		{
			report.problem = "there is no player to move";
			logger::error("start ({}): {}", report.reason, report.problem);
			g_report = report;
			return false;
		}

		logger::info("start ({}): beginning", report.reason);

		// From here on this game's house is ours to look after, and the first thing it needs is to be
		// clean BEFORE the player's arrival attaches the cell (see ClearTheHouse).
		g_ranInThisGame.store(true);
		ClearTheHouse("before the move", SceneIsOurs());

		g_destination = nullptr;
		if (settings::start::moveToHjerim) { report.moved = MovePlayer(player, report.problem); }
		else { logger::info("move: skipped (bMoveToHjerim=0)"); }

		report.ran = true;
		g_report = report;
		StageTheRest(report.reason);
		logger::info("start ({}): moved={}; the deed and the furniture follow once you are there", report.reason, report.moved);
		return true;
	}

	const Report& LastRun()
	{
		return g_report;
	}

	bool WasChosen()
	{
		return g_chosen.load();
	}

	bool StartRanInThisGame()
	{
		return g_ranInThisGame.load();
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (!a_intfc) { return; }
		if (a_intfc->OpenRecord(kRecordRan, kRecordVersion))
		{
			const std::uint8_t v = g_ranInThisGame.load() ? 1 : 0;
			a_intfc->WriteRecordData(&v, sizeof(v));
		}
	}

	void OnLoad(SKSE::SerializationInterface* a_intfc)
	{
		g_ranInThisGame.store(false);
		if (!a_intfc) { return; }
		std::uint32_t type = 0, version = 0, length = 0;
		while (a_intfc->GetNextRecordInfo(type, version, length))
		{
			if (type == kRecordRan && length >= sizeof(std::uint8_t))
			{
				std::uint8_t v = 0;
				if (a_intfc->ReadRecordData(&v, sizeof(v)) == sizeof(v)) { g_ranInThisGame.store(v != 0); }
			}
		}
		logger::debug("co-save: ran-in-this-game={}", g_ranInThisGame.load());
	}

	void OnRevert(SKSE::SerializationInterface*)
	{
		g_ranInThisGame.store(false);
		std::scoped_lock l(g_lock);
		g_report = Report{};
	}

	std::string StateJson()
	{
		std::scoped_lock l(g_lock);

		auto* ours = OurQuest();
		auto* ms11 = Vanilla<RE::TESQuest>(hjerim::kBloodOnTheIce);
		auto* cell = Vanilla<RE::TESObjectCELL>(hjerim::kHjerimCell);
		auto* owner = cell ? cell->GetOwner() : nullptr;
		auto* abandoned = Vanilla<RE::TESObjectREFR>(hjerim::kAbandonedClutter);
		auto* killer = Vanilla<RE::TESObjectREFR>(hjerim::kKillerClutter);
		int bloodStillOn = 0;
		for (const auto id : hjerim::kBloodDecals)
		{
			const auto* decal = Vanilla<RE::TESObjectREFR>(id);
			if (decal && !decal->IsDisabled()) { ++bloodStillOn; }
		}

		std::string rooms;
		for (std::size_t i = 0; i < hjerim::kRoomCount; ++i)
		{
			auto* ref = Vanilla<RE::TESObjectREFR>(hjerim::kRooms[i].marker);
			rooms += std::format("{}{{\"name\":\"{}\",\"marker\":\"{:08X}\",\"enabled\":{}}}", i ? "," : "", hjerim::kRooms[i].name,
								 hjerim::kRooms[i].marker, ref && !ref->IsDisabled() ? "true" : "false");
		}

		return std::format(
			"\"chosen\":{},\"ranInThisGame\":{},"
			"\"ourQuest\":{{\"formID\":\"{:08X}\",\"enabled\":{},\"stopped\":{}}},"
			"\"house\":{{\"cell\":\"{:08X}\",\"owner\":\"{}\",\"ownedByPlayerFaction\":{},\"abandonedClutterEnabled\":{},\"killerClutterEnabled\":{},\"bloodDecalsEnabled\":{},\"rooms\":[{}]}},"
			"\"bloodOnTheIce\":{{\"formID\":\"{:08X}\",\"enabled\":{},\"completed\":{},\"stage\":{}}},"
			"\"lastRun\":{{\"ran\":{},\"reason\":\"{}\",\"moved\":{},\"owned\":{},\"keyGiven\":{},\"guideGiven\":{},\"scriptVarSet\":{},\"bloodOnTheIceSettled\":{},\"roomsFurnished\":{},\"problem\":\"{}\"}}",
			g_chosen.load() ? "true" : "false", g_ranInThisGame.load() ? "true" : "false",
			g_questID, ours && ours->IsEnabled() ? "true" : "false", ours && ours->IsStopped() ? "true" : "false",
			hjerim::kHjerimCell, owner ? owner->GetFormEditorID() : "", owner && owner->GetFormID() == hjerim::kPlayerFaction ? "true" : "false",
			abandoned && !abandoned->IsDisabled() ? "true" : "false", killer && !killer->IsDisabled() ? "true" : "false", bloodStillOn, rooms,
			hjerim::kBloodOnTheIce, ms11 && ms11->IsEnabled() ? "true" : "false", ms11 && ms11->IsCompleted() ? "true" : "false",
			ms11 ? ms11->GetCurrentStageID() : 0,
			g_report.ran ? "true" : "false", JsonText(g_report.reason), g_report.moved ? "true" : "false",
			g_report.owned ? "true" : "false", g_report.keyGiven ? "true" : "false", g_report.guideGiven ? "true" : "false",
			g_report.scriptVarSet ? "true" : "false", g_report.bloodOnTheIce ? "true" : "false",
			g_report.roomsFurnished, JsonText(g_report.problem));
	}
}
