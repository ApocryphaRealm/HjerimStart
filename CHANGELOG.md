# Alternate Perspective Hjerim Start - changelog

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
