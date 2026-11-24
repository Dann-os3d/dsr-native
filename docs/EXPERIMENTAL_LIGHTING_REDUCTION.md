# Experimental lighting reduction mode (Mali G57)

## Goal

Add an optional graphics setting for devices where GPU-bound scenes (especially boss encounters) remain slow after render-scale reduction. The intended trade-off is a flatter image in exchange for fewer lighting and shadow passes.

This is an experimental optimization, not a guaranteed FPS fix. It must remain opt-in and must not change the default rendering path.

## What the current source already supports

The game hooks currently expose two separate quality paths:

- `performance_mode`: forces the engine's low-quality VFX path and reduces the shadow-map size from 2048 to 512.
- `lighter_post_fx`: disables ambient occlusion, depth of field, camera motion blur and light shafts through checked instruction patches.

These controls do **not** turn off all lighting. In particular, the source audit has not yet identified a verified, safe switch that disables every direct/indirect light contribution or the complete shadow-rendering pass.

## Proposed staged implementation

### Stage A — verified reductions

Create a separate experimental setting (do not overload `performance_mode` or `lighter_post_fx`) that combines only known, individually verified engine controls:

1. Keep low-quality VFX enabled.
2. Disable the already identified post-processing effects.
3. Use the engine's documented minimum shadow-map setting (512; its debug-menu range is 512..2048) after confirming the global is initialized to 2048. The renderer derives target dimensions and texel offsets from this value.
4. Log each applied or skipped patch, including the expected and observed instruction/data values.

The UI wording must say **Reduced lighting** or **Lighting-lite**, not **Lighting off**, until full disablement is proven.

### Stage B — true shadow-pass suppression

Before attempting to disable shadows entirely, reverse-engineer the shadow pass enable path and its dependencies for every supported game build. Verify that skipping it does not leave stale shadow maps, break render-pass dependencies, or cause flicker/black frames. Apply the patch only after all expected instructions and data are validated.

### Stage C — full lighting suppression (optional research)

Identify separate controls for direct lights, ambient/environment lighting, light probes and emissive effects. Do not blindly zero global lighting data or skip generic render passes: those can affect unrelated rendering and may produce missing geometry/materials or unstable Vulkan synchronization.

If a safe global switch does not exist, retain a practical reduced-lighting preset rather than claiming complete engine shutdown.

## Compatibility and safety requirements

- Default OFF; preserve the original visual path unless the user opts in.
- Keep Adreno and system-driver behavior unchanged when the setting is OFF.
- Use per-build offsets and all-or-nothing patch validation, matching the existing hook conventions.
- Never patch an instruction unless its expected original value matches.
- If a patch fails validation, leave that feature enabled and log the reason.
- Do not change Vulkan image barriers or synchronization as part of this feature.
- Preserve gameplay-critical visibility and effects where possible (boss telegraphs, player/enemy effects, bonfires and interactable cues).

## Validation plan on Mali-G57 MC2 / Helio G99

Use the same save, location, camera direction, frame cap and driver for each comparison:

1. Baseline: current graphics settings.
2. Existing low-quality VFX + lighter post FX.
3. Stage A reduced-lighting preset.
4. Stage B shadow suppression only, if implemented and validated.

Record average FPS, 1% lows/frame-time spikes, GPU busy time, memory/transfer activity, flicker/black frames and visual regressions. Test a normal exploration area, a boss encounter, a bonfire and a transition/loading scene.

A result is useful only if it improves boss-scene frame pacing without introducing visual corruption. Keep the feature experimental until repeatable A/B results exist.

## Current status

### Stage A — implemented in source, awaiting build/device validation

The branch now includes an opt-in `lighting_lite` setting in the Graphics tab. It is parsed from `settings.txt` and, when enabled, applies the existing checked `ApplyPerformanceMode` and `ApplyLighterPostFx` patches together. It does not change the default path when OFF.

The preset currently means:
- low-quality VFX path;
- shadow map size reduced from 2048 to 512 where the expected game data is present;
- ambient occlusion, depth of field, camera motion blur and light shafts disabled.

This is **not** full lighting shutdown and does not skip the shadow-rendering pass. No APK build or physical-device test has yet been confirmed for this implementation.

### Remaining work

- Confirm the Android/native build and host tests pass.
- Test on Mali-G57 MC2, comparing the same exploration and boss scenes with the preset OFF/ON.
- Inspect frame pacing, GPU busy time, flicker/black frames and visual regressions.
- Only then research deeper shadow-pass suppression and separate lighting systems.


### Stage A2 — minimum documented shadow-map resolution

The Lighting-lite preset now selects 512x512 shadow maps instead of 1024x1024. The game build metadata documents 512..2048 as the supported debug-menu range, and the renderer sizes its shadow target and texel offsets from this global. The patch remains conditional: it writes 512 only when the expected global is inside the mapped main module and currently equals 2048. It does not suppress shadow rendering. The Android build and device behavior for this change still require validation.

### Stage B investigation — current limitation

The current hook metadata exposes a validated shadow-map size global and a separate LightShaft draw-group switch, but it does not expose a verified shadow-pass enable flag or a complete list of shadow-pass dependencies. The Vulkan backend source handles generic image transitions and render-pass execution; it does not provide a game-specific semantic switch for the shadow pass. Therefore, disabling shadows by suppressing generic Vulkan passes or guessing another game-code offset would be unsafe and is not being committed. A real Stage B patch requires disassembly of the exact supported game binaries/build IDs and confirmation of the pass mask, resource lifetime, and frame-to-frame shadow target behavior.


### Stage B (backend) — `lighting_off`: shadow passes are not drawn

The Vulkan backend can do what the game binary could not be audited for. A shadow map is a depth-only pass (no pixel program, no colour target) into a square, power-of-two depth image of 512..2048 pixels (the engine's `ShadwoMap Size` range; 2048 raw is the 16 MB depth image the backend already sizes for). With `lighting_off = 1` such a pass is dropped in `Draw`, before its render pass starts, and the shadow samplers use `VK_COMPARE_OP_NEVER` (the same result as the `shadow_compare = 3` diagnostic: "lit"), whatever the undrawn map holds. The detection is a pure function (`IsShadowMapPass`, `hle/nvn/shadow_pass.h`) with host tests.

`lighting_off` also applies everything `lighting_lite` does (low-quality effects, 512 shadow maps as a fallback if a pass is not recognised, no ambient occlusion / depth of field / camera motion blur / light shafts).

What it does **not** do: direct, ambient and probe lighting inside the objects' own shaders stay as they are. There is still no verified game-code switch for them, and this feature deliberately adds no unverified offsets.

Validation still needed on a device: that no square depth-only pass other than the shadow map exists (the log prints the size of the first skipped pass: `lighting_off: first shadow map pass skipped (depth image WxH)`; anything other than 2048/1024/512 square is a misdetection), no flicker or black frames, and the FPS difference with the setting OFF/ON. The setting is read at start-up and is OFF by default.
