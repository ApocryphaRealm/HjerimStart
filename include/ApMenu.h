#pragma once

// Our option inside Alternate Perspective's own menu.
//
// AP's menu (Interface\AlternatePerspectiveMenu.swf, class MessengerMenu) is handed the list of add-on
// JSON files and pushes EVERY entry of every file into one array, `_root.main._options`; the option list
// then shows one card per entry, and a card's suboptions are read from the entry when the card is
// highlighted. There is no merging by name, so a JSON of our own could only ever be a separate card -
// and the owner wants Hjerim where the four vanilla-ownable houses are, in AP's "My own home" card
// (2026-09-18). So this mod ships no JSON at all: when the menu opens, we find that card in the movie and
// add one suboption to it - text in the player's language, our quest's FormID, our plugin's name - which
// is exactly what AP's own Whiterun / Solitude / Riften / Markarth entries carry. Nothing of AP's is
// edited on disk, and a future AP that renames or drops the card gets a card of our own instead.

#include <string>

namespace apmenu
{
	// Start listening for AP's menu opening. Called at kDataLoaded; safe to call twice.
	void Install();

	// What the last attempt did, for the settings page and the DevBench tool.
	struct Report
	{
		bool sawMenu = false;       // AP's option array was found in the movie
		bool injected = false;      // the suboption reached AP's "My own home" card
		bool ownCardAdded = false;  // the fallback: a card of our own was added instead
		int passes = 0;             // how many frames it took to see the options
		int cards = 0;              // how many cards the menu had
		std::string problem;        // empty when it went in
	};

	const Report& Last();
	std::string StateJson();
}
