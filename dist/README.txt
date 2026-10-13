Alternate Perspective Hjerim Addon
==================================
Version 1.0.2

An add-on for Alternate Perspective. It adds Windhelm to the "My own Home" card of the menu the
Messenger offers you in the Resting Pilgrim, beside Whiterun, Solitude, Riften and Markarth. Choose it
and the game begins inside Hjerim, the house in Windhelm, already yours: the deed in your name, the
key and the decorating guide in your pack, and every room furnished - kitchen, living room, bedroom,
enchanting and alchemy laboratories, armory and displays, guest room, basement lights. No steward,
no gold.

Hjerim is the Butcher's house, and the game only sells it once Blood on the Ice is closed. By default
the start marks that case solved - the state the game itself reaches when it is closed - and clears
the grisly clutter in the hidden room. The INI can turn the quest off instead, or leave it alone to
play as usual; then the clutter stays until the case is closed, as the steward's clean-up would have
removed it.

Nothing else is touched. The mod edits no cell, no furniture and no dialogue; it works through the
same markers and the same script variable the steward's own sale and room purchases use. A mod that
refurnishes or improves Hjerim sees exactly what it would see after buying every room from the steward.


WHAT IS IN THE PACKAGE
----------------------
  Alternate Perspective Hjerim Addon.esp        one quest record, nothing else (light-flagged)
  SKSE\Plugins\HjerimStart.dll                  the mod
  SKSE\Plugins\HjerimStart.pdb                  debug symbols, so crash logs name this mod
  SKSE\Plugins\HjerimStart.ini                  the settings, with every one explained
  Interface\Translations\HjerimStart_*.txt      the menu text in eleven languages


REQUIREMENTS
------------
  SKSE64
  Address Library for SKSE Plugins
  Alternate Perspective - Alternate Start 4.0 or newer
  JContainers SE - Alternate Perspective's own requirement; without it AP shows only its own starts

  Supported runtimes: Skyrim SE 1.5.97, AE 1.6.1170, and Skyrim 1.7.99 / 1.7.104 - the installer asks
  which one you have and installs the matching DLL.


INSTALLATION
------------
  Install with a mod manager, pick your game version in the installer, and enable the plugin. Load order does not matter - nothing in this mod
  overrides anything.

  The start appears in Alternate Perspective's menu on the next new game.

  Updating from Alternate Perspective Hjerim Start 1.0.0: remove it (and untick its plugin) first. Its
  JSON file is what made Hjerim a card of its own.


SETTINGS
--------
  There is no in-game menu page. Everything is in SKSE\Plugins\HjerimStart.ini, which explains each
  one where it sits:

    bEnabled                     run the start at all
    fStartDelaySeconds           how long to wait after Alternate Perspective hands over
    fStageGapSeconds             the beat between the steps of the start
    bMoveToHjerim                begin inside Hjerim's main hall
    bFurnishEveryRoom            every room furnished, or the basic set only
    uBloodOnTheIce               mark Blood on the Ice solved, turn it off, or leave it alone
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
  "Windhelm is not in the menu."             Check that JContainers SE is installed - without it
                                            Alternate Perspective shows only its own starts.
  "A room is not furnished."                 Another mod has removed or replaced that room's vanilla
                                            furnishing marker; the log names it. That mod's own
                                            furniture is what you see, which is the intended result.


WHAT CHANGED
------------
Version 1.0.2
The option in the "My own Home" card is called Windhelm, like the other cities beside it.
No in-game menu page any more: the mod simply adds the start. Every setting is still in the INI.

Version 1.0.1
The mod is named Alternate Perspective Hjerim Addon, and its plugin with it.
The option now sits inside Alternate Perspective's own "My own Home" card, beside Whiterun, Solitude, Riften and Markarth, instead of being a card of its own. It is added to that card as the menu opens, in your language; no file of Alternate Perspective's is touched, and if a future version has no such card the start appears as its own card instead.
The start settles Blood on the Ice: marked solved by default (the state the game itself reaches when the case is closed), or turned off, or left alone to play as usual. The first two also clear the grisly clutter in the hidden room.


LICENCE
-------
  GPL-3.0-or-later. See LICENSE, NOTICE.md and THIRD_PARTY_NOTICES.md.
  Source: https://github.com/ApocryphaRealm/HjerimStart
