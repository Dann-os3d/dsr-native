# Performance options and quick-access UI roadmap

This document separates controls that the current runtime already exposes from ideas that need engine work or device validation. A visible setting must never be presented as working unless the runtime actually applies it.

## In-game quick access

- Add a compact gear button at the top-right of the touch overlay.
- Keep the shortcut available when on-screen game controls are hidden or disabled, including while using a physical controller.
- Keep it outside the editable touch-control layout so moving game buttons cannot make settings inaccessible.
- The shortcut opens the existing SettingsMenu; the existing pause/hold behavior remains authoritative.

## Options already backed by runtime settings

These are candidates for clearer grouping and better descriptions in the existing Graphics tab, not claims of newly implemented engine features:

- FPS cap: 30 / 45 / 60, automatic dynamic pacing, and experimental unlocked pacing.
- Render scale and performance preset.
- Reduced post-processing.
- Texture quality slider (`texture_quality`, 25 / 50 / 75 / 100 %): big BC textures are loaded without their most detailed mip level(s), so they are never read, converted, uploaded or sampled. Less CPU work, GPU memory and bandwidth, not only a different look. Applied at the next start; each texture keeps at least 256 texels on its smaller side. Replaces the old on/off `texture_mip_skip` (a file with only that key is read as 50 %).
- BC3-to-ETC2 texture conversion where supported.
- Mali-specific barrier path and render-pass-object fallback.
- Presentation mode (FIFO / mailbox).
- Anisotropy limit, staging-memory policy, async pipeline compilation, command-recording thread, and GPU compression where the current menu/runtime supports them.

## Additional controls to investigate

| Option | Potential benefit | Engineering / validation required |
| --- | --- | --- |
| Resolution scale presets (50%, 60%, 75%, 85%, 100%) | Reduce fragment and bandwidth cost | Verify live vs restart behavior; protect aspect ratio and UI |
| Dynamic resolution | Adapt GPU load to a frame-time target | Requires stable GPU timing, hysteresis, and safe render-target recreation |
| Shadow quality / shadow-map resolution | Reduce memory and GPU cost | Identify every shadow allocation and check for game-side assumptions |
| Draw distance / LOD | Reduce vertex and fragment work | Only if the game exposes safe LOD or culling controls |
| Texture filtering presets | Trade sharpness for sampling cost | Map to supported Vulkan sampler settings; preserve UI readability |
| Effects quality (AO, DOF, motion blur, light shafts) | Reduce expensive post-processing | Existing combined toggle should be split only if independent engine switches exist |
| Frame pacing target and latency mode | Improve consistency or input latency | Benchmark FIFO/mailbox/immediate on each driver; avoid claiming lower latency without measurements |
| Thermal-aware preset | Reduce sustained heat and throttling | Use Android thermal status APIs and conservative hysteresis; do not override system thermal limits |
| CPU worker / thread profile | Reduce contention on heterogeneous CPUs | Device-specific profiling; avoid pinning threads by default |
| Shader cache controls | Reduce repeat stutter and diagnose cache problems | Safe cache size/clear UI and robust invalidation |
| GPU compatibility profile | Apply tested driver-specific workarounds | Profiles must be based on GPU/driver identifiers and have a safe system-driver fallback |

## Mali-G57 / Helio G99 validation profile

The first target is Mali-G57 MC2 on Helio G99. Compare the system Vulkan driver with each user-supplied driver only when it is ABI-compatible and trusted. Turnip is an Adreno driver and is not a Mali solution.

Capture, for each run:
- GPU name/vendor/device ID, Vulkan API and driver versions.
- Selected compatibility path and relevant Vulkan feature support.
- Resolution scale, FPS cap, average and percentile frame times.
- Visual correctness (flicker, missing effects, broken textures, black frames).
- Memory use, device temperature / thermal status, and sustained performance.

Do not promote an experimental setting to a recommended preset until it passes repeatable gameplay tests on a physical device. Keep a one-tap reset to safe defaults.

## Scope boundaries

The quick-access button is a UI change. The additional performance controls above are proposals until the engine exposes a reliable implementation and tests demonstrate that they work. Never expose arbitrary Vulkan driver libraries as safe merely because they can be imported.
