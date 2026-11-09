module wescene.timer;
import wescene.json;
import rstd;

using namespace rstd::prelude;
using rstd::time::Duration;
using rstd::time::Instant;
using namespace owe;
using namespace rstd::literals;

FrameTimer::FrameTimer(Option<Callback> callback)
    : m_callback(rstd::move(callback)), m_timer(ThreadTimer::Callback::make([this] {
          const auto measured = m_frametime.load();
          const auto ideal    = m_ideatime.load();
          m_timer.SetInterval(Duration::from_micros(measured > ideal ? measured / u64(2) : ideal));
          auto callback = m_callback.lock().unwrap();
          if (callback->is_some() && m_frame_busy_count.load() <= u32(3)) {
              m_frame_busy_count.fetch_add(u32(1));
              (**callback)->operator()();
          }
      })) {
    SetRequiredFps(u16(15));
}
FrameTimer::~FrameTimer() { Stop(); }

u16    FrameTimer::RequiredFps() const { return m_req_fps.load(); }
double FrameTimer::FrameTime() const {
    return static_cast<double>(m_frametime.load().to_primitive()) / 1'000'000.0;
}
double FrameTimer::TargetFrameTime() const {
    return static_cast<double>(m_ideatime.load().to_primitive()) / 1'000'000.0;
}

void FrameTimer::SetRequiredFps(u16 value) {
    if (value == u16()) value = u16(1);
    auto samples = m_samples.lock().unwrap();
    m_req_fps.store(value);
    auto ideal = u64(1'000'000) / u64(value.to_primitive());
    m_ideatime.store(ideal);
    for (auto& sample : samples->values) sample = ideal;
    samples->next = usize();
    m_frametime.store(ideal);
    m_timer.SetInterval(Duration::from_micros(ideal));
}
void FrameTimer::FrameBegin() { m_clock = Instant::now(); }
void FrameTimer::FrameEnd() {
    {
        auto samples                   = m_samples.lock().unwrap();
        samples->values[samples->next] = rstd::as_cast<u64>(m_clock.elapsed().as_micros());
        samples->next                  = (samples->next + usize(1)) % samples->values.len();
        u64 total {};
        for (auto sample : samples->values) total += sample;
        m_frametime.store(total / u64(5));
    }
    auto expected = m_frame_busy_count.load();
    while (expected > u32() &&
           ! m_frame_busy_count.compare_exchange_weak(expected, expected - u32(1))) {
    }
}
void FrameTimer::SetCallback(Option<Callback> callback) {
    if (Running()) return;
    auto current = m_callback.lock().unwrap();
    rstd::mem::swap(*current, callback);
}
void FrameTimer::Run() { m_timer.Start(); }
void FrameTimer::Stop() { m_timer.Stop(); }
bool FrameTimer::Running() const { return m_timer.Running(); }

auto owe::ParseFrameTimes(ref<str> source) -> Result<Vec<f64>, String> {
    auto parsed = owe::ParseJson(source);
    if (parsed.is_err()) return Err("the frame times are not JSON"_Str);
    auto array = parsed->as_array();
    if (array.is_none()) return Err("the frame times are not a JSON array"_Str);
    Vec<f64> times;
    for (const auto& value : **array) {
        auto seconds = value.as_f64();
        if (seconds.is_none() || seconds->to_primitive() < 0.0)
            return Err("a frame time is not a number of seconds"_Str);
        times.push(f64(seconds->to_primitive()));
    }
    return Ok(rstd::move(times));
}

double owe::RecordedFrameStep(const Vec<f64>& times, usize index, double fallback) {
    return index < times.len() ? times[index].to_primitive() : fallback;
}
