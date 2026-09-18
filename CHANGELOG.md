# Alternate Perspective Hjerim Addon - changelog

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
