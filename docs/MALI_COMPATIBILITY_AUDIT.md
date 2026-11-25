# Mali compatibility audit (initial)

## Scope

This fork targets Android ARM64 devices, with the MediaTek Helio G99 / Mali-G57 MC2 as the first validation target. This document records source-level findings before changing Vulkan behavior. It does not claim that Mali support is already working or that any setting has been device-tested.

## Findings in the current source

### Existing configuration surface

`dsr::RuntimeConfig` in `src/runtime/runtime.h` already exposes:

- Frame pacing: `target_fps`, `dynamic_fps`, `min_fps`, `unlocked_fps`.
- Resolution: `render_width`, `render_height`, `render_scale` (documented range 0.5–1.0).
- Quality/performance: `performance_mode`, `lighter_post_fx`, `texture_quality`, `max_anisotropy`, `exposure`.
- Texture compatibility: `bc_etc2`, `bc3_etc2`, `gpu_compression`.
- Vulkan path switches: `mali_barriers`, `render_pass_objects`, `async_pipelines`, `record_thread`, `recorder_copies`, `staging_memory`.

The Android in-game `SettingsMenu` already has Graphics and Driver tabs. Its Graphics tab offers 30/45/60 FPS, auto/unlocked pacing, render scale, performance mode, lighter post effects, BC3-to-ETC2 conversion, a texture quality slider (100% down to 25%), and the Mali barrier option. The Driver tab supports the system driver, bundled Turnip Gen8, and user-imported ZIP/SO files.

### Important compatibility constraints

- The app is currently built only for `arm64-v8a` and requires Android API 29 or newer.
- Turnip is an Adreno driver project; the bundled Turnip package must not be treated as a Mali driver.
- External Vulkan driver loading is mediated by libadrenotools. A driver package being importable does not prove that it is ABI-compatible with a device or safe to load.
- The Vulkan backend already queries the physical device name, Vulkan version, and relevant feature flags. It uses ETC2 as a compressed fallback for supported BC texture formats when native BC sampling is unavailable.
- The Mali barrier path is implemented but opt-in. The render-pass-object path is also configurable for diagnostics/driver workarounds.
- Existing quality switches are coarse-grained. `performance_mode` lowers effect quality and shadow-map size; `lighter_post_fx` disables selected post-processing effects. There are not yet independent user-facing switches for every individual light or shadow feature.

## Recommended implementation order

1. **Diagnostics first:** expose GPU name/vendor/device IDs, Vulkan API/driver versions, BC/ETC2 support, dynamic-rendering availability, and selected compatibility path in the menu and logs.
2. **Safe Mali profile:** add a reversible, explicit Mali preset that changes only settings already supported by the runtime. Do not silently force experimental barriers or driver packages.
3. **Graphics controls:** improve labels and ranges for frame pacing and render scale; expose existing post-FX, mip, anisotropy, and render-pass options with clear restart/live-apply indications.
4. **Vulkan compatibility:** use device capabilities rather than vendor-name assumptions; test ETC2 formats, render-pass fallback, barriers, and synchronization separately.
5. **Performance work:** profile CPU, GPU, memory and pipeline compilation on actual Mali hardware before changing thread scheduling or staging-memory defaults.
6. **Device validation:** run repeatable scenes on Mali-G57 and at least one Adreno device; attach logs and frame-time measurements to each tested build.

## Validation limitations

Source inspection alone cannot establish stable frame rate, visual correctness, thermals, or driver compatibility on a Helio G99. Those require a physical device and representative game data supplied by the owner. CI compilation and host tests are necessary but do not replace on-device Vulkan testing.
