#include <rstd/test/gtest.hpp>

import rstd;
import wescene.timer;

using namespace rstd::prelude;
using rstd::sync::atomic::Atomic;
using rstd::time::Duration;
using rstd::time::Instant;
using namespace rstd::literals;

namespace
{
bool WaitFor(const Atomic<u32>& count, u32 target) {
    auto start = Instant::now();
    while (count.load() < target && start.elapsed() < Duration::from_secs(u64(3)))
        rstd::thread::sleep(Duration::from_millis(u64(1)));
    return count.load() >= target;
}
} // namespace

TEST(ThreadTimer, StopsWithoutEmittingAndCanRestart) {
    Atomic<u32>      calls {};
    owe::ThreadTimer timer(owe::ThreadTimer::Callback::make([&] {
        calls.fetch_add(u32(1));
    }));
    EXPECT_FALSE(timer.Running());
    timer.Stop();
    timer.SetInterval(Duration::from_secs(u64(30)));
    auto started = Instant::now();
    timer.Start();
    timer.Start();
    timer.Stop();
    EXPECT_TRUE(started.elapsed() < Duration::from_secs(u64(3)));
    EXPECT_EQ(calls.load(), u32());
    timer.SetInterval(Duration::from_millis(u64(1)));
    timer.Start();
    EXPECT_TRUE(WaitFor(calls, u32(2)));
    timer.Stop();
    auto stopped = calls.load();
    rstd::thread::sleep(Duration::from_millis(u64(10)));
    EXPECT_EQ(calls.load(), stopped);
    EXPECT_FALSE(timer.Running());
}

// Callbacks start one interval apart, whatever they take: a 12 ms callback on a 20 ms interval is 50 a second, not the
// 31 a second of a 20 ms sleep after each 12 ms callback.
TEST(ThreadTimer, StartsCallbacksOneIntervalApartNotOneIntervalAfterTheLastEnds) {
    Atomic<u32>      calls {};
    owe::ThreadTimer timer(owe::ThreadTimer::Callback::make([&] {
        rstd::thread::sleep(Duration::from_millis(u64(12)));
        calls.fetch_add(u32(1));
    }));
    timer.SetInterval(Duration::from_millis(u64(20)));
    auto started = Instant::now();
    timer.Start();
    rstd::thread::sleep(Duration::from_millis(u64(1000)));
    timer.Stop();
    const auto seconds = static_cast<double>(started.elapsed().as_millis().to_primitive()) / 1000.0;
    const auto rate    = static_cast<double>(calls.load().to_primitive()) / seconds;
    EXPECT_GT(rate, 44.0);
    EXPECT_LT(rate, 52.0);
}

TEST(FrameTimer, OwnsMutableCallbackAndPreservesBackpressure) {
    Atomic<u32>     calls {};
    auto            state = Box<u32>::make();
    owe::FrameTimer timer(
        Some(owe::FrameTimer::Callback::make([state = rstd::move(state), &calls]() mutable {
            calls.store(++*state);
        })));
    timer.SetRequiredFps(u16(1000));
    timer.Run();
    ASSERT_TRUE(WaitFor(calls, u32(4)));
    rstd::thread::sleep(Duration::from_millis(u64(10)));
    EXPECT_EQ(calls.load(), u32(4));
    timer.SetCallback(None());
    timer.FrameBegin();
    timer.FrameEnd();
    EXPECT_TRUE(WaitFor(calls, u32(5)));
    timer.Stop();
    timer.SetCallback(None());
    auto stopped = calls.load();
    timer.Run();
    rstd::thread::sleep(Duration::from_millis(u64(10)));
    timer.Stop();
    EXPECT_EQ(calls.load(), stopped);
}

