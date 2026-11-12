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
    // Fire as soon as the last frame is done instead of every 1 / fps (frames are still 1 / fps long to the scene): a
    // capture's clock is frame-exact, so it can run as fast as the machine draws.
    void   SetUnpaced(bool);
    bool   Unpaced() const;
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
    Atomic<bool>            m_unpaced { false };
    Atomic<u64>             m_frametime {};
    Atomic<u64>             m_ideatime {};
    Atomic<u32>             m_frame_busy_count {};
    Instant                 m_clock { Instant::now() };
    ThreadTimer             m_timer;
};

// The most a paced frame steps the scene by: a stall (a drag, a suspended machine) must not make the scene jump.
constexpr double kMaxPacedFrameStep = 0.1;
// A paced frame loop that runs behind its target (a 120 fps target on a scene that draws at 70) steps the scene by the
// wall time that really passed since the last frame, `elapsed` seconds, so animation keeps its speed: never less than
// `target`, never more than the cap (a target slower than the cap is kept as it is), and `target` when `elapsed` is not
// a usable time.
double PacedFrameStep(double target, double elapsed);
} // namespace owe
