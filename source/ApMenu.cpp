#include "PCH.h"

#include "ApMenu.h"

#include "Hjerim.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

#include <atomic>
#include <chrono>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace apmenu
{
	namespace
	{
		std::mutex g_lock;
		Report g_report;
		std::atomic<bool> g_installed{ false };
		std::atomic<bool> g_running{ false };

		// How many frames to wait for AP's option array. The menu opens first and the JSON files are then
		// read asynchronously (LoadVars, one file at a time), so the array is empty for a few frames on a
		// cold cache; five seconds at 60 fps is generous and costs nothing while it waits.
		inline constexpr int kMaxPasses = 300;

		RE::GPtr<RE::GFxMovieView> Movie()
		{
			auto* ui = RE::UI::GetSingleton();
			if (!ui) { return {}; }
			auto menu = ui->GetMenu(hjerim::kApMenuName);
			return menu ? menu->uiMovie : RE::GPtr<RE::GFxMovieView>{};
		}

		std::string MemberString(const RE::GFxValue& a_obj, const char* a_name)
		{
			RE::GFxValue v;
			if (!a_obj.GetMember(a_name, &v) || !v.IsString()) { return {}; }
			const char* s = v.GetString();
			return s ? s : std::string{};
		}

		// The sub-choice is labelled with the city, like AP's own Whiterun / Solitude / Riften / Markarth
		// beside it (the owner, 2026-10-13: Hjerim is "Windhelm" in that card). AP's suboptions carry no
		// description line, so the house's name only appears on the fallback card of our own.
		const char* SubChoiceText() { return strings::TR("HJS_Card", "Windhelm"); }
		const char* OwnCardText() { return strings::TR("HJS_OwnCard", "Hjerim"); }

		// One suboption, shaped exactly like AP's own: the text the player reads, the quest's FormID
		// WITHOUT the plugin index (AP calls Game.GetFormFromFile with it), and the plugin that owns it -
		// a suboption without `mod` inherits the card's, which here would be AP's own ESP.
		bool MakeOurOption(RE::GFxMovieView* a_movie, RE::GFxValue& a_out, const char* a_text)
		{
			a_movie->CreateObject(&a_out);
			if (!a_out.IsObject()) { return false; }
			RE::GFxValue text, mod, id;
			a_movie->CreateString(&text, a_text);
			a_movie->CreateString(&mod, hjerim::kPluginName);
			id.SetNumber(static_cast<double>(hjerim::kStartQuestLocalID));
			return a_out.SetMember("text", text) && a_out.SetMember("id", id) && a_out.SetMember("mod", mod);
		}

		// Already there? The menu is rebuilt on every open, but a second pass of ours (or a future AP that
		// keeps the array) must not add the option twice.
		bool AlreadyInSuboptions(const RE::GFxValue& a_subs)
		{
			const std::uint32_t n = a_subs.GetArraySize();
			for (std::uint32_t i = 0; i < n; ++i)
			{
				RE::GFxValue e;
				if (a_subs.GetElement(i, &e) && e.IsObject() && MemberString(e, "mod") == hjerim::kPluginName) { return true; }
			}
			return false;
		}

		// The fallback, when AP has no "My own home" card any more: a card of our own. It has to reach BOTH
		// the array AP keeps and the list the menu has already been given, or it would not be drawn.
		bool AddOwnCard(RE::GFxMovieView* a_movie, RE::GFxValue& a_options, std::string& a_problem)
		{
			RE::GFxValue card;
			if (!MakeOurOption(a_movie, card, OwnCardText()))
			{
				a_problem = "the movie would not make an object";
				return false;
			}
			RE::GFxValue descr, colour;
			a_movie->CreateString(&descr, strings::TR("HJS_CardDescr",
				"The house in Windhelm is already yours - deed, key and every room furnished."));
			a_movie->CreateString(&colour, "0x5B6D7A");
			card.SetMember("description", descr);
			card.SetMember("color", colour);
			if (!a_options.PushBack(card))
			{
				a_problem = "the option array refused the card";
				return false;
			}
			RE::GFxValue list;
			if (!a_movie->GetVariable(&list, hjerim::kApListPath) || !list.IsObject())
			{
				a_problem = "the card was added to the options but the list itself was not found";
				return false;
			}
			RE::GFxValue entries;
			if (!list.GetMember("entryList", &entries) || !entries.IsArray() || !entries.PushBack(card))
			{
				a_problem = "the card was added to the options but the menu's entry list refused it";
				return false;
			}
			list.Invoke("InvalidateData");
			return true;
		}

		// Returns true when this attempt is over (added, fell back, failed, or the menu closed); false to look again.
		bool Pass(int a_pass);

		// 1.0.3: every pass runs as an SKSE UI task - inside the UI's own message processing, the thread AP's movie
		// advances on. Until 1.0.2 each pass was an AddTask, whose queue on 1.5.97 runs as its own job beside the UI job,
		// so its GetVariable / GetMember / PushBack on AP's movie raced the movie's own advance (the shape that crashed
		// Dragon's Eye Minimap 1.7.5). The retries are paced from a short-lived thread of ours, one pass per ~16 ms with
		// the next posted only after the last has run, because SKSE drains its UI queue in a loop: a UI task that queued
		// the next pass itself would run all of them in the same frame, before AP had read its files. That thread never
		// touches Scaleform - it only waits and posts.
		inline constexpr auto kPassInterval = std::chrono::milliseconds(16);
		inline constexpr auto kPassRunTimeout = std::chrono::seconds(10);

		void Drive()
		{
			for (int pass = 1; pass <= kMaxPasses; ++pass)
			{
				if (pass > 1) { std::this_thread::sleep_for(kPassInterval); }

				auto* tasks = SKSE::GetTaskInterface();
				if (!tasks)
				{
					g_running.store(false);
					return;
				}

				// 0 = waiting for the UI to run it, 1 = look again, 2 = over.
				auto state = std::make_shared<std::atomic<int>>(0);
				tasks->AddUITask([pass, state]() { state->store(Pass(pass) ? 2 : 1); });

				const auto deadline = std::chrono::steady_clock::now() + kPassRunTimeout;
				while (state->load() == 0)
				{
					if (std::chrono::steady_clock::now() > deadline)
					{
						logger::warn("menu: pass {} was not run by the UI within {} s; giving up on this menu", pass,
									 std::chrono::duration_cast<std::chrono::seconds>(kPassRunTimeout).count());
						g_running.store(false);
						return;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
				}
				if (state->load() == 2) { return; }
			}
		}

		void Start()
		{
			try
			{
				std::thread(Drive).detach();
			}
			catch (const std::exception& e)
			{
				logger::error("menu: could not start the pass driver ({}); the start will not be added this time", e.what());
				g_running.store(false);
			}
		}

		void Finish(const Report& a_report)
		{
			{
				std::scoped_lock l(g_lock);
				g_report = a_report;
			}
			g_running.store(false);
			if (a_report.injected)
			{
				logger::info("menu: added \"{}\" (Hjerim) to Alternate Perspective's \"My own home\" card ({} card(s), {} pass(es))",
							 SubChoiceText(), a_report.cards, a_report.passes);
			}
			else if (a_report.ownCardAdded)
			{
				logger::warn("menu: Alternate Perspective has no \"{}\" card in this version, so the start was added as a card of "
							 "its own ({} card(s), {} pass(es))", hjerim::kApHomeCardText, a_report.cards, a_report.passes);
			}
			else
			{
				logger::error("menu: the start could not be added to Alternate Perspective's menu: {} ({} pass(es))",
							  a_report.problem.empty() ? "the option list never appeared" : a_report.problem, a_report.passes);
			}
		}

		bool Pass(int a_pass)
		{
			Report r;
			r.passes = a_pass;

			auto movie = Movie();
			if (!movie)
			{
				// The menu closed under us (the player pressed Tab); nothing to do and nothing wrong.
				if (a_pass > 1) { logger::debug("menu: it closed before the option list appeared (pass {})", a_pass); }
				g_running.store(false);
				return true;
			}

			RE::GFxValue options;
			const bool have = movie->GetVariable(&options, hjerim::kApOptionsPath) && options.IsArray() && options.GetArraySize() > 0;
			if (!have)
			{
				if (a_pass < kMaxPasses) { return false; }   // Drive() posts the next pass
				r.problem = "Alternate Perspective's option list never appeared - is this another mod's custom menu?";
				Finish(r);
				return true;
			}

			r.sawMenu = true;
			r.cards = static_cast<int>(options.GetArraySize());
			for (std::uint32_t i = 0; i < options.GetArraySize(); ++i)
			{
				RE::GFxValue card;
				if (!options.GetElement(i, &card) || !card.IsObject()) { continue; }
				if (MemberString(card, "text") != hjerim::kApHomeCardText) { continue; }
				RE::GFxValue subs;
				if (!card.GetMember("suboptions", &subs) || !subs.IsArray())
				{
					r.problem = "the \"My own home\" card has no suboptions to join";
					break;
				}
				if (AlreadyInSuboptions(subs))
				{
					r.injected = true;
					Finish(r);
					return true;
				}
				RE::GFxValue option;
				if (!MakeOurOption(movie.get(), option, SubChoiceText()))
				{
					r.problem = "the movie would not make an object";
					break;
				}
				if (!subs.PushBack(option))
				{
					r.problem = "the card's suboptions refused the option";
					break;
				}
				r.injected = true;
				Finish(r);
				return true;
			}

			if (r.problem.empty())
			{
				r.problem = std::format("no card named {} among the {} the menu has", hjerim::kApHomeCardText, r.cards);
			}
			std::string fallbackProblem;
			if (AddOwnCard(movie.get(), options, fallbackProblem))
			{
				r.ownCardAdded = true;
			}
			else
			{
				r.problem += "; " + fallbackProblem;
			}
			Finish(r);
			return true;
		}

		class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuSink* GetSingleton()
			{
				static MenuSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event,
												  RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && a_event->opening && a_event->menuName == hjerim::kApMenuName && !g_running.exchange(true))
				{
					logger::debug("menu: a custom menu opened; looking for Alternate Perspective's option list");
					Start();
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Install()
	{
		if (g_installed.exchange(true)) { return; }
		auto* ui = RE::UI::GetSingleton();
		if (!ui)
		{
			logger::error("menu: the UI is not available, so the start cannot be added to Alternate Perspective's menu");
			return;
		}
		ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
		logger::info("menu: listening for Alternate Perspective's menu ({}), to add the start to its \"{}\" card",
					 hjerim::kApMenuName, hjerim::kApHomeCardText);
	}

	const Report& Last()
	{
		return g_report;
	}

	std::string StateJson()
	{
		std::scoped_lock l(g_lock);
		std::string problem;
		for (char c : g_report.problem)
		{
			if (c == '"' || c == '\\') { problem += '\\'; }
			problem += c;
		}
		return std::format("\"menu\":{{\"sawMenu\":{},\"injected\":{},\"ownCardAdded\":{},\"passes\":{},\"cards\":{},\"problem\":\"{}\"}}",
						   g_report.sawMenu ? "true" : "false", g_report.injected ? "true" : "false",
						   g_report.ownCardAdded ? "true" : "false", g_report.passes, g_report.cards, problem);
	}
}
