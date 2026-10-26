# Alternate Perspective Hjerim Addon - changelog

## 1.0.3 - 2026-10-25 - untested

### Fixed
- **Adding the start to Alternate Perspective's menu now talks to the menu only on the UI thread.** Each look at AP's
  option list (GetVariable / GetMember / PushBack on its movie) was queued through SKSE's AddTask, whose queue on Skyrim
  1.5.97 runs beside the UI job that advances the menu's movie, so the calls raced the movie itself - the shape that
  crashed Dragon's Eye Minimap 1.7.5 (2026-10-25). Each pass is now an SKSE UI task. The retries while AP reads its
  files are paced by a short-lived thread of the mod's, one pass about every 16 ms (300 passes, about five seconds,
  as before), posting the next only after the last has run - SKSE drains its UI queue in a loop, so a UI task that
  queued the next pass itself would run them all in one frame. That thread never touches the movie. What is added
  to the menu, and where, is unchanged. Gate rows scaleform-calls-only-on-the-ui-thread and
  scaleform-helpers-only-on-the-ui-thread pass.

## 1.0.2 - 2026-10-13 - working

### Changed
- **The log ships at info** (uLogLevel=2, compiled and in the INI), the standing default since the owner's 2026-09-26 rule; it shipped at trace. For a bug report, set uLogLevel=0, reproduce, and send the log.
- **The option in Alternate Perspective's "My own Home" card is called Windhelm**, like Whiterun, Solitude, Riften and Markarth beside it, instead of Hjerim (the owner, 2026-10-13). AP's suboptions carry no description line, so the house is named only on the fallback card of our own, which keeps the Hjerim label.

### Removed
- **No in-game menu page.** The Apocrypha Menu Framework page (Settings and Status) is gone; the mod simply adds the start (the owner, 2026-10-02: Alternate Perspective mods "should just be a simple mod that adds what is intended and not an AMF page"). Every setting is still in `HjerimStart.ini` and is read the same way; the page-only text is gone from the translation files.

## 1.0.1 - 2026-09-18 - untested

### Changed
- **The mod is Alternate Perspective Hjerim Addon** (the owner, 2026-09-18). The plugin is
  `Alternate Perspective Hjerim Addon.esp`; the DLL, INI and log keep the `HjerimStart` name they load under.
- **The option lives in Alternate Perspective's own "My own home" card**, beside Whiterun, Solitude, Riften and
  Markarth - the four houses vanilla lets you own without a quest (the owner: *"it needs its selection option added to
  the my own home area of alt perspective in the starting room"*). AP pushes every add-on file's entries into one list
  and merges nothing, so a JSON of our own could only ever be a separate card; this build ships no JSON and adds the
  option to that card in the menu itself when it opens, in your language. If a future AP has no such card, the start is
  added as a card of its own instead, and the settings page's Status tab says which happened.
- **Blood on the Ice is settled by the start** (the owner: *"hjerim should either deactivate the blood on the ice quest
  or set it as complete"*). New setting `uBloodOnTheIce`: mark it solved (the default - the state the game itself
  reaches when the case is closed, which is what makes the house ownable), turn it off, or leave it alone and play the
  case as usual. The first two also clear the grisly clutter in the hidden room, so the house you arrive in is clean.

## 1.0.0 - 2026-09-18 - untested

First version. One start for Alternate Perspective's menu: Hjerim, the house in Windhelm, already the
player's.

- **The sale, without the steward.** The Hjerim cell is set to the player's faction, the house key and
  the decorating guide go into the pack, the HousePurchase quest's `WindhelmHouseVar` is set as an
  owned house has it, and "Houses Owned" is counted - the same steps `HousePurchaseScript.PurchaseHouse`
  and the steward's sale fragment take, minus the gold.
- **Every room furnished** (the owner, 2026-09-18: *"It should be fully furnished."*): the abandoned
  clutter is disabled and the basic furnishing, the player's chest and every room the steward sells are
  enabled through the game's own furnishing markers, one room per frame beat.
- **Blood on the Ice untouched.** The quest starts from its own triggers as always; the Butcher's clutter
  - its evidence - stays until the quest is completed, then goes as the steward's clean-up would have
  removed it. The mod remembers in the SKSE co-save that it handed the house over, so a player who bought
  Hjerim the ordinary way keeps the steward's own clean-up.
- **Compatible with house overhauls** (the owner: *"compatible with mods that refurnish or improve the
  house"*): the plugin carries one quest record and edits no cell, reference or global; a marker another
  mod has removed or deleted is skipped and named in the log.
- Settings page in the Apocrypha Menu Framework (Settings and Status), INI beside the DLL, the card and
  the page in eleven languages, DevBench tool `hjerimstart.start`, Address Library pre-check before
  `SKSE::Init`.
