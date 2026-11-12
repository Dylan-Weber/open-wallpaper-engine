module wescene.timer;
import rstd;

using namespace rstd::prelude;
using rstd::time::Duration;
using namespace owe;

ThreadTimer::ThreadTimer(Callback callback): m_callback(rstd::move(callback)) {}
ThreadTimer::~ThreadTimer() { Stop(); }

bool ThreadTimer::Running() const { return m_state.lock().unwrap()->running; }

void ThreadTimer::SetInterval(Duration interval) {
    auto state      = m_state.lock().unwrap();
    state->interval = interval;
}

void ThreadTimer::Start() {
    auto operation = m_op_mutex.lock().unwrap();
    {
        auto state = m_state.lock().unwrap();
        if (state->running) return;
        state->running = true;
    }
    auto thread = rstd::thread::spawn([this] {
        // Callbacks start one interval apart (start to start), not one interval after the previous one ends: a frame
        // that takes 4 ms of a 16.7 ms interval otherwise makes a 20.7 ms period, and a 120 fps target tops out below 100.
        // `anchor` is when the current callback was due; a callback that overran by more than an interval restarts it.
        auto anchor = rstd::time::Instant::now();
        for (;;) {
            {
                auto state = m_state.lock().unwrap();
                if (! state->running) break;
                for (;;) {
                    const auto interval_us = rstd::as_cast<u64>(state->interval.as_micros());
                    const auto elapsed_us  = rstd::as_cast<u64>(anchor.elapsed().as_micros());
                    if (elapsed_us >= interval_us) {
                        anchor = elapsed_us >= interval_us * u64(2)
                                     ? rstd::time::Instant::now()
                                     : anchor + state->interval;
                        break;
                    }
                    m_condition.wait_timeout_while(
                        state,
                        Duration::from_micros(interval_us - elapsed_us),
                        [](const State& value) { return value.running; });
                    if (! state->running) break;
                }
                if (! state->running) break;
            }
            m_callback->operator()();
        }
    });
    if (thread.is_err()) {
        auto state     = m_state.lock().unwrap();
        state->running = false;
    }
    m_timer_thread = Some(rstd::move(thread).unwrap());
}

void ThreadTimer::Stop() {
    auto operation = m_op_mutex.lock().unwrap();
    if (! m_timer_thread) return;
    if (rstd::thread::current_id() == m_timer_thread->thread().id())
        rstd::panic { "ThreadTimer cannot stop from its callback" };
    {
        auto state     = m_state.lock().unwrap();
        state->running = false;
    }
    m_condition.notify_all();
    rstd::move(m_timer_thread.take().unwrap()).join().unwrap();
}
