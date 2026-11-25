// lighting_off: which render passes are shadow maps.
#pragma once

namespace dsr::hle::nvn {

// The game's shadow map is a square depth image whose side is a power of two in the engine's debug-menu range
// (ShadwoMap Size, 512..2048; 2048 by default, 512 with performance mode / lighting_lite). It is drawn by a depth-only
// pass: no pixel program and no colour target. The screen's depth prepass, the half / quarter size particle depth
// buffers and every colour pass are not square, or have a pixel program, so they never match.
inline bool IsShadowMapSize(int width, int height) {
    return width == height && width >= 512 && width <= 2048 && (width & (width - 1)) == 0;
}

// `has_pixel_program`: a pixel program is bound; `colour_targets`: colour attachments that are bound and usable.
inline bool IsShadowMapPass(int depth_width, int depth_height, bool has_pixel_program, int colour_targets) {
    return !has_pixel_program && colour_targets == 0 && IsShadowMapSize(depth_width, depth_height);
}

}  // namespace dsr::hle::nvn
