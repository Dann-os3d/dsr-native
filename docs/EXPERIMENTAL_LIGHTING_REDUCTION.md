# Experimental lighting reduction mode (Mali G57)

## Goal

Add an optional graphics setting for devices where GPU-bound scenes (especially boss encounters) remain slow after render-scale reduction. The intended trade-off is a flatter image in exchange for fewer lighting and shadow passes.

This is an experimental optimization, not a guaranteed FPS fix. It must remain opt-in and must not change the default rendering path.

## What the current source already supports

The game hooks currently expose two separate quality paths:

- `performance_mode`: forces the engine's low-quality VFX path and reduces the shadow-map size from 2048 to 1024.
- `lighter_post_fx`: disables ambient occlusion, depth of field, camera motion blur and light shafts through checked instruction patches.

These controls do **not** turn off all lighting. In particular, the source audit has not yet identified a verified, safe switch that disables every direct/indirect light contribution or the complete shadow-rendering pass.

## Proposed staged implementation

### Stage A — verified reductions

Create a separate experimental setting (do not overload `performance_mode` or `lighter_post_fx`) that combines only known, individually verified engine controls:

1. Keep low-quality VFX enabled.
2. Disable the already identified post-processing effects.
3. Reduce shadow-map resolution further only after confirming the engine accepts the target value and updates its shadow target/texel calculations consistently.
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

Design/research proposal only. No lighting-disable patch has been applied yet. The existing controls are documented above; complete lighting/shadow shutdown still requires reverse-engineering and device validation.
