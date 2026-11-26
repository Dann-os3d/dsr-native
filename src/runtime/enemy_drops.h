#pragma once
// enemy_drops_100: every item an enemy (or any other item lot) can drop always drops.
//
// ItemLotParam is the game's drop table (ITEMLOT_PARAM_ST: per row 8 item slots with lotItemBasePoint = the chance in
// %). A few seconds after the game starts it has loaded its parameters into memory; a background thread finds the table
// there (whatever way the file was read) and sets lotItemBasePoint = 100 for every slot that holds an item, which is
// what the PC "100% drops" mods do to GameParam. Nothing on disk is touched: turn the setting off and the next start is
// vanilla again.
#include <cstddef>
#include <cstdint>

namespace dsr {

// Starts the background search (settings enemy_drops_100). Returns at once.
void StartEnemyDropsScan();

// Looks for ItemLotParam in the resident pages of [base, base + size) and patches every valid copy. *copies = copies
// found, *slots = item slots changed to 100%. Exposed for the host tests.
void ScanAndPatchItemLots(uintptr_t base, size_t size, size_t* copies, size_t* slots);

}  // namespace dsr
