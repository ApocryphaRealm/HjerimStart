Alternate Perspective Hjerim Start
==================================
Version 1.0.1

An add-on for Alternate Perspective. It adds a "Hjerim" card to the menu the Messenger offers you in
the Resting Pilgrim. Choose it and the game begins inside Hjerim, the house in Windhelm, already
yours: the deed in your name, the key and the decorating guide in your pack, and every room
furnished - kitchen, living room, bedroom, enchanting and alchemy laboratories, armory and displays,
guest room, basement lights. No steward, no gold.

Hjerim is the Butcher's house, and Blood on the Ice still happens as it always does: walk Windhelm's
streets at night and the case opens. The grisly clutter in the hidden room is the quest's evidence,
so it stays until the case is closed - then it goes, as the steward's clean-up would have removed it.

Nothing else is touched. The mod edits no cell, no furniture and no dialogue; it works through the
same markers and the same script variable the steward's own sale and room purchases use. A mod that
refurnishes or improves Hjerim sees exactly what it would see after buying every room from the steward.


WHAT IS IN THE PACKAGE
----------------------
  Alternate Perspective Hjerim Start.esp        one quest record, nothing else (light-flagged)
  SKSE\Plugins\HjerimStart.dll                  the mod
  SKSE\Plugins\HjerimStart.pdb                  debug symbols, so crash logs name this mod
  SKSE\Plugins\HjerimStart.ini                  the settings, with every one explained
  SKSE\AlternatePerspective\HjerimStart.json    the start, as Alternate Perspective reads it
  Interface\Translations\HjerimStart_*.txt      the card and the settings page in eleven languages


REQUIREMENTS
------------
  SKSE64
  Address Library for SKSE Plugins
  Alternate Perspective - Alternate Start 4.0 or newer
  JContainers SE - Alternate Perspective's own requirement; without it AP shows only its own starts

  Supported runtimes: Skyrim SE 1.5.97, AE 1.6.1170, and Skyrim 1.7.99 / 1.7.104 - the installer asks
  which one you have and installs the matching DLL.

  Optional: the Apocrypha Menu Framework (or SKSE Menu Framework), which is where this mod's settings
  page appears. Without one the mod reads its INI and works the same; it simply has no page.


INSTALLATION
------------
  Install with a mod manager, pick your game version in the installer, and enable the plugin. Load order does not matter - nothing in this mod
  overrides anything.

  The start appears in Alternate Perspective's menu on the next new game.


SETTINGS
--------
  Everything is on this mod's page in the Apocrypha Menu Framework, and in
  SKSE\Plugins\HjerimStart.ini, which explains each one where it sits:

    bEnabled                     run the start at all
    fStartDelaySeconds           how long to wait after Alternate Perspective hands over
    fStageGapSeconds             the beat between the steps of the start
    bMoveToHjerim                begin inside Hjerim's main hall
    bFurnishEveryRoom            every room furnished, or the basic set only
    bCleanUpAfterBloodOnTheIce   clear the Butcher's mess once the quest is completed
    uLogLevel                    how much detail reaches the log


IF SOMETHING GOES WRONG
-----------------------
  The log is at
  Documents\My Games\Skyrim Special Edition\SKSE\HjerimStart.log
  and it ships at full detail, so it already contains everything needed.

  "I arrived back in the Resting Pilgrim."  Raise fStartDelaySeconds a little.
  "I ended up in Helgen."                   Lower it. Alternate Perspective has a safety net that
                                            moves you to the Helgen inn if a start takes too long,
                                            and it fired before this mod acted.
  "The start is not in the menu."            Check that JContainers SE is installed - without it
                                            Alternate Perspective shows only its own starts.
  "A room is not furnished."                 Another mod has removed or replaced that room's vanilla
                                            furnishing marker; the log names it. That mod's own
                                            furniture is what you see, which is the intended result.


WHAT CHANGED
------------
Version 1.0.1
The mod is named Alternate Perspective Hjerim Addon, and its plugin with it.
The option now sits inside Alternate Perspective's own "My own home" card, beside Whiterun, Solitude, Riften and Markarth, instead of being a card of its own. It is added to that card as the menu opens, in your language; no file of Alternate Perspective's is touched, and if a future version has no such card the start appears as its own card instead.
The start settles Blood on the Ice: marked solved by default (the state the game itself reaches when the case is closed), or turned off, or left alone to play as usual - a new setting on the page and in the INI. The first two also clear the grisly clutter in the hidden room.


LICENCE
-------
  GPL-3.0-or-later. See LICENSE, NOTICE.md and THIRD_PARTY_NOTICES.md.
  Source: https://github.com/ApocryphaRealm/HjerimStart