// A capture's clock is frame-exact (each frame advances the scene by the required fps' frame time),
// so it need not wait that long between frames: an unpaced timer fires as soon as the last frame is
// done, and its frame time stays the required fps'.
TEST(FrameTimer, UnpacedRunsAsFastAsFramesFinishButKeepsItsFrameTime) {
    Atomic<u32>     calls {};
    owe::FrameTimer timer;
    timer.SetCallback(Some(owe::FrameTimer::Callback::make([&] {
        timer.FrameBegin();
        calls.fetch_add(u32(1));
        timer.FrameEnd();
    })));
    timer.SetRequiredFps(u16(1));
    timer.SetUnpaced(true);
    EXPECT_TRUE(timer.Unpaced());
    EXPECT_DOUBLE_EQ(timer.TargetFrameTime(), 1.0);
    timer.Run();
    // paced at 1 fps these would take 20 seconds
    EXPECT_TRUE(WaitFor(calls, u32(20)));
    timer.Stop();
    EXPECT_DOUBLE_EQ(timer.TargetFrameTime(), 1.0);
    EXPECT_EQ(timer.RequiredFps(), u16(1));
}

TEST(FrameTimer, SerializesSamplesWithFpsChanges) {
    owe::FrameTimer timer;
    EXPECT_EQ(timer.RequiredFps(), u16(15));
    EXPECT_DOUBLE_EQ(timer.TargetFrameTime(), 1.0 / 15.0);
    timer.SetRequiredFps(u16());
    EXPECT_EQ(timer.RequiredFps(), u16(1));
    EXPECT_DOUBLE_EQ(timer.FrameTime(), 1.0);
    auto writer = rstd::thread::spawn([&] {
                      for (int i = 0; i < 100; ++i) timer.SetRequiredFps(u16(30 + i));
                  }).unwrap();
    for (int i = 0; i < 100; ++i) {
        timer.FrameBegin();
        timer.FrameEnd();
    }
    rstd::move(writer).join().unwrap();
    EXPECT_EQ(timer.RequiredFps(), u16(129));
    EXPECT_TRUE(timer.TargetFrameTime() > 0.0);
}

// The scene steps by exactly 1 / fps (Wallpaper Engine's particle counts match a 1/30 step, not 33333 us: rounded to
// whole microseconds, 4 emissions a second at 30 fps add up to just under 2 by frame 15 and emit a frame late).
TEST(FrameTimer, TargetFrameTimeIsExactlyOneOverTheFrameRate) {
    owe::FrameTimer timer;
    const int rates[] = { 15, 30, 60, 144 };
    for (int fps : rates) {
        timer.SetRequiredFps(u16(static_cast<rstd::uint16_t>(fps)));
        EXPECT_EQ(timer.TargetFrameTime(), 1.0 / static_cast<double>(fps));
    }
}

// --frame-times FILE: a reference's recorded frame times (a JSON array of seconds) replayed instead of 1 / fps, so a
// comparison steps the scene the way Wallpaper Engine happened to (particles-frametime-*).
TEST(RecordedFrameTimes, ParsesAJsonArrayOfSeconds) {
    auto times = owe::ParseFrameTimes("[0.0001, 0.0572, 0.0164]"_str);
    ASSERT_TRUE(times.is_ok());
    ASSERT_EQ(times->len(), usize(3));
    EXPECT_NEAR((*times)[usize(1)].to_primitive(), 0.0572, 1e-12);
}

TEST(RecordedFrameTimes, RejectsWhatIsNotAListOfTimes) {
    EXPECT_TRUE(owe::ParseFrameTimes("{\"a\": 1}"_str).is_err());
    EXPECT_TRUE(owe::ParseFrameTimes("[0.01, \"x\"]"_str).is_err());
    EXPECT_TRUE(owe::ParseFrameTimes("[0.01, -0.02]"_str).is_err());
    EXPECT_TRUE(owe::ParseFrameTimes("not json"_str).is_err());
}

TEST(RecordedFrameTimes, StepsTheListThenTheTargetFrameTime) {
    auto times = owe::ParseFrameTimes("[0.0001, 0.0572]"_str).unwrap();
    EXPECT_NEAR(owe::RecordedFrameStep(times, usize(0), 1.0 / 30.0), 0.0001, 1e-12);
    EXPECT_NEAR(owe::RecordedFrameStep(times, usize(1), 1.0 / 30.0), 0.0572, 1e-12);
    EXPECT_NEAR(owe::RecordedFrameStep(times, usize(2), 1.0 / 30.0), 1.0 / 30.0, 1e-12);
    EXPECT_NEAR(owe::RecordedFrameStep(Vec<f64>::make(), usize(0), 1.0 / 60.0), 1.0 / 60.0, 1e-12);
}
