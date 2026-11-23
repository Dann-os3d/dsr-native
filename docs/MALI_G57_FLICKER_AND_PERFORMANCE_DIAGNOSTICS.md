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
- Performance mode: enabled (the current menu applies 0.75 render scale and lower-cost effects; on devices without BC sampling it also enables mip skipping).
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
