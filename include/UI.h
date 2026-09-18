#pragma once

// This mod's pages inside the Apocrypha Menu Framework - the standing rule for every mod here is that
// its settings live in that menu rather than in an overlay of its own.
//
// The framework is not a hard requirement: with it missing the mod behaves exactly as its INI says and
// simply has no page. That is logged once, not warned about repeatedly.

namespace UI
{
	void Register() noexcept;

	namespace SettingsPanel
	{
		void __stdcall Render();
	}

	namespace StatusPanel
	{
		void __stdcall Render();
	}
}
