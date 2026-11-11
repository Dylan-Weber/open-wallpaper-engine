module;
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

// Per-frame CPU stage timers for performance work. Off unless OWE_PERF_LOG names a file: then every frame's time in
// each stage is kept and, at exit, written as JSON (frame count, and mean / p50 / p95 / p99 / max in milliseconds per
// stage over the frames after the first 10). Stages nest or overlap (Prepare contains Scripts and Anim); each is
// timed on its own. All calls come from the render thread.
export module wescene.timer:perf;

export namespace owe::perf
{
enum class Stage : int {
    Frame,   // a whole onDraw call
    Prepare, // pipeline.prepare: scripts, animations, particles, uniforms
    Scripts, // scene scripts (JS)
    Anim,    // property animations, camera paths, material and transform updaters
    Draw,    // VulkanRender::drawFrame (contains the five below)
    Update,  // SceneRenderState::update: per-frame scene -> render graph inputs (uniforms, draw lists)
    Begin,   // beginFrame: frame object and resource acquisition
    Advance, // frame advance: graph preparation / binding / hazards
    Record,  // command buffer recording
    Submit,  // queue submission
    Capture, // beginFrame: SceneRenderState::captureFrame (frame request: resources, bindings, uniforms)
    GBegin,  // beginFrame: GraphFrame::begin
    Gpu,     // GPU time of the frame's passes (OWE_GPU_TIMING; the sum of the pass timestamps)
    Wait,    // waiting for the frame's completion fence (GPU time not overlapped with CPU work)
    Queued,  // frames still executing on the GPU when a frame is submitted (a count, shown as "ms"): the display latency a deeper queue adds, in frames of GPU time
    Count
};

inline const char* const kStageNames[] = { "frame", "prepare", "scripts", "anim", "draw", "update", "begin", "advance", "record", "submit", "capture", "gbegin", "gpu", "wait", "queued" };

namespace detail
{
inline bool                     g_enabled = getenv("OWE_PERF_LOG") != nullptr;
inline std::vector<double>      g_samples[int(Stage::Count)];
// per pass name: this frame's GPU ms so far, and the per-frame sums of the frames it ran in
inline std::map<std::string, double>              g_gpu_pending;
inline std::map<std::string, std::vector<double>> g_gpu_samples;
inline uint64_t                 g_pending[int(Stage::Count)];
inline bool                     g_registered = false;

inline void Write() {
    const char* path = getenv("OWE_PERF_LOG");
    FILE*       f    = path ? fopen(path, "w") : nullptr;
    if (! f) return;
    fprintf(f, "{\"frames\": %zu, \"stages\": {", g_samples[0].size());
    for (int s = 0; s < int(Stage::Count); ++s) {
        auto v = g_samples[s];
        if (v.size() > 10) v.erase(v.begin(), v.begin() + 10);
        std::sort(v.begin(), v.end());
        double sum = 0;
        for (double x : v) sum += x;
        auto at = [&](double q) { return v.empty() ? 0.0 : v[size_t(q * double(v.size() - 1))]; };
        fprintf(f,
                "%s\"%s\": {\"mean\": %.4f, \"p50\": %.4f, \"p95\": %.4f, \"p99\": %.4f, \"max\": %.4f}",
                s ? ", " : "",
                kStageNames[s],
                v.empty() ? 0.0 : sum / double(v.size()),
                at(0.5),
                at(0.95),
                at(0.99),
                at(1.0));
    }
    fprintf(f, "}, \"gpu_passes\": [");
    const double frames = double(g_samples[0].size() > 10 ? g_samples[0].size() - 10 : 1);
    struct Row {
        std::string name;
        double      per_frame, when_run, p95;
        size_t      runs;
    };
    std::vector<Row> rows;
    for (auto& [name, v] : g_gpu_samples) {
        auto w = v;
        if (w.size() > 10) w.erase(w.begin(), w.begin() + 10);
        std::sort(w.begin(), w.end());
        double sum = 0;
        for (double x : w) sum += x;
        rows.push_back({ name, sum / frames, w.empty() ? 0.0 : sum / double(w.size()),
                         w.empty() ? 0.0 : w[size_t(0.95 * double(w.size() - 1))], w.size() });
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.per_frame > b.per_frame; });
    for (size_t i = 0; i < rows.size(); ++i)
        fprintf(f, "%s{\"name\": \"%s\", \"ms_per_frame\": %.4f, \"ms_when_run\": %.4f, \"p95_when_run\": %.4f, \"frames_run\": %zu}",
                i ? ", " : "", rows[i].name.c_str(), rows[i].per_frame, rows[i].when_run, rows[i].p95, rows[i].runs);
    fprintf(f, "]}\n");
    fclose(f);
}
} // namespace detail

inline bool Enabled() { return detail::g_enabled; }
inline uint64_t Now() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}
// adds a count to the frame's value in `stage` (stored like a time in ms)
inline void AddCount(Stage stage, uint64_t count) {
    if (! detail::g_enabled) return;
    detail::g_pending[int(stage)] += count * 1000000ull;
}
// adds `Now() - start` to the frame's time in `stage` (a stage may run several times a frame)
inline void Add(Stage stage, uint64_t start) {
    if (! detail::g_enabled) return;
    detail::g_pending[int(stage)] += Now() - start;
}
// one pass's GPU time (ms) in the current frame; a pass name may run several times a frame
inline void GpuPass(const std::string& name, double ms) {
    if (! detail::g_enabled) return;
    detail::g_gpu_pending[name] += ms;
    detail::g_pending[int(Stage::Gpu)] += uint64_t(ms * 1e6);
}
// closes the frame: every stage's accumulated time becomes one sample
inline void FrameDone() {
    if (! detail::g_enabled) return;
    if (! detail::g_registered) {
        detail::g_registered = true;
        atexit(detail::Write);
    }
    for (int s = 0; s < int(Stage::Count); ++s) {
        detail::g_samples[s].push_back(double(detail::g_pending[s]) / 1e6);
        detail::g_pending[s] = 0;
    }
    for (auto& [name, ms] : detail::g_gpu_pending) detail::g_gpu_samples[name].push_back(ms);
    detail::g_gpu_pending.clear();
}

// RAII: times its scope into `stage`
struct Scope {
    Stage    stage;
    uint64_t start;
    explicit Scope(Stage s): stage(s), start(detail::g_enabled ? Now() : 0) {}
    ~Scope() { Add(stage, start); }
};
} // namespace owe::perf
