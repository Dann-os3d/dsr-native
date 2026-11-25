# Mali-G57 flicker and performance investigation

Target under test: MediaTek Helio G99 / Mali-G57 MC2. This document records code-level findings and a conservative test plan. It does not claim device validation.

## User-observed symptoms

- System Vulkan driver: at or below roughly 30 FPS.
- PanVK: roughly 15–18 FPS.
- Frequent visual flicker / intermittent black frames.
- Severe frame-rate drops in boss encounters.

Treat visual correctness as a release gate: do not trade flicker or corrupted frames for a higher FPS counter.

## Existing backend mechanisms found in source

- `src/hle/nvn/vk_backend.cpp` uses a dedicated recorder/presentation path, swapchain image acquisition, transfer/blit into the swapchain, and `vkQueuePresentKHR`.
- `CmdImageBarrier` currently uses broad `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` stages for swapchain image transitions.
- The backend has per-image layout tracking, pending-clear tracking, attachment reuse tracking (`attach_unsynced`), and explicit pass-to-sampler barriers.
- The optional `mali_barriers` path narrows destination stages and releases render targets at pass end. It is OFF by default. Do not enable it blindly while investigating flicker; compare it as a controlled A/B test only after a stable baseline exists.
- `RuntimeConfig::render_pass_objects` is OFF by default. Its source comment explicitly describes it as a diagnostic fallback for drivers whose dynamic rendering misbehaves, including Mali flicker reports. This is the first high-value compatibility experiment.
- The backend already has useful telemetry: slow command-list duration, recorder wait, GPU fence wait, pipeline compile time, draw/pass/barrier counts, texture uploads, GPU timestamp samples and GPU-busy summaries. Prefer extracting these existing measurements before adding duplicate instrumentation.
- GPU timestamp queries are conditional on `timestampComputeAndGraphics`; absence of GPU timing must be reported as unavailable, not inferred as zero.

## Controlled diagnostic matrix

Keep the same game area, camera position, resolution, FPS cap and device thermal state for each run. Restart the app when changing settings marked as next-start.

1. Baseline: system Vulkan driver, `present_mode=fifo`, `mali_barriers=0`, `render_pass_objects=0`.
2. Render-pass fallback: same settings, only `render_pass_objects=1`.
3. Barrier experiment: start from whichever of runs 1–2 is visually stable; change only `mali_barriers=1`.
4. PanVK comparison: repeat only after a stable system-driver baseline; do not assume PanVK is a performance upgrade on Mali.

Record whether black flashes occur in menus, exploration, transitions, or boss scenes. Record average and worst FPS, slow-list logs, GPU-busy / timestamp availability, and whether the visual defect changed.

## Conservative initial profile for the next device test

- Driver: system.
- FPS cap: 30 (stable pacing baseline; not a claim that the device sustains 30).
- Present mode: FIFO.
- Performance mode: enabled (the current menu applies 0.75 render scale and lower-cost effects; on devices without BC sampling it also lowers the texture quality to 50%).
- `render_pass_objects=1` for the diagnostic build/test only.
- `mali_barriers=0` until the render-pass fallback has been tested independently.

The settings above are diagnostic controls, not a permanent default. Restore the original settings after each A/B run.

## Next engineering tasks

1. Add a supported, user-visible diagnostic control for `render_pass_objects` (next-start) or a Mali-only safe default after vendor/device identification is reliable. Do not expose a nonfunctional switch.
2. Add explicit log lines at Vulkan initialization that report selected dynamic-rendering vs render-pass path, `mali_barriers`, present mode, swapchain format/extent, timestamp support, and GPU/driver identity in one compact block.
3. Audit swapchain image ownership and layout transitions around acquire, submit, present, resize and surface recreation. Validate semaphore reuse and per-slot fences before changing synchronization.
4. Audit render-target layout/access/stage transitions for color, depth/stencil, transfer, sampled-image and storage-image use. Preserve correctness first; narrow barriers only with evidence.
5. Use existing slow-list and GPU timestamp telemetry to classify boss-scene stalls as CPU/recorder, pipeline compilation, upload, or GPU-bound.
6. Only then optimize resolution, effects, shadows, texture uploads and command submission.

## Validation gate

A change is not considered Mali-compatible until the system driver completes repeated menu, exploration, transition and boss-scene tests without new flicker, black frames, missing effects or crashes. Report FPS and frame pacing alongside thermal state; one short peak-FPS reading is insufficient.

No physical Helio G99 test or Vulkan validation-layer run is performed by this source audit.


## Helio G99 / Mali-G57 engine adaptation — next phase

The first device report confirms build `1b72cf0`, system Vulkan driver, render scale 0.65, 60 FPS target, `mali_barriers=1`, `performance_mode=1`, `lighter_post_fx=1`, and `render_pass_objects=0`. The excerpt supplied for this run contains startup/configuration only; it does not include the fatal signal, native backtrace, Vulkan error, or last game zone before the crash. No crash cause can be assigned from that excerpt.

### Engineering order

1. **Crash classification:** collect the complete DSR log and logcat tail around the first fatal event. Classify as native signal/abort, Vulkan device loss, failed resource allocation, shader translation/pipeline failure, or guest-game fault before changing synchronization.
2. **GPU identification:** log vendor/device IDs, pipeline-cache UUID prefix, and the actually selected rendering path. This is now emitted during Vulkan initialization so device profiles can be keyed to hardware/driver identity rather than the marketing name alone.
3. **Mali barrier A/B:** the reported configuration has `mali_barriers=1`. Repeat the same crash-prone zone with only this option changed to 0. This isolates the experimental narrow-destination-stage barrier path from the conservative ALL_COMMANDS destination path. Do not change render scale, FPS, or effects during this comparison.
4. **Render path validation:** the render-pass-object option only overrides dynamic rendering when the driver exposes dynamic rendering. On Vulkan 1.1 / devices reporting dynamic rendering unavailable, the backend already selects render-pass/framebuffer objects automatically; the setting cannot change that path.
5. **Shader translation:** inventory Maxwell opcodes used by the game's actual shader set, record translation failures and pipeline creation results, and add deterministic host tests for each supported instruction family before adding optimization passes. Preserve shader semantics, depth compare, discard, derivatives, and texture sampling.
6. **GPU workload reduction:** profile pass counts, barriers, render-target transitions, texture uploads, pipeline compilation, and GPU-busy time in the same scene. Optimize the measured dominant category, not guessed bottlenecks.
7. **Submission and synchronization:** audit swapchain acquire/present semaphore ownership, slot fences, command recorder handoff, and device-loss recovery. No global barrier weakening or queue restructuring until the crash is reproducible and synchronization ownership is proven.

### Current architectural facts

- Guest Maxwell shaders are translated to SPIR-V by `src/hle/nvn/shader/translator.cpp`; this is the shader translation layer to validate for Mali.
- NVN-like render commands are executed by the Vulkan backend in `src/hle/nvn/vk_backend.cpp`.
- The backend already has a persistent Vulkan pipeline cache, background pipeline compiler threads, four submission slots, GPU timing summaries, upload counters, and barrier counters. These should be measured before replacing them.
- A Vulkan 1.1 device without dynamic rendering already uses render-pass/framebuffer objects. Do not mistake `render_pass_objects=0` for dynamic rendering being active.
- The Mali barrier option narrows destination stages for image transitions; it is experimental and should be isolated when investigating crashes.

A source change is not considered a performance improvement until repeatable on-device measurements show better frame pacing without new crashes, flicker, missing effects, or corrupted rendering.
