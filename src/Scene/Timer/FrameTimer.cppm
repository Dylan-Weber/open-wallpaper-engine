export module wescene.timer:frame_timer;
import rstd;
import :thread_timer;

using namespace rstd::prelude;
using rstd::sync::Mutex;
using rstd::sync::atomic::Atomic;
using rstd::time::Instant;

export namespace owe
{
class FrameTimer {
public:
    using Callback = Box<dyn<FnMut<void()>>>;
    explicit FrameTimer(Option<Callback> callback = None());
    ~FrameTimer();
    void   SetCallback(Option<Callback>);
    void   Run();
    void   Stop();
    u16    RequiredFps() const;
    bool   Running() const;
    double FrameTime() const;
    double TargetFrameTime() const;
    void   SetRequiredFps(u16);
    void   FrameBegin();
    void   FrameEnd();

private:
    struct Samples {
        array<u64, 5> values {};
        usize         next {};
    };
    Mutex<Option<Callback>> m_callback;
    Mutex<Samples>          m_samples { Samples {} };
    Atomic<u16>             m_req_fps { u16(15) };
    Atomic<u64>             m_frametime {};
    Atomic<u64>             m_ideatime {};
    Atomic<u32>             m_frame_busy_count {};
    Instant                 m_clock { Instant::now() };
    ThreadTimer             m_timer;
};

// A reference's recorded frame times (--frame-times FILE: a JSON array of seconds), replayed instead of 1 / fps so a
// comparison steps the scene the way Wallpaper Engine happened to (particles-frametime-*).
auto ParseFrameTimes(ref<str> source) -> Result<Vec<f64>, String>;
// Frame `index`'s step: the recorded time, or `fallback` (the target frame time) once the list runs out.
double RecordedFrameStep(const Vec<f64>& times, usize index, double fallback);
} // namespace owe
