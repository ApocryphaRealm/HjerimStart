#include "PCH.h"

#include "Settings.h"

#include "Hjerim.h"
#include "utils/INISettingCollection.h"
#include "utils/Logger.h"
#include "utils/Setting.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace settings
{
	namespace
	{
		std::string iniPath;

		struct Defaults
		{
			std::uint32_t logLevel;
			bool enabled;
			float startDelaySeconds;
			float stageGapSeconds;
			bool moveToHjerim;
			bool furnishEveryRoom;
			std::uint32_t bloodOnTheIce;
			bool cleanUpAfterBloodOnTheIce;
		} defaults{};

		std::string Lower(std::string a_s)
		{
			for (char& c : a_s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return a_s;
		}

		std::string Trim(const std::string& a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) { return {}; }
			const auto e = a_s.find_last_not_of(" \t\r\n");
			return a_s.substr(b, e - b + 1);
		}

		bool ParseBool(const std::string& a_text, bool& a_out)
		{
			const std::string v = Lower(Trim(a_text));
			if (v == "1" || v == "true" || v == "yes") { a_out = true; return true; }
			if (v == "0" || v == "false" || v == "no") { a_out = false; return true; }
			return false;
		}

		bool ParseUInt(const std::string& a_text, std::uint32_t& a_out)
		{
			try { a_out = static_cast<std::uint32_t>(std::stoull(Trim(a_text), nullptr, 0)); return true; } catch (...) { return false; }
		}

		bool ParseFloat(const std::string& a_text, float& a_out)
		{
			try { a_out = std::stof(Trim(a_text)); return true; } catch (...) { return false; }
		}

		// key = "<name>:<section>", lowercased, so a value can be found without caring how the file is laid out.
		void ReadFile(std::map<std::string, std::string>& a_out, int& a_bad)
		{
			std::ifstream in(iniPath);
			if (!in) { return; }
			std::string line, section;
			bool first = true;
			while (std::getline(in, line))
			{
				// A UTF-8 BOM on the first line hides the first section header from the parser, and a settings
				// file edited in Notepad picks one up silently. It is stripped on read and put back on write.
				if (first)
				{
					first = false;
					if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
						static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
					{
						line.erase(0, 3);
					}
				}
				const std::string t = Trim(line);
				if (t.empty() || t[0] == ';' || t[0] == '#') { continue; }
				if (t.front() == '[' && t.back() == ']') { section = Lower(t.substr(1, t.size() - 2)); continue; }
				const auto eq = t.find('=');
				if (eq == std::string::npos) { ++a_bad; continue; }
				a_out[Lower(Trim(t.substr(0, eq))) + ":" + section] = Trim(t.substr(eq + 1));
			}
		}

		bool LoadFileValues()
		{
			if (!std::filesystem::exists(iniPath))
			{
				logger::warn("INI not found at {}; keeping compiled defaults", iniPath);
				return false;
			}
			std::map<std::string, std::string> k;
			int bad = 0;
			ReadFile(k, bad);
			auto get = [&](const char* a_key, auto& a_out, auto a_parse) {
				const auto it = k.find(a_key);
				if (it == k.end()) { logger::debug("INI key {} missing; keeping current value", a_key); return; }
				if (!a_parse(it->second, a_out)) { logger::warn("INI value \"{}\" for {} is not valid; keeping current value", it->second, a_key); }
			};
			get("uloglevel:debug", debug::logLevel, ParseUInt);
			get("benabled:general", general::enabled, ParseBool);
			get("fstartdelayseconds:general", general::startDelaySeconds, ParseFloat);
			get("fstagegapseconds:general", general::stageGapSeconds, ParseFloat);
			get("bmovetohjerim:start", start::moveToHjerim, ParseBool);
			get("bfurnisheveryroom:start", start::furnishEveryRoom, ParseBool);
			get("ubloodontheice:start", start::bloodOnTheIce, ParseUInt);
			get("bcleanupafterbloodontheice:start", start::cleanUpAfterBloodOnTheIce, ParseBool);
			start::bloodOnTheIce = std::clamp<std::uint32_t>(start::bloodOnTheIce, 0u, 2u);

			general::startDelaySeconds = std::clamp(general::startDelaySeconds, 0.0f, 30.0f);
			general::stageGapSeconds = std::clamp(general::stageGapSeconds, 0.0f, 5.0f);

			logger::info("settings loaded from {}: enabled={} delay={:.2f}s stageGap={:.2f}s move={} furnish={} bloodOnTheIce={} cleanUp={} logLevel={}{}",
						 iniPath, general::enabled, general::startDelaySeconds, general::stageGapSeconds, start::moveToHjerim,
						 start::furnishEveryRoom, start::bloodOnTheIce, start::cleanUpAfterBloodOnTheIce,
						 debug::logLevel, bad ? std::format(" ({} bad line(s) ignored)", bad) : "");
			return true;
		}

		// Rewrite one key in place, so every comment the player's file carries survives the write. A key that
		// is not in the file is appended under its section, and a section that is not there is created.
		bool WriteKey(std::vector<std::string>& a_lines, const char* a_section, const char* a_key, const std::string& a_value)
		{
			const std::string wantSection = Lower(a_section);
			const std::string wantKey = Lower(a_key);
			std::string section;
			std::size_t sectionEnd = std::string::npos;
			for (std::size_t i = 0; i < a_lines.size(); ++i)
			{
				const std::string t = Trim(a_lines[i]);
				if (!t.empty() && t.front() == '[' && t.back() == ']')
				{
					if (section == wantSection) { sectionEnd = i; }
					section = Lower(t.substr(1, t.size() - 2));
					continue;
				}
				if (section != wantSection) { continue; }
				const auto eq = t.find('=');
				if (eq == std::string::npos || t.empty() || t[0] == ';' || t[0] == '#') { continue; }
				if (Lower(Trim(t.substr(0, eq))) == wantKey)
				{
					a_lines[i] = std::format("{}={}", a_key, a_value);
					return true;
				}
			}
			if (section == wantSection) { sectionEnd = a_lines.size(); }
			if (sectionEnd == std::string::npos)
			{
				a_lines.push_back("");
				a_lines.push_back(std::format("[{}]", a_section));
				a_lines.push_back(std::format("{}={}", a_key, a_value));
				return true;
			}
			a_lines.insert(a_lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd), std::format("{}={}", a_key, a_value));
			return true;
		}
	}

	void Init(const std::string& a_iniFileName)
	{
		iniPath = (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / a_iniFileName).string();

		defaults = { debug::logLevel, general::enabled, general::startDelaySeconds, general::stageGapSeconds,
					 start::moveToHjerim, start::furnishEveryRoom, start::bloodOnTheIce, start::cleanUpAfterBloodOnTheIce };

		auto* collection = utils::INISettingCollection::GetSingleton();
		collection->AddSettings(
			utils::MakeSetting("uLogLevel:Debug", static_cast<unsigned int>(debug::logLevel)),
			utils::MakeSetting("bEnabled:General", general::enabled),
			utils::MakeSetting("fStartDelaySeconds:General", general::startDelaySeconds),
			utils::MakeSetting("fStageGapSeconds:General", general::stageGapSeconds),
			utils::MakeSetting("bMoveToHjerim:Start", start::moveToHjerim),
			utils::MakeSetting("bFurnishEveryRoom:Start", start::furnishEveryRoom),
			utils::MakeSetting("uBloodOnTheIce:Start", static_cast<unsigned int>(start::bloodOnTheIce)),
			utils::MakeSetting("bCleanUpAfterBloodOnTheIce:Start", start::cleanUpAfterBloodOnTheIce));

		LoadFileValues();
	}

	bool Reload()
	{
		const bool ok = LoadFileValues();
		ApplyLogLevel();
		return ok;
	}

	bool Save()
	{
		std::vector<std::string> lines;
		bool bom = false;
		{
			std::ifstream in(iniPath, std::ios::binary);
			if (!in) { logger::error("Save: could not open {} for reading", iniPath); return false; }
			std::string line;
			bool first = true;
			while (std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r') { line.pop_back(); }
				if (first)
				{
					first = false;
					if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
						static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
					{
						bom = true;
						line.erase(0, 3);
					}
				}
				lines.push_back(line);
			}
		}

		WriteKey(lines, "Debug", "uLogLevel", std::to_string(debug::logLevel));
		WriteKey(lines, "General", "bEnabled", general::enabled ? "1" : "0");
		WriteKey(lines, "General", "fStartDelaySeconds", std::format("{:.2f}", general::startDelaySeconds));
		WriteKey(lines, "General", "fStageGapSeconds", std::format("{:.2f}", general::stageGapSeconds));
		WriteKey(lines, "Start", "bMoveToHjerim", start::moveToHjerim ? "1" : "0");
		WriteKey(lines, "Start", "bFurnishEveryRoom", start::furnishEveryRoom ? "1" : "0");
		WriteKey(lines, "Start", "uBloodOnTheIce", std::to_string(start::bloodOnTheIce));
		WriteKey(lines, "Start", "bCleanUpAfterBloodOnTheIce", start::cleanUpAfterBloodOnTheIce ? "1" : "0");

		std::ofstream out(iniPath, std::ios::binary | std::ios::trunc);
		if (!out) { logger::error("Save: could not open {} for writing", iniPath); return false; }
		if (bom) { out << "\xEF\xBB\xBF"; }
		for (const auto& line : lines) { out << line << "\r\n"; }
		logger::info("settings saved to {}", iniPath);
		return true;
	}

	void RestoreDefaults()
	{
		debug::logLevel = defaults.logLevel;
		general::enabled = defaults.enabled;
		general::startDelaySeconds = defaults.startDelaySeconds;
		general::stageGapSeconds = defaults.stageGapSeconds;
		start::moveToHjerim = defaults.moveToHjerim;
		start::furnishEveryRoom = defaults.furnishEveryRoom;
		start::bloodOnTheIce = defaults.bloodOnTheIce;
		start::cleanUpAfterBloodOnTheIce = defaults.cleanUpAfterBloodOnTheIce;
		ApplyLogLevel();
		logger::info("settings restored to the shipped defaults");
	}

	void ApplyLogLevel()
	{
		const auto lvl = static_cast<spdlog::level::level_enum>(std::clamp<std::uint32_t>(debug::logLevel, 0u, 6u));
		SKSE::log::set_level(lvl, lvl);
	}

	const std::string& GetIniPath()
	{
		return iniPath;
	}
}
