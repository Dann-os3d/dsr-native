#pragma once
// enemy_drops_100: every item an enemy (or any other item lot) can drop always drops.
//
// The game reads GameParam.parambnd.dcx out of dvdbnd0.bdt in one piece. When the setting is on, fs.cpp hands that
// buffer to PatchGameParamDrops, which rewrites ItemLotParam in place: every slot that holds an item gets
// lotItemBasePoint = 100 (the same change the PC "100% drops" mods make to the file). Nothing on disk is touched:
// turn the setting off and the next start reads the original data again.
#include <cstddef>
#include <cstdint>

namespace dsr::hle {

enum class DropPatch {
    NotGameParam,  // not the GameParam DCX (the buffer is untouched)
    Patched,       // ItemLotParam rewritten in the buffer
    AlreadyFull,   // every slot already had 100% (the buffer is untouched)
    Failed,        // it is the GameParam DCX but could not be patched (buffer untouched); *why says the reason
};

// `buf` holds a file read from a .bdt, `size` bytes. On Patched / AlreadyFull *slots is the number of slots that
// changed / that hold an item. `why` (optional) receives a short reason for Failed.
DropPatch PatchGameParamDrops(uint8_t* buf, size_t size, size_t* slots, const char** why);

}  // namespace dsr::hle
