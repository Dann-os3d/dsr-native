#include "runtime/enemy_drops.h"

#include <sys/mman.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "common/log.h"
#include "hle/os/heap.h"

namespace dsr {
namespace {
constexpr const char* kTag = "drops";

// PARAM as the game keeps it in memory (the file's layout): u32 string table offset, u16 offset of the first row's
// data, u16 -, u16 version, u16 row count, 32-byte type name at 0x0C, row table { id, data offset, name offset } at 0x30.
// ITEMLOT_PARAM_ST rows are 148 bytes: 8 x s32 lotItemId, 8 x s32 lotItemCategory, 8 x u16 lotItemBasePoint, ...
constexpr char kType[] = "ITEMLOT_PARAM_ST";
constexpr size_t kTypeOffset = 0x0C, kRowTable = 0x30, kRowSize = 148, kSlots = 8, kBasePoint = 64;
constexpr int kPageShift = 12;

uint32_t Le32(const uint8_t* p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
uint16_t Le16(const uint8_t* p) { return uint16_t(p[1] << 8 | p[0]); }

struct Counts {
    size_t copies = 0, slots = 0, rejected = 0;
};

// One candidate: `hdr` is where the PARAM header would start, [hdr, end) is resident memory.
void TryPatch(uint8_t* hdr, const uint8_t* end, Counts* c) {
    if (size_t(end - hdr) < kRowTable) return;
    const uint32_t strings = Le32(hdr);
    const uint32_t first = Le16(hdr + 4), rows = Le16(hdr + 10);
    if (rows == 0 || first != kRowTable + rows * 12u || strings < first ||
        size_t(strings - first) != size_t(rows) * kRowSize || size_t(end - hdr) < strings) {
        if (c->rejected++ < 3)
            LOGW(kTag, "enemy_drops_100: \"%s\" at %p is not laid out as expected (strings %u, first row %u, %u rows, %zu bytes resident)",
                 kType, static_cast<void*>(hdr), strings, first, rows, size_t(end - hdr));
        return;
    }
    size_t changed = 0;
    for (uint32_t r = 0; r < rows; r++) {
        const uint32_t data = Le32(hdr + kRowTable + size_t(r) * 12 + 4);
        if (data < first || size_t(data) + kRowSize > strings) return;  // not a table after all
        uint8_t* row = hdr + data;
        for (size_t s = 0; s < kSlots; s++) {
            const int32_t id = int32_t(Le32(row + s * 4));
            if (id <= 0) continue;  // empty slot
            uint8_t* bp = row + kBasePoint + s * 2;
            if (Le16(bp) != 100) {
                bp[0] = 100;
                bp[1] = 0;
                changed++;
            }
        }
    }
    c->copies++;
    c->slots += changed;
}

void ScanRun(uint8_t* a, uint8_t* b, Counts* c) {
    const size_t len = sizeof(kType) - 1;
    uint8_t* p = a;
    while (size_t(b - p) >= len) {
        auto* hit = static_cast<uint8_t*>(memmem(p, size_t(b - p), kType, len));
        if (!hit) return;
        if (hit - a >= ptrdiff_t(kTypeOffset)) TryPatch(hit - kTypeOffset, b, c);
        p = hit + 1;
    }
}
}  // namespace

void ScanAndPatchItemLots(uintptr_t base, size_t size, size_t* copies, size_t* slots) {
    Counts c;
    const size_t page = size_t(1) << kPageShift;
    const size_t chunk = size_t(64) << 20;  // pages asked about at a time
    std::vector<unsigned char> vec(chunk >> kPageShift);
    uint8_t* run = nullptr;  // start of the current run of resident pages
    for (size_t off = 0; off < size; off += chunk) {
        const size_t n = size - off < chunk ? size - off : chunk;
        uint8_t* at = reinterpret_cast<uint8_t*>(base + off);
        if (mincore(at, n, vec.data()) != 0) {
            if (run) ScanRun(run, at, &c);
            run = nullptr;
            continue;
        }
        for (size_t i = 0; i < (n + page - 1) / page; i++) {
            uint8_t* pg = at + i * page;
            if (vec[i] & 1) {
                if (!run) run = pg;
            } else if (run) {
                ScanRun(run, pg, &c);
                run = nullptr;
            }
        }
    }
    if (run) ScanRun(run, reinterpret_cast<uint8_t*>(base + size), &c);
    if (copies) *copies = c.copies;
    if (slots) *slots = c.slots;
}

void StartEnemyDropsScan() {
    std::thread([] {
        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();
        LOGI(kTag, "enemy_drops_100: looking for ItemLotParam in the game's memory");
        bool found = false;
        int after = 0;
        for (int pass = 1;; pass++) {
            std::this_thread::sleep_for(std::chrono::seconds(pass <= 10 ? 3 : 5));
            const double since = std::chrono::duration<double>(clock::now() - t0).count();
            uintptr_t base = 0;
            size_t size = 0;
            hle::os::GuestHeapRange(&base, &size);
            if (!base || !size) continue;  // the game has not set up its heap yet
            const auto s0 = clock::now();
            size_t copies = 0, slots = 0;
            ScanAndPatchItemLots(base, size, &copies, &slots);
            const long ms = long(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - s0).count());
            if (copies > 0) {
                if (slots > 0 || !found)
                    LOGI(kTag, "enemy_drops_100: %zu copy(ies) of ItemLotParam found, %zu item slots set to 100%% (pass %d, %.0f s after start, scan %ld ms)",
                         copies, slots, pass, since, ms);
                found = true;
                if (++after >= 4) break;  // a few more passes in case the table is loaded again
            } else if (since > 300) {
                LOGW(kTag, "enemy_drops_100: ItemLotParam was not found in the game's memory in %.0f s (%d passes, last scan %ld ms): drops are unchanged",
                     since, pass, ms);
                break;
            }
        }
    }).detach();
}

}  // namespace dsr
