# Mali-G57 render-object cache profiling

This instrumentation measures the existing compatibility render-pass/framebuffer caches. It does not change render-pass scheduling, image layouts, barriers, or the default driver configuration.

## Log line

Every normal 10-second backend report may include:

`render-object cache (last 10 s): renderpass H hits/M misses, C created, L live; framebuffer H hits/M misses, C created/D destroyed, L live`

- **hits / misses**: lookups in the interval. A miss means a Vulkan object lookup was needed, not necessarily a performance regression.
- **created / destroyed**: successful Vulkan object creation and framebuffer invalidation in the interval.
- **live**: cache entries currently retained when the report is sampled.
- Render-pass keys include color formats/load operations, depth format, stencil aspect hint, and depth load operation. Including the stencil hint prevents reuse of a pass whose stencil load/store behavior differs.
- Framebuffer entries are removed when one of their image views is retired, after the associated GPU submission has completed.

## Device capture protocol

1. Keep the same driver, game settings, render scale, and scene between captures. Do not change `mali_barriers` for this measurement.
2. Capture at least three 10-second report intervals during a repeatable exploration route.
3. Capture another set around a repeatable boss encounter, including the loading/transition period if possible.
4. Record the cache line alongside GPU busy time, recorder busy time, render-pass count, image barriers, resident memory, and any flicker/hang symptoms.
5. Compare miss/create rates and live counts. Repeated misses with low hit rates suggest unstable attachment/view identity; a rising live framebuffer count may indicate delayed retirement or excessive render-target churn. Interpret these together with scene behavior rather than optimizing from one counter alone.

## Scope and limitations

CI verifies compilation and host tests, not Mali GPU performance. These counters are diagnostic only; they do not claim an FPS improvement. PanVK remains experimental on this device based on prior observed flicker and low performance. Keep the system Vulkan driver as the stability reference until device captures demonstrate otherwise.


## Small big.LITTLE CPU compiler policy

On Android, the platform layer reports the detected performance-core count from the same CPU-frequency topology used by the affinity policy. If it detects only one or two performance cores, the Vulkan pipeline compiler pool is capped at two workers (never below two); devices with more performance cores and non-Android hosts retain the previous worker-count policy. This avoids creating extra competing workers on Helio G80/G99-class CPUs. It is a CPU scheduling/load reduction, not an FPS claim; verify shader-compilation stalls and frame pacing on-device.

## Pipeline-cache concurrency safety

The shared Vulkan pipeline cache is accessed by background pipeline compiler threads and by the asynchronous cache-save worker. All calls that access this shared cache are now serialized with one mutex, including the two-step cache-data size/read operation. This follows Vulkan's external-synchronization requirement for host access to a shared pipeline-cache object and prevents compiler/save overlap from racing on the same driver object. Pipeline creation without a cache handle remains unlocked. The trade-off is that cache-backed pipeline creation calls are serialized; stability takes precedence over speculative parallel throughput on the reference Mali/Helio platform.
