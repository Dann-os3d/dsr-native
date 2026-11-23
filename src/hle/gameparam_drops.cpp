#include "hle/gameparam_drops.h"

#include <cstring>
#include <vector>

#if __has_include(<zlib.h>)
#include <zlib.h>
#define DSR_HAVE_ZLIB 1
#endif

namespace dsr::hle {
namespace {

// DCX (DFLT) header as FromSoftware writes it: "DCX\0", sizes at 0x1C (inflated) and 0x20 (deflated), "DFLT" at
// 0x28, "DCA\0" at 0x44 and the zlib stream at 0x4C.
constexpr size_t kDcxHeader = 0x4C;
constexpr uint32_t kMaxInflated = 64u << 20;
// ITEMLOT_PARAM_ST: 8 x s32 lotItemId, 8 x s32 lotItemCategory, 8 x u16 lotItemBasePoint, ... 148 bytes per row.
constexpr size_t kRowSize = 148, kSlots = 8, kBasePointOffset = 64;

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint32_t Le32(const uint8_t* p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
uint16_t Le16(const uint8_t* p) { return uint16_t(p[1] << 8 | p[0]); }
void PutLe16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }

bool HasName(const uint8_t* d, size_t n, const char* name) {
    const size_t len = strlen(name);
    for (size_t i = 0; i + len <= n; i++)
        if (d[i] == uint8_t(name[0]) && memcmp(d + i, name, len) == 0) return true;
    return false;
}

#ifdef DSR_HAVE_ZLIB
// Inflates up to `cap` bytes of the stream (all of it when it is shorter).
size_t InflatePrefix(const uint8_t* src, size_t n, uint8_t* dst, size_t cap) {
    z_stream z;
    memset(&z, 0, sizeof z);
    if (inflateInit(&z) != Z_OK) return 0;
    z.next_in = const_cast<uint8_t*>(src);
    z.avail_in = uInt(n);
    z.next_out = dst;
    z.avail_out = uInt(cap);
    inflate(&z, Z_SYNC_FLUSH);
    const size_t got = cap - z.avail_out;
    inflateEnd(&z);
    return got;
}

// The smallest zlib stream (level 9; the memory level changes the size by a few KB either way) of data, or empty.
std::vector<uint8_t> Deflate(const uint8_t* data, size_t n) {
    std::vector<uint8_t> best;
    for (int mem_level : {8, 9, 7}) {
        z_stream z;
        memset(&z, 0, sizeof z);
        if (deflateInit2(&z, 9, Z_DEFLATED, 15, mem_level, Z_DEFAULT_STRATEGY) != Z_OK) continue;
        std::vector<uint8_t> out(deflateBound(&z, uLong(n)));
        z.next_in = const_cast<uint8_t*>(data);
        z.avail_in = uInt(n);
        z.next_out = out.data();
        z.avail_out = uInt(out.size());
        const int r = deflate(&z, Z_FINISH);
        const size_t used = out.size() - z.avail_out;
        deflateEnd(&z);
        if (r == Z_STREAM_END && (best.empty() || used < best.size())) {
            out.resize(used);
            best.swap(out);
        }
    }
    return best;
}
#endif

}  // namespace

DropPatch PatchGameParamDrops(uint8_t* buf, size_t size, size_t* slots, const char** why) {
    auto fail = [&](const char* msg) {
        if (why) *why = msg;
        return DropPatch::Failed;
    };
    if (slots) *slots = 0;
    if (size < kDcxHeader + 16 || memcmp(buf, "DCX\0", 4) != 0 || memcmp(buf + 0x28, "DFLT", 4) != 0 ||
        memcmp(buf + 0x44, "DCA\0", 4) != 0)
        return DropPatch::NotGameParam;
    const uint32_t inflated = Be32(buf + 0x1C), deflated = Be32(buf + 0x20);
    if (inflated < 0x100 || inflated > kMaxInflated || deflated == 0 || kDcxHeader + size_t(deflated) > size)
        return DropPatch::NotGameParam;

#ifndef DSR_HAVE_ZLIB
    (void)inflated;
    return fail("built without zlib");
#else
    // Cheap test first: the BND3 header and its name table are in the first few KB. Most DCX files are not this one.
    uint8_t head[4096];
    const size_t hn = InflatePrefix(buf + kDcxHeader, deflated, head, sizeof head);
    if (hn < 0x30 || memcmp(head, "BND3", 4) != 0 || !HasName(head, hn, "GameParam\\ItemLotParam.param"))
        return DropPatch::NotGameParam;

    std::vector<uint8_t> bnd(inflated);
    if (InflatePrefix(buf + kDcxHeader, deflated, bnd.data(), bnd.size()) != bnd.size()) return fail("inflate failed");

    // BND3 as the game ships it: flags 0x74, little endian, 24-byte entries
    // { flags, size, offset, id, name offset, inflated size } after a 0x20-byte header.
    if (bnd[0x0C] != 0x74 || bnd[0x0D] != 0) return fail("unexpected BND3 flags");
    const uint32_t count = Le32(&bnd[0x10]);
    if (count == 0 || 0x20 + size_t(count) * 24 > bnd.size()) return fail("bad BND3 entry table");
    uint8_t* param = nullptr;
    size_t param_size = 0;
    for (uint32_t i = 0; i < count && !param; i++) {
        const uint8_t* e = &bnd[0x20 + size_t(i) * 24];
        const uint32_t esize = Le32(e + 4), eoff = Le32(e + 8), noff = Le32(e + 16);
        if (noff >= bnd.size() || size_t(eoff) + esize > bnd.size()) return fail("bad BND3 entry");
        if (HasName(&bnd[noff], strnlen(reinterpret_cast<const char*>(&bnd[noff]), bnd.size() - noff),
                    "GameParam\\ItemLotParam.param")) {
            param = &bnd[eoff];
            param_size = esize;
        }
    }
    if (!param) return fail("ItemLotParam not found");

    // PARAM: u32 string table offset, u16 first row data offset, u16 -, u16 version, u16 row count,
    // 32-byte param type name at 0x0C, row table { id, data offset, name offset } at 0x30.
    if (param_size < 0x30) return fail("short PARAM");
    const uint32_t strings = Le32(param);
    const uint32_t first = Le16(param + 4), rows = Le16(param + 10);
    if (memcmp(param + 0x0C, "ITEMLOT_PARAM_ST", 17) != 0) return fail("not ITEMLOT_PARAM_ST");
    if (rows == 0 || first != 0x30 + rows * 12u || strings > param_size || strings < first ||
        size_t(strings - first) != size_t(rows) * kRowSize)
        return fail("unexpected ItemLotParam row layout");

    size_t with_item = 0, changed = 0;
    for (uint32_t r = 0; r < rows; r++) {
        const uint32_t data = Le32(param + 0x30 + size_t(r) * 12 + 4);
        if (size_t(data) + kRowSize > param_size) return fail("row outside ItemLotParam");
        uint8_t* row = param + data;
        for (size_t s = 0; s < kSlots; s++) {
            const int32_t id = int32_t(Le32(row + s * 4));
            if (id <= 0) continue;  // empty slot
            with_item++;
            uint8_t* bp = row + kBasePointOffset + s * 2;
            if (Le16(bp) != 100) {
                PutLe16(bp, 100);
                changed++;
            }
        }
    }
    if (slots) *slots = with_item;
    if (changed == 0) return DropPatch::AlreadyFull;

    // Back into the same space: the game read exactly the original DCX, so the new stream has to fit in it. The
    // header keeps its sizes; whatever is left after the end of the zlib stream is zero (inflate stops at the end).
    const std::vector<uint8_t> out = Deflate(bnd.data(), bnd.size());
    if (out.empty()) return fail("deflate failed");
    if (out.size() > deflated) return fail("recompressed data does not fit");
    memcpy(buf + kDcxHeader, out.data(), out.size());
    memset(buf + kDcxHeader + out.size(), 0, deflated - out.size());
    if (slots) *slots = changed;
    return DropPatch::Patched;
#endif
}

}  // namespace dsr::hle
