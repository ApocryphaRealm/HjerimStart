#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "ApMenu.h"
#include "Hjerim.h"
#include "Settings.h"
#include "Start.h"
#include "utils/Logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace DevBenchTool
{
	namespace
	{
		std::string EscapeJson(std::string_view a_in)
		{
			std::string out;
			out.reserve(a_in.size() + 8);
			for (const char c : a_in)
			{
				switch (c)
				{
				case '\\': out += "\\\\"; break;
				case '"': out += "\\\""; break;
				case '\n': out += "\\n"; break;
				default: out += c; break;
				}
			}
			return out;
		}

		// The value of a top-level JSON member, as text: a quoted string or a bare word. Empty when absent.
		std::string Get(std::string_view a_json, const char* a_name)
		{
			const std::string key = std::format("\"{}\"", a_name);
			auto pos = a_json.find(key);
			if (pos == std::string_view::npos) { return {}; }
			pos = a_json.find(':', pos + key.size());
			if (pos == std::string_view::npos) { return {}; }
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) { ++pos; }
			if (pos >= a_json.size()) { return {}; }
			if (a_json[pos] == '"')
			{
				std::string out;
				for (++pos; pos < a_json.size() && a_json[pos] != '"'; ++pos)
				{
					if (a_json[pos] == '\\' && pos + 1 < a_json.size()) { ++pos; }
					out += a_json[pos];
				}
				return out;
			}
			std::string out;
			while (pos < a_json.size() && a_json[pos] != ',' && a_json[pos] != '}' && a_json[pos] != ' ') { out += a_json[pos++]; }
			return out;
		}

		// The handler runs on DevBench's own thread; moving the player and touching references are
		// main-thread work. Results are owned by shared_ptr so a timed-out task never writes into a dead
		// frame (the Civil War Starts lesson, 2026-09-17).
		bool RunOnMainThread(std::function<void()> a_fn, int a_timeoutMs = 8000)
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) { return false; }
			auto done = std::make_shared<std::atomic<bool>>(false);
			auto m = std::make_shared<std::mutex>();
			auto cv = std::make_shared<std::condition_variable>();
			auto fn = std::make_shared<std::function<void()>>(std::move(a_fn));
			tasks->AddTask([=]() {
				(*fn)();
				{
					std::scoped_lock l(*m);
					done->store(true);
				}
				cv->notify_all();
			});
			std::unique_lock l(*m);
			return cv->wait_for(l, std::chrono::milliseconds(a_timeoutMs), [&]() { return done->load(); });
		}

		std::string StateJson()
		{
			return std::format(
				"{{\"ok\":true,\"op\":\"state\",\"mod\":\"{}\",\"plugin\":\"{}\",{},{},"
				"\"settings\":{{\"enabled\":{},\"startDelaySeconds\":{:.2f},\"stageGapSeconds\":{:.2f},\"moveToHjerim\":{},"
				"\"furnishEveryRoom\":{},\"bloodOnTheIce\":{},\"cleanUpAfterBloodOnTheIce\":{},\"logLevel\":{}}}}}",
				EscapeJson(hjerim::kDisplayName), hjerim::kPluginName, start::StateJson(), apmenu::StateJson(),
				settings::general::enabled ? "true" : "false", settings::general::startDelaySeconds, settings::general::stageGapSeconds,
				settings::start::moveToHjerim ? "true" : "false",
				settings::start::furnishEveryRoom ? "true" : "false", settings::start::bloodOnTheIce,
				settings::start::cleanUpAfterBloodOnTheIce ? "true" : "false", settings::debug::logLevel);
		}

		void StartTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string_view args = a_argsJson ? a_argsJson : "";
			const std::string op = Get(args, "op");

			if (op == "run")
			{
				auto ok = std::make_shared<bool>(false);
				if (!RunOnMainThread([ok]() { *ok = start::Run("devbench"); }))
				{
					a_write(a_sink, R"({"ok":false,"op":"run","error":"main thread did not run the task in time"})");
					return;
				}
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"run\",{}}}", *ok ? "true" : "false", start::StateJson()).c_str());
				return;
			}
			if (op == "menu")
			{
				// The driving tool for this mod's one visible surface: the option inside Alternate Perspective's
				// menu. AP opens that menu when its Messenger dialogue sends this mod event (APDialoguePlayer
				// listens for it), so sending it here opens the same menu the player would, and our sink adds
				// the option to it exactly as it would in play.
				auto sent = std::make_shared<bool>(false);
				if (!RunOnMainThread([sent]() {
						if (auto* source = SKSE::GetModCallbackEventSource())
						{
							SKSE::ModCallbackEvent e{ "AP_MessengerMenuOpen", "", 0.0f, nullptr };
							source->SendEvent(&e);
							*sent = true;
						}
					}))
				{
					a_write(a_sink, R"({"ok":false,"op":"menu","error":"main thread did not run the task in time"})");
					return;
				}
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"menu\",\"note\":\"AP_MessengerMenuOpen sent; read op=state a second later for what was added\"}}",
											*sent ? "true" : "false").c_str());
				return;
			}
			if (op == "state")
			{
				// Read on the main thread too: the state walks references and quests.
				auto text = std::make_shared<std::string>();
				if (!RunOnMainThread([text]() { *text = StateJson(); }))
				{
					a_write(a_sink, R"({"ok":false,"op":"state","error":"main thread did not run the task in time"})");
					return;
				}
				a_write(a_sink, text->c_str());
				return;
			}
			if (op == "reload")
			{
				auto ok = std::make_shared<bool>(false);
				RunOnMainThread([ok]() { *ok = settings::Reload(); });
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"reload\"}}", *ok ? "true" : "false").c_str());
				return;
			}
			if (op == "save")
			{
				auto ok = std::make_shared<bool>(false);
				RunOnMainThread([ok]() { *ok = settings::Save(); });
				a_write(a_sink, std::format("{{\"ok\":{},\"op\":\"save\"}}", *ok ? "true" : "false").c_str());
				return;
			}
			if (op == "set")
			{
				const std::string name = Get(args, "name");
				const std::string value = Get(args, "value");
				if (name.empty() || value.empty())
				{
					a_write(a_sink, R"({"ok":false,"op":"set","error":"need name and value"})");
					return;
				}
				const bool on = value == "1" || value == "true" || value == "yes";
				bool known = true;
				if (name == "enabled") { settings::general::enabled = on; }
				else if (name == "moveToHjerim") { settings::start::moveToHjerim = on; }
				else if (name == "furnishEveryRoom") { settings::start::furnishEveryRoom = on; }
				else if (name == "bloodOnTheIce") { try { settings::start::bloodOnTheIce = static_cast<std::uint32_t>(std::stoul(value)); } catch (...) { known = false; } }
				else if (name == "cleanUpAfterBloodOnTheIce") { settings::start::cleanUpAfterBloodOnTheIce = on; }
				else if (name == "startDelaySeconds") { try { settings::general::startDelaySeconds = std::stof(value); } catch (...) { known = false; } }
				else if (name == "stageGapSeconds") { try { settings::general::stageGapSeconds = std::stof(value); } catch (...) { known = false; } }
				else { known = false; }
				if (!known)
				{
					a_write(a_sink, std::format("{{\"ok\":false,\"op\":\"set\",\"error\":\"unknown setting \\\"{}\\\"\"}}", EscapeJson(name)).c_str());
					return;
				}
				a_write(a_sink, std::format("{{\"ok\":true,\"op\":\"set\",\"name\":\"{}\",\"value\":\"{}\"}}", EscapeJson(name), EscapeJson(value)).c_str());
				return;
			}

			auto text = std::make_shared<std::string>();
			if (!RunOnMainThread([text]() { *text = StateJson(); }))
			{
				a_write(a_sink, R"({"ok":false,"op":"state","error":"main thread did not run the task in time"})");
				return;
			}
			a_write(a_sink, text->c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"hjerimstart.start\" tool"); }
			else { logger::debug("DevBench not detected yet; will retry at the next message"); }
			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Alternate Perspective Hjerim Start. op=state (default): the settings, whether the start "
			"was chosen or has run in this game, who owns the Hjerim cell, which furnishing markers are enabled, the "
			"abandoned and Butcher clutter states, and Blood on the Ice's stage. op=run: run the whole start now - move "
			"the player into Hjerim, hand the house over (deed, key, guide, script variable) and furnish every room - "
			"without going through Alternate Perspective's menu. op=set with name (enabled, moveToHjerim, "
			"furnishEveryRoom, bloodOnTheIce (0 leave / 1 complete / 2 stop), cleanUpAfterBloodOnTheIce, "
			"startDelaySeconds, stageGapSeconds) and value: change one "
			"setting for this session. op=menu opens Alternate Perspective's own menu (the mod event its Messenger sends), "
			"which is how the option this mod adds to AP's \"My own home\" card can be seen. "
			"setting for this session. op=save writes the settings to the INI; op=reload re-reads it.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"value\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool("hjerimstart.start", descriptor, &StartTool, nullptr))
		{
			logger::info("Registered \"hjerimstart.start\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
